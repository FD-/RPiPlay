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

#ifndef AIRPLAYSERVER_LOG_H
#define AIRPLAYSERVER_LOG_H

#include <cstdio>
#include <stdarg.h>

#define LEVEL_ERROR 0
#define LEVEL_WARN 1
#define LEVEL_INFO 2
#define LEVEL_DEBUG 3
#define LEVEL_VERBOSE 4

void log(int level, const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vprintf(format, vargs);
    printf("\n");
    va_end(vargs);
}

#define LOGV(...) log(LEVEL_VERBOSE, __VA_ARGS__)
#define LOGD(...) log(LEVEL_DEBUG, __VA_ARGS__)
#define LOGI(...) log(LEVEL_INFO, __VA_ARGS__)
#define LOGW(...) log(LEVEL_WARN, __VA_ARGS__)
#define LOGE(...) log(LEVEL_ERROR, __VA_ARGS__)


#endif //AIRPLAYSERVER_LOG_H
