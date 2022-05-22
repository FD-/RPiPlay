/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "video_renderer.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "bcm_host.h"
#include "ilclient.h"
#include "../lib/threads.h"
#include "h264-bitstream/h264_stream.h"

/*
 * H264 renderer using OpenMAX for hardware accelerated decoding
 * on the Raspberry Pi.
 * Based on the hello_video sample from the Raspberry Pi project.
*/

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define LAYER_VIDEO 2
#define LAYER_BACKGROUND 1

typedef struct video_renderer_rpi_s {
    video_renderer_t base;
    video_renderer_config_t const *config;

    uint16_t background_visits;
    DISPMANX_ELEMENT_HANDLE_T background_element;

    ILCLIENT_T *client;
    COMPONENT_T *video_decoder;
    COMPONENT_T *video_renderer;
    COMPONENT_T *video_scheduler;
    COMPONENT_T *clock;

    COMPONENT_T *components[5];
    TUNNEL_T tunnels[4];

    uint64_t first_packet_time;
    uint64_t input_frames;
} video_renderer_rpi_t;

static const video_renderer_funcs_t video_renderer_rpi_funcs;

/* From: https://github.com/popcornmix/omxplayer/blob/master/omxplayer.cpp#L455
 * Licensed under the GPLv2 */
static void video_renderer_rpi_render_background(video_renderer_rpi_t *renderer) {
    if (renderer->background_element) {
        return;
    }

    // we create a 1x1 black pixel image that is added to display just behind video
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_UPDATE_HANDLE_T update;
    DISPMANX_RESOURCE_HANDLE_T resource;
    uint32_t vc_image_ptr;
    VC_IMAGE_TYPE_T type = VC_IMAGE_RGB565;
    uint16_t image = 0x0000; // black

    VC_RECT_T dst_rect, src_rect;

    display = vc_dispmanx_display_open(0);

    resource = vc_dispmanx_resource_create(type, 1 /*width*/, 1 /*height*/, &vc_image_ptr);

    vc_dispmanx_rect_set(&dst_rect, 0, 0, 1, 1);

    vc_dispmanx_resource_write_data(resource, type, sizeof(image), &image, &dst_rect);

    vc_dispmanx_rect_set(&src_rect, 0, 0, 1 << 16, 1 << 16);
    vc_dispmanx_rect_set(&dst_rect, 0, 0, 0, 0);

    update = vc_dispmanx_update_start(0);

    renderer->background_element = vc_dispmanx_element_add(update, display, LAYER_BACKGROUND, &dst_rect, resource,
                                                           &src_rect,
                                                           DISPMANX_PROTECTION_NONE, NULL, NULL,
                                                           DISPMANX_STEREOSCOPIC_MONO);

    vc_dispmanx_update_submit_sync(update);
}

static void video_renderer_rpi_remove_background(video_renderer_rpi_t *renderer) {
    if (renderer->background_element) {
        DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
        vc_dispmanx_element_remove(update, renderer->background_element);
        vc_dispmanx_update_submit_sync(update);
    }
    renderer->background_element = 0;
}

static void video_renderer_rpi_update_background(video_renderer_t *renderer, int type) {
    video_renderer_rpi_t *r = (video_renderer_rpi_t *)renderer;
    if (type < 0) {
        r->background_visits--;
    } else if (type > 0) {
        r->background_visits++;
    }
    if (r->background_visits < 0) {
        r->background_visits = 0;
    }

    if (r->config->background_mode == BACKGROUND_MODE_ON) {
        video_renderer_rpi_render_background(r);
    } else if (r->config->background_mode == BACKGROUND_MODE_AUTO) {
        // Show background when connection is made and hide background when all connections are gone
        if (r->background_visits > 0) {
            video_renderer_rpi_render_background(r);
        } else {
            video_renderer_rpi_remove_background(r);
        }
    }
}

