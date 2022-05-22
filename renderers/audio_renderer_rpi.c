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

/* 
 * AAC renderer using fdk-aac for decoding and OpenMAX for rendering
*/

#include "audio_renderer.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "fdk-aac/libAACdec/include/aacdecoder_lib.h"

#include "bcm_host.h"
#include "ilclient.h"
#include "../lib/threads.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

extern ILCLIENT_T *video_renderer_rpi_get_ilclient(video_renderer_t *renderer);
extern COMPONENT_T *video_renderer_rpi_get_clock(video_renderer_t *renderer);

typedef struct audio_renderer_rpi_s {
    audio_renderer_t base;
    video_renderer_t *video_renderer;
    audio_renderer_config_t const *config;

    HANDLE_AACDECODER audio_decoder;

    ILCLIENT_T *client;
    COMPONENT_T *audio_renderer;
    COMPONENT_T *clock; // Owned by the video renderer if one is used, so don't destroy!

    COMPONENT_T *components[3];
    TUNNEL_T tunnels[2];

    uint64_t first_packet_time;
    uint64_t last_packet_time;
    uint64_t input_frames;
} audio_renderer_rpi_t;

static const audio_renderer_funcs_t audio_renderer_rpi_funcs;

static void audio_renderer_rpi_destroy_decoder(audio_renderer_rpi_t *renderer) {
    aacDecoder_Close(renderer->audio_decoder);
}

static int audio_renderer_rpi_init_decoder(audio_renderer_rpi_t *renderer) {
    int ret = 0;
    renderer->audio_decoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (renderer->audio_decoder == NULL) {
        logger_log(renderer->base.logger, LOGGER_ERR, "aacDecoder open faild!");
        return -1;
    }
    /* ASC config binary data */
    UCHAR eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
    UCHAR *conf[] = { eld_conf };
    static UINT conf_len = sizeof(eld_conf);
    ret = aacDecoder_ConfigRaw(renderer->audio_decoder, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Unable to set configRaw");
        return -2;
    }
    CStreamInfo *aac_stream_info = aacDecoder_GetStreamInfo(renderer->audio_decoder);
    if (aac_stream_info == NULL) {
        logger_log(renderer->base.logger, LOGGER_ERR, "aacDecoder_GetStreamInfo failed!");
        return -3;
    }

    logger_log(renderer->base.logger, LOGGER_DEBUG, "> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",   \
            aac_stream_info->channelConfig, aac_stream_info->aacSampleRate,
            aac_stream_info->aacSamplesPerFrame, aac_stream_info->aot, aac_stream_info->bitRate);
    return 1;
}

static void audio_renderer_rpi_destroy_renderer(audio_renderer_rpi_t *renderer) {
    ilclient_disable_tunnel(&renderer->tunnels[0]);
    ilclient_disable_port_buffers(renderer->audio_renderer, 100, NULL, NULL, NULL);
    ilclient_teardown_tunnels(renderer->tunnels);

    ilclient_state_transition(renderer->components, OMX_StateIdle);
    ilclient_state_transition(renderer->components, OMX_StateLoaded);
    ilclient_cleanup_components(renderer->components);

    if (!renderer->video_renderer) {
        OMX_Deinit();
        ilclient_destroy(renderer->client);
    }
}

