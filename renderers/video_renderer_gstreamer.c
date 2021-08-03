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

#ifdef X_DISPLAY_FIX
#include "x_display_fix.h"
#endif

struct video_renderer_s {
    logger_t *logger;
    GstElement *appsrc, *pipeline, *sink;
};

static gboolean check_plugins (void)
{
    int i;
    gboolean ret;
    GstRegistry *registry;
    const gchar *needed[] = { "app", "libav", "playback", "autodetect", NULL};

    registry = gst_registry_get ();
    ret = TRUE;
    for (i = 0; i < g_strv_length ((gchar **) needed); i++) {
        GstPlugin *plugin;
        plugin = gst_registry_find_plugin (registry, needed[i]);
        if (!plugin) {
            g_print ("Required gstreamer plugin '%s' not found\n", needed[i]);
            ret = FALSE;
            continue;
        }
        gst_object_unref (plugin);
    }
    return ret;
}

video_renderer_t *video_renderer_init(logger_t *logger, const char *server_name, videoflip_t videoflip) {
    video_renderer_t *renderer;
    GError *error = NULL;

#ifdef X_DISPLAY_FIX
    get_x_display_root();
    g_set_application_name(server_name);
#endif

    renderer = calloc(1, sizeof(video_renderer_t));
    assert(renderer);

    gst_init(NULL, NULL);

    renderer->logger = logger;
     
    assert(check_plugins ());

    GString *launch = g_string_new("appsrc name=video_source stream-type=0 format=GST_FORMAT_TIME is-live=true !"
                     "queue ! decodebin ! videoconvert ! ");

    /* image transform */
    switch (videoflip) {
    case LEFT:
        g_string_append(launch, "videoflip method=counterclockwise ! ");
       break;
    case RIGHT: 
        g_string_append(launch, "videoflip method=clockwise ! ");
        break;
    case INVERT:
        g_string_append(launch, "videoflip method=rotate-180 ! ");
	break;
    case HFLIP:
        g_string_append(launch, "videoflip method=horizontal-flip ! ");
        break;
    case VFLIP:
        g_string_append(launch, "videoflip method=vertical-flip ! ");
    case NONE:
    break;
    }
    
    g_string_append(launch, "autovideosink name=video_sink sync=false");
    renderer->pipeline = gst_parse_launch(launch->str,  &error);
    g_assert (renderer->pipeline);

    renderer->appsrc = gst_bin_get_by_name (GST_BIN (renderer->pipeline), "video_source");
    renderer->sink = gst_bin_get_by_name (GST_BIN (renderer->pipeline), "video_sink");

    return renderer;
}

void video_renderer_start(video_renderer_t *renderer) {
    //g_signal_connect( renderer->pipeline, "deep-notify", G_CALLBACK(gst_object_default_deep_notify ), NULL );
    gst_element_set_state (renderer->pipeline, GST_STATE_PLAYING);
}

void video_renderer_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char* data, int data_len, uint64_t pts, int type) {
    GstBuffer *buffer;

    assert(data_len != 0);

    buffer = gst_buffer_new_and_alloc(data_len);
    assert(buffer != NULL);
    GST_BUFFER_DTS(buffer) = (GstClockTime)pts;
    gst_buffer_fill(buffer, 0, data, data_len);
    gst_app_src_push_buffer (GST_APP_SRC(renderer->appsrc), buffer);

#ifdef X_DISPLAY_FIX
    fix_x_window_name();
#endif
}

void video_renderer_flush(video_renderer_t *renderer) {

}

void video_renderer_destroy(video_renderer_t *renderer) {
    gst_app_src_end_of_stream (GST_APP_SRC(renderer->appsrc));
    gst_element_set_state (renderer->pipeline, GST_STATE_NULL);
    gst_object_unref (renderer->pipeline);
    if (renderer) {
        free(renderer);
    }
}

/* not implemented for gstreamer */
void video_renderer_update_background(video_renderer_t *renderer, int type) {
}
