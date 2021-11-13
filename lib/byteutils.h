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

#ifndef AIRPLAYSERVER_BYTEUTILS_H
#define AIRPLAYSERVER_BYTEUTILS_H
#include <stdint.h>

uint16_t byteutils_get_short(unsigned char* b, int offset);
uint32_t byteutils_get_int(unsigned char* b, int offset);
uint64_t byteutils_get_long(unsigned char* b, int offset);
uint16_t byteutils_get_short_be(unsigned char* b, int offset);
uint32_t byteutils_get_int_be(unsigned char* b, int offset);
uint64_t byteutils_get_long_be(unsigned char* b, int offset);
float byteutils_get_float(unsigned char* b, int offset);

#define SECONDS_FROM_1900_TO_1970 2208988800ULL

uint64_t byteutils_get_ntp_timestamp(unsigned char *b, int offset);
void byteutils_put_ntp_timestamp(unsigned char *b, int offset, uint64_t us_since_1970);

#endif //AIRPLAYSERVER_BYTEUTILS_H