static int audio_renderer_rpi_init_renderer(audio_renderer_rpi_t *renderer, video_renderer_t *video_renderer) {
    memset(renderer->components, 0, sizeof(renderer->components));

    if (video_renderer) {
        renderer->client = video_renderer_rpi_get_ilclient(video_renderer);
    } else {
        bcm_host_init();
        if ((renderer->client = ilclient_init()) == NULL) {
            return -3;
        }
        if (OMX_Init() != OMX_ErrorNone) {
            ilclient_destroy(renderer->client);
            return -4;
        }
    }

    // Create audio_renderer
    if (ilclient_create_component(renderer->client, &renderer->audio_renderer, "audio_render",
                                  ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0) {
        audio_renderer_rpi_destroy_renderer(renderer);
        return -14;
    }
    renderer->components[0] = renderer->audio_renderer;

    if (video_renderer) {
        renderer->clock = video_renderer_rpi_get_clock(video_renderer);

        // Tell the audio render component that it's not the clock master
        OMX_CONFIG_BOOLEANTYPE audio_is_clock_source;
        memset(&audio_is_clock_source, 0, sizeof(OMX_CONFIG_BOOLEANTYPE));
        audio_is_clock_source.nSize = sizeof(OMX_CONFIG_BOOLEANTYPE);
        audio_is_clock_source.nVersion.nVersion = OMX_VERSION;
        audio_is_clock_source.bEnabled = OMX_FALSE;
        if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexConfigBrcmClockReferenceSource,
                          &audio_is_clock_source) != OMX_ErrorNone) {
            logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not disable audio render as clock master");
            audio_renderer_rpi_destroy_renderer(renderer);
            return -13;
        }
    } else {
        // Create clock if no video renderer is used
        if (ilclient_create_component(renderer->client, &renderer->clock, "clock",
                                      ILCLIENT_DISABLE_ALL_PORTS) != 0) {
            audio_renderer_rpi_destroy_decoder(renderer);
            return -14;
        }
        renderer->components[1] = renderer->clock;

        // Setup clock
        OMX_TIME_CONFIG_CLOCKSTATETYPE clock_state;
        memset(&clock_state, 0, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
        clock_state.nSize = sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE);
        clock_state.nVersion.nVersion = OMX_VERSION;
        clock_state.eState = OMX_TIME_ClockStateWaitingForStartTime;
        clock_state.nWaitMask = 1;
        if (OMX_SetParameter(ilclient_get_handle(renderer->clock), OMX_IndexConfigTimeClockState,
                             &clock_state) != OMX_ErrorNone) {
            audio_renderer_rpi_destroy_decoder(renderer);
            return -13;
        }
    }

    // Create tunnels
    set_tunnel(&renderer->tunnels[0], renderer->clock, 81, renderer->audio_renderer, 101);

    // Setup clock tunnel
    if (ilclient_setup_tunnel(&renderer->tunnels[0], 0, 0) != 0) {
        audio_renderer_rpi_destroy_decoder(renderer);
        return -15;
    }

    // Setup renderer
    // Use PCM
    OMX_AUDIO_PARAM_PORTFORMATTYPE port_format;
    memset(&port_format, 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    port_format.nSize = sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE);
    port_format.nVersion.nVersion = OMX_VERSION;
    port_format.nPortIndex = 100;
    port_format.eEncoding = OMX_AUDIO_CodingPCM;
    if (OMX_SetParameter(ilclient_get_handle(renderer->audio_renderer), OMX_IndexParamAudioPortFormat,
                         &port_format) != OMX_ErrorNone) {
        logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not set pcm format");
        audio_renderer_rpi_destroy_renderer(renderer);
        return -13;
    }

    // Fill PCM details
    OMX_AUDIO_PARAM_PCMMODETYPE pcm_mode;
    memset(&pcm_mode, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    pcm_mode.nSize = sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
    pcm_mode.nVersion.nVersion = OMX_VERSION;
    pcm_mode.nPortIndex = 100;
    pcm_mode.nChannels = 2;
    pcm_mode.eNumData = OMX_NumericalDataSigned;
    pcm_mode.eEndian = OMX_EndianLittle;
    pcm_mode.nSamplingRate = 44100;
    pcm_mode.bInterleaved = OMX_TRUE;
    pcm_mode.nBitPerSample = 16;
    pcm_mode.ePCMMode = OMX_AUDIO_PCMModeLinear;

    if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexParamAudioPcm,
                      &pcm_mode) != OMX_ErrorNone) {
        logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not set pcm config");
        audio_renderer_rpi_destroy_renderer(renderer);
        return -13;
    }

    // Set audio device
    const char *device_name = renderer->config->device == AUDIO_DEVICE_HDMI ? "hdmi" : "local";
    OMX_CONFIG_BRCMAUDIODESTINATIONTYPE audio_destination;
    memset(&audio_destination, 0, sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE));
    audio_destination.nSize = sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE);
    audio_destination.nVersion.nVersion = OMX_VERSION;
    strcpy((char *)audio_destination.sName, device_name);

    if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexConfigBrcmAudioDestination,
                      &audio_destination) != OMX_ErrorNone) {
        logger_log(renderer->base.logger, LOGGER_DEBUG, "Could not set audio device");
        audio_renderer_rpi_destroy_renderer(renderer);
        return -14;
    }

    // Components are started in audio_renderer_start()

    return 1;
}

