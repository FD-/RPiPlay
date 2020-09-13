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

#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "../lib/logger.h"
#include "../lib/raop_ntp.h"
#include "video_renderer.h"

typedef enum audio_device_e { AUDIO_DEVICE_HDMI, AUDIO_DEVICE_ANALOG, AUDIO_DEVICE_NONE } audio_device_t;

typedef enum audio_renderer_type_e {
    AUDIO_RENDERER_DUMMY,
    AUDIO_RENDERER_RPI,
    AUDIO_RENDERER_GSTREAMER
} audio_renderer_type_t;

typedef struct audio_renderer_config_s {
    audio_device_t device;
    bool low_latency;
} audio_renderer_config_t;

typedef struct audio_renderer_s audio_renderer_t;

typedef struct audio_renderer_funcs_s {
    void (*start)(audio_renderer_t *renderer);
    void (*render_buffer)(audio_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts);
    void (*set_volume)(audio_renderer_t *renderer, float volume);
    void (*flush)(audio_renderer_t *renderer);
    void (*destroy)(audio_renderer_t *renderer);
} audio_renderer_funcs_t;

typedef struct audio_renderer_s {
    audio_renderer_funcs_t const *funcs;
    logger_t *logger;
    audio_renderer_type_t type;
} audio_renderer_t;

audio_renderer_t *audio_renderer_dummy_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config);
audio_renderer_t *audio_renderer_rpi_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config);
audio_renderer_t *audio_renderer_gstreamer_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config);

#ifdef __cplusplus
}
#endif

#endif //AUDIO_RENDERER_H
