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

#include "audio_renderer.h"
#include <assert.h>
#include <math.h>
#include <gst/app/gstappsrc.h>

typedef struct audio_renderer_gstreamer_s {
    audio_renderer_t base;
    GstElement *appsrc;
    GstElement *pipeline;
    GstElement *volume;
} audio_renderer_gstreamer_t;

static const audio_renderer_funcs_t audio_renderer_gstreamer_funcs;

static gboolean check_plugins(void)
{
    int i;
    gboolean ret;
    GstRegistry *registry;
    const gchar *needed[] = {"app", "libav", "playback", "autodetect", NULL};

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

audio_renderer_t *audio_renderer_gstreamer_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config) {
    audio_renderer_gstreamer_t *renderer;
    GError *error = NULL;

    renderer = calloc(1, sizeof(audio_renderer_gstreamer_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &audio_renderer_gstreamer_funcs;
    renderer->base.type = AUDIO_RENDERER_GSTREAMER;
    
    // If the video renderer is not a gstreamer renderer, we need to initialize gstreamer
    if (!video_renderer || video_renderer->type != VIDEO_RENDERER_GSTREAMER) {
        gst_init(NULL, NULL);
    }

    assert(check_plugins());

    renderer->pipeline = gst_parse_launch("appsrc name=audio_source stream-type=0 format=GST_FORMAT_TIME is-live=true ! queue ! decodebin !"
    "audioconvert ! volume name=volume ! level ! autoaudiosink sync=false", &error);
    g_assert(renderer->pipeline);

    renderer->appsrc = gst_bin_get_by_name(GST_BIN(renderer->pipeline), "audio_source");
    renderer->volume = gst_bin_get_by_name(GST_BIN(renderer->pipeline), "volume");

    gchar eld_conf[] = {0xF8, 0xE8, 0x50, 0x00};
    GstBuffer *codec_data = gst_buffer_new_and_alloc(sizeof(eld_conf));
    GstMapInfo map;

    gst_buffer_map(codec_data, &map, GST_MAP_WRITE);
    memset(map.data, eld_conf[0], map.size);
    memset(map.data+1, eld_conf[1], map.size);
    memset(map.data+2, eld_conf[2], map.size);
    memset(map.data+3, eld_conf[3], map.size);

    GstCaps *caps = gst_caps_new_simple("audio/mpeg",
        "rate", G_TYPE_INT, 44100,
        "channels", G_TYPE_INT, 2,
        "mpegversion", G_TYPE_INT, 4,
        "stream-format", G_TYPE_STRING, "raw",
        "codec_data", GST_TYPE_BUFFER, codec_data,
        NULL);
    g_object_set(renderer->appsrc, "caps", caps, NULL);

    gst_caps_unref(caps);
    gst_buffer_unmap(codec_data, &map);
    gst_buffer_unref(codec_data);

    return &renderer->base;
}

void audio_renderer_gstreamer_start(audio_renderer_t *renderer) {
    audio_renderer_gstreamer_t *r = (audio_renderer_gstreamer_t *)renderer;
    gst_element_set_state(r->pipeline, GST_STATE_PLAYING);
}

void audio_renderer_gstreamer_render_buffer(audio_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts) {
    GstBuffer *buffer;

    if (data_len == 0) return;
    
    audio_renderer_gstreamer_t *r = (audio_renderer_gstreamer_t *)renderer;

    buffer = gst_buffer_new_and_alloc(data_len);
    assert(buffer != NULL);
    GST_BUFFER_DTS(buffer) = (GstClockTime)pts;
    gst_buffer_fill(buffer, 0, data, data_len);
    gst_app_src_push_buffer(GST_APP_SRC(r->appsrc), buffer);

}

void audio_renderer_gstreamer_set_volume(audio_renderer_t *renderer, float volume) {
    audio_renderer_gstreamer_t *r = (audio_renderer_gstreamer_t *)renderer;
    float avol;
    if (fabs(volume) < 28) {
        avol = floorf(((28-fabs(volume))/28)*10)/10;
        g_object_set(r->volume, "volume", avol, NULL);
    }
}

void audio_renderer_gstreamer_flush(audio_renderer_t *renderer) {
}

void audio_renderer_gstreamer_destroy(audio_renderer_t *renderer) {
    audio_renderer_gstreamer_t *r = (audio_renderer_gstreamer_t *)renderer;
    gst_app_src_end_of_stream(GST_APP_SRC(r->appsrc));
    gst_element_set_state(r->pipeline, GST_STATE_NULL);
    gst_object_unref(r->pipeline);
    gst_object_unref(r->appsrc);
    gst_object_unref(r->volume);
    if (renderer) {
        free(renderer);
    }
}

static const audio_renderer_funcs_t audio_renderer_gstreamer_funcs = {
    .start = audio_renderer_gstreamer_start,
    .render_buffer = audio_renderer_gstreamer_render_buffer,
    .set_volume = audio_renderer_gstreamer_set_volume,
    .flush = audio_renderer_gstreamer_flush,
    .destroy = audio_renderer_gstreamer_destroy,
};