audio_renderer_t *audio_renderer_rpi_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config) {
    audio_renderer_rpi_t *renderer;
    renderer = calloc(1, sizeof(audio_renderer_rpi_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &audio_renderer_rpi_funcs;
    renderer->base.type = AUDIO_RENDERER_RPI;
    // Only refer to an existing video renderer if it's an RPI renderer,
    // in which case we have to share resources
    if (video_renderer && video_renderer->type != VIDEO_RENDERER_RPI) {
        video_renderer = NULL;
    }
    renderer->video_renderer = video_renderer;
    renderer->config = config;

    renderer->first_packet_time = 0;
    renderer->input_frames = 0;

    if (audio_renderer_rpi_init_decoder(renderer) != 1) {
        free(renderer);
        renderer = NULL;
    }

    if (audio_renderer_rpi_init_renderer(renderer, video_renderer) != 1) {
        audio_renderer_rpi_destroy_decoder(renderer);
        free(renderer);
        renderer = NULL;
    }

    return &renderer->base;
}

static void audio_renderer_rpi_start(audio_renderer_t *renderer) {
    audio_renderer_rpi_t *r = (audio_renderer_rpi_t *)renderer;
    if (!r->video_renderer) {
        // If no video renderer is used, we're responsible for starting the clock here
        ilclient_change_component_state(r->clock, OMX_StateExecuting);
    }

    ilclient_change_component_state(r->audio_renderer, OMX_StateIdle);
    ilclient_enable_port_buffers(r->audio_renderer, 100, NULL, NULL, NULL);
    ilclient_change_component_state(r->audio_renderer, OMX_StateExecuting);
}

#ifdef DUMP_AUDIO
static FILE* file_pcm = NULL;
#endif

static void audio_renderer_rpi_render_buffer(audio_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts) {
    if (data_len == 0) return;
    
    audio_renderer_rpi_t *r = (audio_renderer_rpi_t *)renderer;

    logger_log(renderer->logger, LOGGER_DEBUG, "Got AAC data of %d bytes", data_len);
    r->input_frames++;

    // We assume that every buffer contains exactly 1 frame.

    AAC_DECODER_ERROR error = 0;

    UCHAR *p_buffer[1] = {data};
    UINT buffer_size = data_len;
    UINT bytes_valid = data_len;
    error = aacDecoder_Fill(r->audio_decoder, p_buffer, &buffer_size, &bytes_valid);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_Fill error : %x", error);
    }

    INT time_data_size = 4 * 480;
    INT_PCM *p_time_data = malloc(time_data_size); // The buffer for the decoded AAC frames
    error = aacDecoder_DecodeFrame(r->audio_decoder, p_time_data, time_data_size, 0);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_DecodeFrame error : 0x%x", error);
    }

#ifdef DUMP_AUDIO
    if (file_pcm == NULL) {
        file_pcm = fopen("/home/pi/Airplay.pcm", "wb");
    }

    fwrite(p_time_data, time_data_size, 1, file_pcm);
#endif

    int offset = 0;
    while (offset < time_data_size) {
        int64_t audio_delay = ((int64_t) raop_ntp_get_local_time(ntp)) - ((int64_t) pts);
        logger_log(renderer->logger, LOGGER_DEBUG, "Audio delay is %lld", audio_delay);
        if (audio_delay > 100000)
            r->first_packet_time = 0;

        OMX_BUFFERHEADERTYPE *buffer = ilclient_get_input_buffer(r->audio_renderer, 100, 1);
        if (!buffer)
            break;

        int chunk_size = MIN(time_data_size - offset, buffer->nAllocLen);
        memcpy(buffer->pBuffer, p_time_data, chunk_size);
        offset += chunk_size;

        buffer->nFilledLen = chunk_size;
        buffer->nOffset = 0;

        if (!r->config->low_latency) buffer->nTimeStamp = ilclient_ticks_from_s64(pts);
        if (r->first_packet_time == 0) {
            buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;
            r->first_packet_time = raop_ntp_get_local_time(ntp);
            if (!r->config->low_latency) buffer->nTimeStamp = ilclient_ticks_from_s64(r->first_packet_time);
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(r->audio_renderer), buffer) != OMX_ErrorNone) {
            logger_log(renderer->logger, LOGGER_ERR, "Audio renderer refused processing buffer");
        }
    }

    free(p_time_data);
}

static void audio_renderer_rpi_set_volume(audio_renderer_t *renderer, float volume) {
    audio_renderer_rpi_t *r = (audio_renderer_rpi_t *)renderer;
    OMX_AUDIO_CONFIG_VOLUMETYPE audio_volume;
    memset(&audio_volume, 0, sizeof(audio_volume));
    audio_volume.nSize = sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE);
    audio_volume.nVersion.nVersion = OMX_VERSION;

    audio_volume.bLinear = OMX_FALSE;
    audio_volume.nPortIndex = 100;
    // Factor 100 for dB -> mB (millibel)
    // It's not clear where the additional factor of 2 comes from,
    // but without it, volume is too high.
    audio_volume.sVolume.nValue = volume * 200.0;

    if (OMX_SetConfig(ilclient_get_handle(r->audio_renderer), OMX_IndexConfigAudioVolume,
                      &audio_volume) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set audio volume");
    }
}

static void audio_renderer_rpi_flush(audio_renderer_t *renderer) {
    // TODO
}

static void audio_renderer_rpi_destroy(audio_renderer_t *renderer) {
    if (renderer) {
        audio_renderer_rpi_t *r = (audio_renderer_rpi_t *)renderer;
        audio_renderer_rpi_flush(renderer);
        audio_renderer_rpi_destroy_decoder(r);
        audio_renderer_rpi_destroy_renderer(r);
        free(renderer);
    }
}

static const audio_renderer_funcs_t audio_renderer_rpi_funcs = {
    .start = audio_renderer_rpi_start,
    .render_buffer = audio_renderer_rpi_render_buffer,
    .set_volume = audio_renderer_rpi_set_volume,
    .flush = audio_renderer_rpi_flush,
    .destroy = audio_renderer_rpi_destroy,
};
