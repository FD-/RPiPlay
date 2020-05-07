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

video_renderer_t *video_renderer_init(logger_t *logger, background_mode_t background_mode, bool low_latency) {
    video_renderer_t *renderer;
    GError *error = NULL;

    renderer = calloc(1, sizeof(video_renderer_t));
    assert(renderer);

    gst_init(NULL, NULL);

    renderer->logger = logger;
    
    assert(check_plugins ());

    renderer->pipeline = gst_parse_launch("appsrc name=video_source stream-type=0 format=GST_FORMAT_TIME is-live=true !"
    "queue ! decodebin ! videoconvert ! autovideosink name=video_sink sync=false", &error);
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

void video_renderer_update_background(video_renderer_t *renderer, int type) {

}
