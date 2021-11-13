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

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

typedef struct http_request_s http_request_t;


http_request_t *http_request_init(void);

int http_request_add_data(http_request_t *request, const char *data, int datalen);
int http_request_is_complete(http_request_t *request);
int http_request_has_error(http_request_t *request);

const char *http_request_get_error_name(http_request_t *request);
const char *http_request_get_error_description(http_request_t *request);
const char *http_request_get_method(http_request_t *request);
const char *http_request_get_url(http_request_t *request);
const char *http_request_get_header(http_request_t *request, const char *name);
const char *http_request_get_data(http_request_t *request, int *datalen);

void http_request_destroy(http_request_t *request);

#endif
