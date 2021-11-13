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

#ifndef RAOP_RTP_H
#define RAOP_RTP_H

/* For raop_callbacks_t */
#include "raop.h"
#include "logger.h"
#include "raop_ntp.h"

#define RAOP_AESIV_LEN  16
#define RAOP_AESKEY_LEN 16
#define RAOP_PACKET_LEN 32768

typedef struct raop_rtp_s raop_rtp_t;

raop_rtp_t *raop_rtp_init(logger_t *logger, raop_callbacks_t *callbacks, raop_ntp_t *ntp, const unsigned char *remote, int remotelen,
                          const unsigned char *aeskey, const unsigned char *aesiv, const unsigned char *ecdh_secret);

void raop_rtp_start_audio(raop_rtp_t *raop_rtp, int use_udp, unsigned short control_rport,
                          unsigned short *control_lport, unsigned short *data_lport);

void raop_rtp_set_volume(raop_rtp_t *raop_rtp, float volume);
void raop_rtp_set_metadata(raop_rtp_t *raop_rtp, const char *data, int datalen);
void raop_rtp_set_coverart(raop_rtp_t *raop_rtp, const char *data, int datalen);
void raop_rtp_remote_control_id(raop_rtp_t *raop_rtp, const char *dacp_id, const char *active_remote_header);
void raop_rtp_set_progress(raop_rtp_t *raop_rtp, unsigned int start, unsigned int curr, unsigned int end);
void raop_rtp_flush(raop_rtp_t *raop_rtp, int next_seq);
void raop_rtp_stop(raop_rtp_t *raop_rtp);
int raop_rtp_is_running(raop_rtp_t *raop_rtp);
void raop_rtp_destroy(raop_rtp_t *raop_rtp);

#endif
