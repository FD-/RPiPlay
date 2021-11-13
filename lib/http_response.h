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

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

typedef struct http_response_s http_response_t;

http_response_t *http_response_init(const char *protocol, int code, const char *message);

void http_response_add_header(http_response_t *response, const char *name, const char *value);
void http_response_finish(http_response_t *response, const char *data, int datalen);

void http_response_set_disconnect(http_response_t *response, int disconnect);
int http_response_get_disconnect(http_response_t *response);

const char *http_response_get_data(http_response_t *response, int *datalen);

void http_response_destroy(http_response_t *response);

#endif
