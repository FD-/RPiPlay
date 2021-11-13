/*
 * Copyright (c) 2019 dsafa22, All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef AIRPLAYSERVER_STREAM_H
#define AIRPLAYSERVER_STREAM_H

#include <stdint.h>

typedef struct {
    int n_gop_index;
    int frame_type;
    int n_frame_poc;
    unsigned char *data;
    int data_len;
    unsigned int n_time_stamp;
    uint64_t pts;
} h264_decode_struct;

typedef struct {
    unsigned char *data;
    int data_len;
    uint64_t pts;
} aac_decode_struct;

#endif //AIRPLAYSERVER_STREAM_H