static void video_renderer_rpi_destroy_decoder(video_renderer_rpi_t *renderer) {
    ilclient_disable_tunnel(&renderer->tunnels[0]);
    ilclient_disable_tunnel(&renderer->tunnels[1]);
    ilclient_disable_tunnel(&renderer->tunnels[2]);
    ilclient_disable_port_buffers(renderer->video_decoder, 130, NULL, NULL, NULL);
    ilclient_teardown_tunnels(renderer->tunnels);

    ilclient_state_transition(renderer->components, OMX_StateIdle);
    ilclient_state_transition(renderer->components, OMX_StateLoaded);
    ilclient_cleanup_components(renderer->components);

    OMX_Deinit();
    ilclient_destroy(renderer->client);
}

static void omx_event_handler(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
    video_renderer_rpi_t *renderer = (video_renderer_rpi_t *) userdata;
    logger_log(renderer->base.logger, LOGGER_DEBUG, "Video renderer config change: %p: %d", comp, data);
}

static int video_renderer_rpi_init_decoder(video_renderer_rpi_t *renderer) {
    memset(renderer->components, 0, sizeof(renderer->components));
    memset(renderer->tunnels, 0, sizeof(renderer->tunnels));

    bcm_host_init();

    video_renderer_rpi_update_background(&renderer->base, 0);

    if ((renderer->client = ilclient_init()) == NULL) {
        return -3;
    }

    if (OMX_Init() != OMX_ErrorNone) {
        ilclient_destroy(renderer->client);
        return -4;
    }

    // Create video_decode
    if (ilclient_create_component(renderer->client, &renderer->video_decoder, "video_decode",
                                  ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -14;
    }
    renderer->components[0] = renderer->video_decoder;

    // Create video_renderer
    if (ilclient_create_component(renderer->client, &renderer->video_renderer, "video_render",
                                  ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -14;
    }
    renderer->components[1] = renderer->video_renderer;

    // Register to video stalls
    OMX_CONFIG_REQUESTCALLBACKTYPE request_callback;
    memset(&request_callback, 0, sizeof(OMX_CONFIG_REQUESTCALLBACKTYPE));
    request_callback.nSize = sizeof(OMX_CONFIG_REQUESTCALLBACKTYPE);
    request_callback.nVersion.nVersion = OMX_VERSION;
    request_callback.nPortIndex = 131;
    request_callback.nIndex = OMX_IndexConfigBufferStall;
    request_callback.bEnable = OMX_TRUE;
    if (OMX_SetConfig(ilclient_get_handle(renderer->video_decoder), OMX_IndexConfigRequestCallback,
                      &request_callback) != OMX_ErrorNone) {
        logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not request video stall callback");
        return -14;
    }
    ilclient_set_configchanged_callback(renderer->client, omx_event_handler, renderer);

    // Create clock
    if (ilclient_create_component(renderer->client, &renderer->clock, "clock",
                                  ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -14;
    }
    renderer->components[2] = renderer->clock;

    // Set the reference clock to the video clock
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE active_ref_clock;
    memset(&active_ref_clock, 0, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
    active_ref_clock.nSize = sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE);
    active_ref_clock.nVersion.nVersion = OMX_VERSION;
    active_ref_clock.eClock = OMX_TIME_RefClockVideo;
    if (OMX_SetConfig(ilclient_get_handle(renderer->clock), OMX_IndexConfigTimeActiveRefClock,
                      &active_ref_clock) != OMX_ErrorNone) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -13;
    }

    // Setup clock
    OMX_TIME_CONFIG_CLOCKSTATETYPE clock_state;
    memset(&clock_state, 0, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
    clock_state.nSize = sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE);
    clock_state.nVersion.nVersion = OMX_VERSION;
    clock_state.eState = OMX_TIME_ClockStateWaitingForStartTime;
    clock_state.nWaitMask = 1;
    if (OMX_SetParameter(ilclient_get_handle(renderer->clock), OMX_IndexConfigTimeClockState,
                         &clock_state) != OMX_ErrorNone) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -13;
    }

    // Create video_scheduler
    if (ilclient_create_component(renderer->client, &renderer->video_scheduler, "video_scheduler",
                                  ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -14;
    }
    renderer->components[3] = renderer->video_scheduler;

    // Create tunnels
    set_tunnel(&renderer->tunnels[0], renderer->video_decoder, 131, renderer->video_scheduler, 10);
    set_tunnel(&renderer->tunnels[1], renderer->video_scheduler, 11, renderer->video_renderer, 90);
    set_tunnel(&renderer->tunnels[2], renderer->clock, 80, renderer->video_scheduler, 12);

    // Setup renderer
    OMX_CONFIG_DISPLAYREGIONTYPE display_region;
    memset(&display_region, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
    display_region.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    display_region.nVersion.nVersion = OMX_VERSION;
    display_region.nPortIndex = 90;
    display_region.set = OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_LAYER;
    display_region.fullscreen = OMX_TRUE;
    display_region.layer = LAYER_VIDEO;

    if (OMX_SetConfig(ilclient_get_handle(renderer->video_renderer), OMX_IndexConfigDisplayRegion,
                      &display_region) != OMX_ErrorNone) {
        logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not set renderer to fullscreen");
        video_renderer_rpi_destroy_decoder(renderer);
        return -13;
    }

    // Setup clock tunnel
    if (ilclient_setup_tunnel(&renderer->tunnels[2], 0, 0) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -15;
    }

    // Setup rotation
    if (renderer->config->rotation != 0) {
        int rotation = renderer->config->rotation;
        OMX_CONFIG_ROTATIONTYPE omx_rotation;
        memset(&omx_rotation, 0, sizeof(OMX_CONFIG_ROTATIONTYPE));
        omx_rotation.nSize = sizeof(OMX_CONFIG_ROTATIONTYPE);
        // Check the rotation here
        if (rotation != 90 && rotation != -90 && rotation != 180 && rotation != -180 && rotation != 270 && rotation != -270) {
            printf("Error: Rotation must be +/- 0,90,180,270\n");
            video_renderer_rpi_destroy_decoder(renderer);
            return -15;
        }
        omx_rotation.nRotation = rotation;
        omx_rotation.nPortIndex = 90;
        omx_rotation.nVersion.nVersion = OMX_VERSION;
        OMX_ERRORTYPE error = OMX_SetConfig(ilclient_get_handle(renderer->video_renderer), OMX_IndexConfigCommonRotate,
                                            &omx_rotation);
        if (error != OMX_ErrorNone) {
            printf("Error: %x\n", error);
            video_renderer_rpi_destroy_decoder(renderer);
            return -15;
        }
    }

    // Setup flipping
    if (renderer->config->flip != FLIP_NONE) {
        OMX_CONFIG_MIRRORTYPE omx_mirror;
        memset(&omx_mirror, 0, sizeof(OMX_CONFIG_MIRRORTYPE));
        omx_mirror.nSize = sizeof(OMX_CONFIG_MIRRORTYPE);
        switch (renderer->config->flip) {
        case FLIP_HORIZONTAL:
            omx_mirror.eMirror = OMX_MirrorHorizontal;
            break;
        case FLIP_VERTICAL:
            omx_mirror.eMirror = OMX_MirrorVertical;
            break;
        case FLIP_BOTH:
            omx_mirror.eMirror = OMX_MirrorBoth;
            break;
        default:
            omx_mirror.eMirror = OMX_MirrorNone;
            break;
        }
        omx_mirror.nPortIndex = 90;
        omx_mirror.nVersion.nVersion = OMX_VERSION;
        OMX_ERRORTYPE error = OMX_SetConfig(ilclient_get_handle(renderer->video_renderer), OMX_IndexConfigCommonMirror,
                                            &omx_mirror);
        if (error != OMX_ErrorNone) {
            printf("Error: %x\n", error);
            video_renderer_rpi_destroy_decoder(renderer);
            return -15;
        }
    }

    // Set decoder format
    ilclient_change_component_state(renderer->video_decoder, OMX_StateIdle);
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
    format.nVersion.nVersion = OMX_VERSION;
    format.nPortIndex = 130;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC;

    if (OMX_SetParameter(ilclient_get_handle(renderer->video_decoder), OMX_IndexParamVideoPortFormat,
                         &format) != OMX_ErrorNone ||
        ilclient_enable_port_buffers(renderer->video_decoder, 130, NULL, NULL, NULL) != 0) {
        video_renderer_rpi_destroy_decoder(renderer);
        return -15;
    }

    // Components are started in video_renderer_start()

    return 1;
}

video_renderer_t *video_renderer_rpi_init(logger_t *logger, video_renderer_config_t const *config) {
    video_renderer_rpi_t *renderer;
    renderer = calloc(1, sizeof(video_renderer_rpi_t));
    if (!renderer) {
        return NULL;
    }

    renderer->base.logger = logger;
    renderer->base.funcs = &video_renderer_rpi_funcs;
    renderer->base.type = VIDEO_RENDERER_RPI;
    renderer->config = config;

    renderer->first_packet_time = 0;
    renderer->input_frames = 0;

    if (video_renderer_rpi_init_decoder(renderer) != 1) {
        free(renderer);
        renderer = NULL;
    }

    return &renderer->base;
}

// Not static because the audio renderer may need to refer to it
ILCLIENT_T *video_renderer_rpi_get_ilclient(video_renderer_rpi_t *renderer) {
    return renderer->client;
}

// Not static because the audio renderer may need to refer to it
COMPONENT_T *video_renderer_rpi_get_clock(video_renderer_rpi_t *renderer) {
    return renderer->clock;
}

static void video_renderer_rpi_start(video_renderer_t *renderer) {
    video_renderer_rpi_t *r = (video_renderer_rpi_t *)renderer;
    ilclient_change_component_state(r->clock, OMX_StateExecuting);
    ilclient_change_component_state(r->video_decoder, OMX_StateExecuting);
}

static void video_renderer_rpi_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len,
                                  uint64_t pts, int type) {
    if (data_len == 0) return;

    video_renderer_rpi_t *r = (video_renderer_rpi_t *)renderer;

    logger_log(renderer->logger, LOGGER_DEBUG, "Got h264 data of %d bytes", data_len);
    r->input_frames++;

    uint8_t *modified_data = NULL;

    if (type == 0) {
        // This reduces the Raspberry Pi H264 decode pipeline delay from about 11 to 6 frames for RPiPlay.
        // Described at https://www.raspberrypi.org/forums/viewtopic.php?t=41053
        logger_log(renderer->logger, LOGGER_DEBUG, "Injecting max_dec_frame_buffering");
        int sps_start, sps_end;
        int sps_size = find_nal_unit(data, data_len, &sps_start, &sps_end);
        if (sps_size > 0) {
            const int sps_wiggle_room = 12;
            const unsigned char nal_marker[] = { 0x0, 0x0, 0x0, 0x1 };
            int modified_data_len = data_len + sps_wiggle_room + sizeof(nal_marker);
            modified_data = malloc(modified_data_len);

            h264_stream_t *h = h264_new();
            h->nal->nal_unit_type = NAL_UNIT_TYPE_SPS;
            h->sps->vui.bitstream_restriction_flag = 1;
            h->sps->vui.max_dec_frame_buffering = 4;

            int new_sps_size = write_nal_unit(h, modified_data + sps_start, sps_wiggle_room);
            if (new_sps_size > 0 && new_sps_size <= sps_wiggle_room) {
                memcpy(modified_data, data, sps_start);
                memcpy(modified_data + sps_start + new_sps_size, nal_marker, sizeof(nal_marker));
                memcpy(modified_data + sps_start + new_sps_size + sizeof(nal_marker), data + sps_start, data_len - sps_start);
                data = modified_data;
                data_len = data_len + new_sps_size + sizeof(nal_marker);
            } else {
                free(modified_data);
                modified_data = NULL;
            }
            h264_free(h);
        } else {
            logger_log(renderer->logger, LOGGER_ERR, "Could not find sps boundaries");
        }
    }

    if (ilclient_remove_event(r->video_decoder, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Port settings changed!!");

        uint64_t time_diff = raop_ntp_get_local_time(ntp) - r->first_packet_time;
        logger_log(renderer->logger, LOGGER_DEBUG, "Video pipeline delay is %llu frames or %llu us",
                   r->input_frames, time_diff);

        if (ilclient_setup_tunnel(&r->tunnels[0], 0, 0) != 0) {
            logger_log(renderer->logger, LOGGER_ERR, "Could not setup decoder tunnel");
        }

        ilclient_change_component_state(r->video_scheduler, OMX_StateExecuting);

        if (ilclient_setup_tunnel(&r->tunnels[1], 0, 1000) != 0) {
            logger_log(renderer->logger, LOGGER_ERR, "Could not setup scheduler tunnel");
        }

        ilclient_change_component_state(r->video_renderer, OMX_StateExecuting);
    }

    int offset = 0;
    while (offset < data_len) {
        OMX_BUFFERHEADERTYPE *buffer = ilclient_get_input_buffer(r->video_decoder, 130, 1);
        if (buffer == NULL) logger_log(renderer->logger, LOGGER_ERR, "Got NULL buffer!");
        if (!buffer)
            exit(-1);
            //break;

        int64_t video_delay = ((int64_t) raop_ntp_get_local_time(ntp)) - ((int64_t) pts);
        logger_log(renderer->logger, LOGGER_DEBUG, "Video delay is %lld", video_delay);
        if (video_delay > 100000)
            r->first_packet_time = 0;

        int chunk_size = MIN(data_len - offset, buffer->nAllocLen);
        memcpy(buffer->pBuffer, data + offset, chunk_size);

        offset += chunk_size;

        buffer->nFilledLen = chunk_size;
        buffer->nOffset = 0;

        if (!r->config->low_latency) buffer->nTimeStamp = ilclient_ticks_from_s64(pts);
        if (r->first_packet_time == 0) {
            buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;
            r->first_packet_time = raop_ntp_get_local_time(ntp);
            if (!r->config->low_latency) buffer->nTimeStamp = ilclient_ticks_from_s64(r->first_packet_time);
        }

        // Mark the last buffer if we had to split the data (probably not necessary)
        if (chunk_size < data_len && offset == data_len) {
            buffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        }

        if (OMX_EmptyThisBuffer(ilclient_get_handle(r->video_decoder), buffer) != OMX_ErrorNone) {
            logger_log(renderer->logger, LOGGER_ERR, "Video decoder refused processing buffer");
        }

    }

    if (modified_data) {
        // We overwrote the data buffer to inject the max_dec_frame_buffering before,
        // so we need to free the data buffer here
        free(modified_data);
    }
}

static void video_renderer_rpi_flush(video_renderer_t *renderer) {
    video_renderer_rpi_t *r = (video_renderer_rpi_t *)renderer;
    OMX_BUFFERHEADERTYPE *buffer = ilclient_get_input_buffer(r->video_decoder, 130, 1);
    if (buffer == NULL) logger_log(renderer->logger, LOGGER_ERR, "Got NULL buffer while flushing!");
    if (!buffer)
        return;

    buffer->nFilledLen = 0;
    buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
    if (OMX_EmptyThisBuffer(ilclient_get_handle(r->video_decoder), buffer) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_ERR, "Video decoder refused processing buffer while flushing!");
    }

    // Wait until EOS reaches renderer
    ilclient_wait_for_event(r->video_renderer, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
                            ILCLIENT_BUFFER_FLAG_EOS, -1);
    ilclient_flush_tunnels(r->tunnels, 0);

    r->first_packet_time = 0;
}

static void video_renderer_rpi_destroy(video_renderer_t *renderer) {
    video_renderer_rpi_t *r = (video_renderer_rpi_t *)renderer;
    if (renderer) {
        // Only flush if data was sent through, gets stuck otherwise
        if (r->first_packet_time) video_renderer_rpi_flush(renderer);
        video_renderer_rpi_destroy_decoder(r);
        free(renderer);
    }
}

static const video_renderer_funcs_t video_renderer_rpi_funcs = {
    .start = video_renderer_rpi_start,
    .render_buffer = video_renderer_rpi_render_buffer,
    .flush = video_renderer_rpi_flush,
    .destroy = video_renderer_rpi_destroy,
    .update_background = video_renderer_rpi_update_background,
};
