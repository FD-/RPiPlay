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

#include <time.h>
#include <netinet/in.h>
#include "byteutils.h"

#ifndef htonll
#include <endian.h>
#define htonll(x) htobe64(x)
#define ntohll(x) be64toh(x)
#endif

// The functions in this file assume a little endian cpu architecture!

/**
 * Reads a little endian unsigned 16 bit integer from the buffer at position offset
 */
uint16_t byteutils_get_short(unsigned char* b, int offset) {
    return *((uint16_t*)(b + offset));
}

/**
 * Reads a little endian unsigned 32 bit integer from the buffer at position offset
 */
uint32_t byteutils_get_int(unsigned char* b, int offset) {
    return *((uint32_t*)(b + offset));
}

/**
 * Reads a little endian unsigned 64 bit integer from the buffer at position offset
 */
uint64_t byteutils_get_long(unsigned char* b, int offset) {
    return *((uint64_t*)(b + offset));
}

/**
 * Reads a big endian unsigned 16 bit integer from the buffer at position offset
 */
uint16_t byteutils_get_short_be(unsigned char* b, int offset) {
    return ntohs(byteutils_get_short(b, offset));
}

/**
 * Reads a big endian unsigned 32 bit integer from the buffer at position offset
 */
uint32_t byteutils_get_int_be(unsigned char* b, int offset) {
    return ntohl(byteutils_get_int(b, offset));
}

/**
 * Reads a big endian unsigned 64 bit integer from the buffer at position offset
 */
uint64_t byteutils_get_long_be(unsigned char* b, int offset) {
    return ntohll(byteutils_get_long(b, offset));
}

/**
 * Reads a float from the buffer at position offset
 */
float byteutils_get_float(unsigned char* b, int offset) {
    return *((float*)(b + offset));
}

/**
 * Writes a little endian unsigned 32 bit integer to the buffer at position offset
 */
void byteutils_put_int(unsigned char* b, int offset, uint32_t value) {
    *((uint32_t*)(b + offset)) = value;
}

/**
 * Reads an ntp timestamp and returns it as micro seconds since the Unix epoch
 */
uint64_t byteutils_get_ntp_timestamp(unsigned char *b, int offset) {
    uint64_t seconds = ntohl(((unsigned int) byteutils_get_int(b, offset))) - SECONDS_FROM_1900_TO_1970;
    uint64_t fraction = ntohl((unsigned int) byteutils_get_int(b, offset + 4));
    return (seconds * 1000000L) + ((fraction * 1000000L) >> 32);
}

/**
 * Writes a time given as micro seconds since the Unix time epoch as an ntp timestamp
 * into the buffer at position offset
 */
void byteutils_put_ntp_timestamp(unsigned char *b, int offset, uint64_t us_since_1970) {
    uint64_t seconds = us_since_1970 / 1000000L;
    uint64_t microseconds = us_since_1970 % 1000000L;
    seconds += SECONDS_FROM_1900_TO_1970;
    uint64_t fraction = (microseconds << 32) / 1000000L;

    // Write in big endian!
    byteutils_put_int(b, offset, htonl(seconds));
    byteutils_put_int(b, offset + 4, htonl(fraction));
}

