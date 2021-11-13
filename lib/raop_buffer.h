/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef RAOP_BUFFER_H
#define RAOP_BUFFER_H

#include "logger.h"
#include "raop_rtp.h"

typedef struct raop_buffer_s raop_buffer_t;

typedef int (*raop_resend_cb_t)(void *opaque, unsigned short seqno, unsigned short count);

raop_buffer_t *raop_buffer_init(logger_t *logger,
                                const unsigned char *aeskey,
                                const unsigned char *aesiv,
                                const unsigned char *ecdh_secret);
int raop_buffer_enqueue(raop_buffer_t *raop_buffer, unsigned char *data, unsigned short datalen, uint64_t timestamp, int use_seqnum);
void *raop_buffer_dequeue(raop_buffer_t *raop_buffer, unsigned int *length, uint64_t *timestamp, int no_resend);
void raop_buffer_handle_resends(raop_buffer_t *raop_buffer, raop_resend_cb_t resend_cb, void *opaque);
void raop_buffer_flush(raop_buffer_t *raop_buffer, int next_seq);

int raop_buffer_decrypt(raop_buffer_t *raop_buffer, unsigned char *data, unsigned char* output,
                        unsigned int datalen, unsigned int *outputlen);
void raop_buffer_destroy(raop_buffer_t *raop_buffer);

#endif
