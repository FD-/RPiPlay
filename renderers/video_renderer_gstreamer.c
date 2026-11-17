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
#include <assert.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct video_renderer_gstreamer_s {
    video_renderer_t base;
    GstElement *appsrc, *pipeline, *sink;
} video_renderer_gstreamer_t;

static const video_renderer_funcs_t video_renderer_gstreamer_funcs;

/* Returns true if the named GStreamer element exists in the registry. */
static gboolean element_available(const char *name) {
    GstElementFactory *factory = gst_element_factory_find(name);
    if (factory) {
        gst_object_unref(factory);
        return TRUE;
    }
    return FALSE;
}

/* Pick the H.264 decoder:
 * - RPIPLAY_VDECODER env var forces a specific decoder (e.g. avdec_h264 for
 *   debugging, v4l2h264dec for the Broadcom GPU on Raspberry Pi)
 * - otherwise use the hardware V4L2 decoder if present (Raspberry Pi 3/4/
 *   Zero 2 W with the bcm2835_codec kernel module), else fall back to
 *   decodebin (software decoding, or whatever else GStreamer auto-selects) */
static const char *select_video_decoder(logger_t *logger) {
    const char *forced = getenv("RPIPLAY_VDECODER");
    if (forced && forced[0]) {
        if (element_available(forced)) {
            return forced;
        }
        logger_log(logger, LOGGER_WARNING, "GStreamer element %s not available, ignoring RPIPLAY_VDECODER", forced);
    }
    if (element_available("v4l2h264dec")) {
        return "v4l2h264dec";
    }
    return "decodebin";
}

static const char *select_sink(const char *env_name, const char *fallback, logger_t *logger, const char *kind) {
    const char *forced = getenv(env_name);
    if (forced && forced[0]) {
        if (element_available(forced)) {
            return forced;
        }
        logger_log(logger, LOGGER_WARNING, "GStreamer element %s not available, ignoring %s", forced, env_name);
    }
    if (!element_available(fallback)) {
        logger_log(logger, LOGGER_WARNING, "GStreamer element %s not available, %s pipeline may fail", fallback, kind);
    }
    return fallback;
}

static gboolean check_plugins(void)
{
    int i;
    gboolean ret;
    GstRegistry *registry;
    const gchar *needed[] = {"app", "libav", "playback", "autodetect", "videoparsersbad", NULL};

    registry = gst_registry_get();
    ret = TRUE;
    for (i = 0; i < g_strv_length((gchar **)needed); i++) {
        GstPlugin *plugin;
        plugin = gst_registry_find_plugin(registry, needed[i]);
        if (!plugin) {
            g_print("Required gstreamer plugin '%s' not found\n", needed[i]);
            ret = FALSE;
            continue;
        }
        gst_object_unref(plugin);
    }
    return ret;
}

