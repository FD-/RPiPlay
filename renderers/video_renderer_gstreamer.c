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

typedef struct video_renderer_gstreamer_s {
    video_renderer_t base;
    GstElement *appsrc, *pipeline, *sink;
    GstBus *bus;
} video_renderer_gstreamer_t;

static const video_renderer_funcs_t video_renderer_gstreamer_funcs;

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

    assert(check_plugins());

    // Begin the video pipeline
    GString *launch = g_string_new("appsrc name=video_source stream-type=0 format=GST_FORMAT_TIME is-live=true !"
                                   "queue ! decodebin ! videoconvert ! ");
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
    g_string_append(launch, "autovideosink name=video_sink sync=false");

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
    r->bus = gst_element_get_bus(r->pipeline);
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

static void video_renderer_gstreamer_flush(video_renderer_t *renderer) {
}

static bool video_renderer_gstreamer_listen(video_renderer_t *renderer) {
    video_renderer_gstreamer_t *r = (video_renderer_gstreamer_t *)renderer;
    GstMessage *msg = NULL;

    /* listen  on the gstreamer pipeline bus for an error or EOS.   */
    /* return true if this occurs, and false if 100 millisecs have */
    /* elapsed with no such event occuring.                         */

    msg = gst_bus_timed_pop_filtered(r->bus, 100 *  GST_MSECOND,
                                     GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    /* parse message */
    if (msg != NULL)  {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &err, &debug_info);
            g_printerr("GStreamer: %s\n", err->message);
            g_clear_error (&err);
            g_free (debug_info);
            break;
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            break;
        default:
            g_printerr("unexpected message\n");
            break;
        }
        gst_message_unref(msg);
        return true;
    }
    return false;
}

static void video_renderer_gstreamer_destroy(video_renderer_t *renderer) {
    video_renderer_gstreamer_t *r = (video_renderer_gstreamer_t *)renderer;
    gst_app_src_end_of_stream(GST_APP_SRC(r->appsrc));
    gst_element_set_state(r->pipeline, GST_STATE_NULL);
    gst_object_unref(r->pipeline);
    if (renderer) {
        free(renderer);
    }
}

static void video_renderer_gstreamer_update_background(video_renderer_t *renderer, int type) {

}

static const video_renderer_funcs_t video_renderer_gstreamer_funcs = {
    .start = video_renderer_gstreamer_start,
    .render_buffer = video_renderer_gstreamer_render_buffer,
    .flush = video_renderer_gstreamer_flush,
    .listen = video_renderer_gstreamer_listen,
    .destroy = video_renderer_gstreamer_destroy,
    .update_background = video_renderer_gstreamer_update_background,
};
