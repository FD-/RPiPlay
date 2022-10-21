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
 * H264 renderer using OpenMAX for hardware accelerated decoding
 * on the Raspberry Pi. 
 * Based on the hello_video sample from the Raspberry Pi project.
*/

#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "../lib/logger.h"
#include "../lib/raop_ntp.h"

typedef enum background_mode_e {
    BACKGROUND_MODE_ON,   // Always show background
    BACKGROUND_MODE_AUTO, // Only show background while there's an active connection
    BACKGROUND_MODE_OFF   // Never show background
} background_mode_t;

typedef enum video_renderer_type_e {
    VIDEO_RENDERER_DUMMY,
    VIDEO_RENDERER_RPI,
    //VIDEO_RENDERER_GSTREAMER
} video_renderer_type_t;

typedef enum flip_mode_e {
    FLIP_NONE,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
    FLIP_BOTH
} flip_mode_t;

typedef struct video_renderer_config_s {
    background_mode_t background_mode;
    bool low_latency;
    int rotation;
    flip_mode_t flip;
} video_renderer_config_t;

typedef struct video_renderer_s video_renderer_t;

typedef struct video_renderer_funcs_s {
    void (*start)(video_renderer_t *renderer);
    void (*render_buffer)(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts, int type);
    void (*flush)(video_renderer_t *renderer);
    void (*destroy)(video_renderer_t *renderer);
    /**
     * Update background according to background mode and connection activity
     * @param renderer
     * @param type visit type.
     *        0: ignore connections
     *        1: a new connection come
     *       -1: a connection lost
     */
    void (*update_background)(video_renderer_t *renderer, int type);
    void (*conn_init)(video_renderer_t *renderer);
    void (*conn_destroy)(video_renderer_t *renderer);

} video_renderer_funcs_t;

typedef struct video_renderer_s {
    video_renderer_funcs_t const *funcs;
    logger_t *logger;
    video_renderer_type_t type;
} video_renderer_t;

video_renderer_t *video_renderer_dummy_init(logger_t *logger, video_renderer_config_t const *config);
video_renderer_t *video_renderer_rpi_init(logger_t *logger, video_renderer_config_t const *config);
video_renderer_t *video_renderer_gstreamer_init(logger_t *logger, video_renderer_config_t const *config);

#ifdef __cplusplus
}
#endif

#endif //VIDEO_RENDERER_H