video_renderer_t *video_renderer_gstreamer_init(logger_t *logger, video_renderer_config_t const *config) {
    video_renderer_gstreamer_t *renderer;
    GError *error = NULL;

    renderer = calloc(1, sizeof(video_renderer_gstreamer_t));
    assert(renderer);

    gst_init(NULL, NULL);

    renderer->base.logger = logger;
    renderer->base.funcs = &video_renderer_gstreamer_funcs;
    renderer->base.type = VIDEO_RENDERER_GSTREAMER;

    if (!check_plugins()) {
        logger_log(logger, LOGGER_ERR, "Missing required GStreamer plugins");
        free(renderer);
        return NULL;
    }

    const char *decoder = select_video_decoder(logger);
    gboolean use_v4l2 = strcmp(decoder, "v4l2h264dec") == 0;
    /* Apple sends the video in BT.709, but the V4L2 Broadcom decoder does
     * not signal the colorimetry correctly; force it to avoid washed out
     * colours. Follows the -bt709 workaround used by UxPlay. */
    const char *bt709_env = getenv("RPIPLAY_BT709");
    gboolean use_bt709 = use_v4l2 && bt709_env && bt709_env[0] && strcmp(bt709_env, "0") != 0;
    const char *videosink = select_sink("RPIPLAY_VIDEOSINK", "autovideosink", logger, "video");

    logger_log(logger, LOGGER_INFO, "GStreamer video decoder: %s%s", decoder, use_bt709 ? " (with bt709 fix)" : "");

    // Begin the video pipeline
    GString *launch = g_string_new("");
    if (use_v4l2) {
        /* Hardware path: parse the byte-stream and decode on the GPU. */
        g_string_append(launch, "appsrc name=video_source caps=\"video/x-h264,stream-format=byte-stream,alignment=au\""
                                " stream-type=0 format=GST_FORMAT_TIME is-live=true !"
                                " queue ! h264parse ! ");
        if (use_bt709) {
            g_string_append(launch, "capssetter caps=\"video/x-h264,colorimetry=bt709\" ! ");
        }
        g_string_append_printf(launch, "%s ! videoconvert ! ", decoder);
    } else {
        g_string_append(launch, "appsrc name=video_source stream-type=0 format=GST_FORMAT_TIME is-live=true !"
                                " queue ! decodebin ! videoconvert ! ");
    }
    // Setup rotation
    if (config->rotation != 0) {
        switch (config->rotation) {
        case 90:
        case -270:
            g_string_append(launch, "videoflip method=clockwise ! ");
            break;
        case -90:
        case 270:
            g_string_append(launch, "videoflip method=counterclockwise ! ");
            break;
        case 180:
        case -180:
            g_string_append(launch, "videoflip method=rotate-180 ! ");
            break;
        default:
            printf("Error: Rotation must be +/- 0,90,180,270\n");
            g_string_free(launch, TRUE);
            free(renderer);
            return NULL;
        }
    }

    // Setup flip
    if (config->flip != FLIP_NONE) {
        switch (config->flip) {
        case FLIP_HORIZONTAL:
            g_string_append(launch, "videoflip method=horizontal-flip ! ");
            break;
        case FLIP_VERTICAL:
            g_string_append(launch, "videoflip method=vertical-flip ! ");
            break;
        case FLIP_BOTH:
            g_string_append(launch, "videoflip method=rotate-180 ! ");
            break;
        case FLIP_NONE:
        default:
            break;
        }
    }

    // Finish the pipeline
    g_string_append_printf(launch, "%s name=video_sink sync=false", videosink);

    renderer->pipeline = gst_parse_launch(launch->str, &error);
    g_assert(renderer->pipeline);
    g_string_free(launch, TRUE);

    renderer->appsrc = gst_bin_get_by_name(GST_BIN(renderer->pipeline), "video_source");
    renderer->sink = gst_bin_get_by_name(GST_BIN(renderer->pipeline), "video_sink");

    return &renderer->base;
}

static void video_renderer_gstreamer_start(video_renderer_t *renderer) {
    video_renderer_gstreamer_t *r = (video_renderer_gstreamer_t *)renderer;
    gst_element_set_state(r->pipeline, GST_STATE_PLAYING);
}

static void video_renderer_gstreamer_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts, int type) {
    video_renderer_gstreamer_t *r = (video_renderer_gstreamer_t *)renderer;
    GstBuffer *buffer;

    assert(data_len != 0);

    buffer = gst_buffer_new_and_alloc(data_len);
    assert(buffer != NULL);
    GST_BUFFER_DTS(buffer) = (GstClockTime)pts;
    gst_buffer_fill(buffer, 0, data, data_len);
    gst_app_src_push_buffer(GST_APP_SRC(r->appsrc), buffer);
}

void video_renderer_gstreamer_flush(video_renderer_t *renderer) {

}

void video_renderer_gstreamer_destroy(video_renderer_t *renderer) {
    video_renderer_gstreamer_t *r = (video_renderer_gstreamer_t *)renderer;
    gst_app_src_end_of_stream(GST_APP_SRC(r->appsrc));
    gst_element_set_state(r->pipeline, GST_STATE_NULL);
    gst_object_unref(r->pipeline);
    if (renderer) {
        free(renderer);
    }
}

void video_renderer_gstreamer_update_background(video_renderer_t *renderer, int type) {

}

static const video_renderer_funcs_t video_renderer_gstreamer_funcs = {
    .start = video_renderer_gstreamer_start,
    .render_buffer = video_renderer_gstreamer_render_buffer,
    .flush = video_renderer_gstreamer_flush,
    .destroy = video_renderer_gstreamer_destroy,
    .update_background = video_renderer_gstreamer_update_background,
};
