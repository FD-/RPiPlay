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

#ifndef MIRROR_BUFFER_H
#define MIRROR_BUFFER_H

#include <stdint.h>
#include "logger.h"

typedef struct mirror_buffer_s mirror_buffer_t;


mirror_buffer_t *mirror_buffer_init( logger_t *logger,
        const unsigned char *aeskey,
        const unsigned char *ecdh_secret);
void mirror_buffer_init_aes(mirror_buffer_t *mirror_buffer, uint64_t streamConnectionID);
void mirror_buffer_decrypt(mirror_buffer_t *raop_mirror, unsigned char* input, unsigned char* output, int datalen);
void mirror_buffer_destroy(mirror_buffer_t *mirror_buffer);
#endif //MIRROR_BUFFER_H
