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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct audio_renderer_dummy_s {
    audio_renderer_t base;
} audio_renderer_dummy_t;

static const audio_renderer_funcs_t audio_renderer_dummy_funcs;

audio_renderer_t *audio_renderer_dummy_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config) {
    audio_renderer_dummy_t *renderer;
    renderer = calloc(1, sizeof(audio_renderer_dummy_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &audio_renderer_dummy_funcs;
    renderer->base.type = AUDIO_RENDERER_DUMMY;
    return &renderer->base;
}

static void audio_renderer_dummy_start(audio_renderer_t *renderer) {
}

static void audio_renderer_dummy_render_buffer(audio_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts) {
}

static void audio_renderer_dummy_set_volume(audio_renderer_t *renderer, float volume) {
}

static void audio_renderer_dummy_flush(audio_renderer_t *renderer) {
}

static void audio_renderer_dummy_destroy(audio_renderer_t *renderer) {
    if (renderer) {
        free(renderer);
    }
}

static const audio_renderer_funcs_t audio_renderer_dummy_funcs = {
    .start = audio_renderer_dummy_start,
    .render_buffer = audio_renderer_dummy_render_buffer,
    .set_volume = audio_renderer_dummy_set_volume,
    .flush = audio_renderer_dummy_flush,
    .destroy = audio_renderer_dummy_destroy,
};
