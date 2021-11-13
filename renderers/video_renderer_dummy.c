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

typedef struct video_renderer_dummy_s {
    video_renderer_t base;
} video_renderer_dummy_t;

static const video_renderer_funcs_t video_renderer_dummy_funcs;

video_renderer_t *video_renderer_dummy_init(logger_t *logger, video_renderer_config_t const *config) {
    video_renderer_dummy_t *renderer;
    renderer = calloc(1, sizeof(video_renderer_dummy_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &video_renderer_dummy_funcs;
    renderer->base.type = VIDEO_RENDERER_DUMMY;
    return &renderer->base;
}

static void video_renderer_dummy_start(video_renderer_t *renderer) {
}

static void video_renderer_dummy_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts, int type) {
}

static void video_renderer_dummy_flush(video_renderer_t *renderer) {
}

static void video_renderer_dummy_destroy(video_renderer_t *renderer) {
    if (renderer) {
        free(renderer);
    }
}

static void video_renderer_dummy_update_background(video_renderer_t *renderer, int type) {

}

static const video_renderer_funcs_t video_renderer_dummy_funcs = {
    .start = video_renderer_dummy_start,
    .render_buffer = video_renderer_dummy_render_buffer,
    .flush = video_renderer_dummy_flush,
    .destroy = video_renderer_dummy_destroy,
    .update_background = video_renderer_dummy_update_background,
};
