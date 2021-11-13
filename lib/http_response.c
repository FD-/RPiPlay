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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "http_response.h"
#include "compat.h"

struct http_response_s {
    int complete;
    int disconnect;

    char *data;
    int data_size;
    int data_length;
};


static void
http_response_add_data(http_response_t *response, const char *data, int datalen)
{
    int newdatasize;

    assert(response);
    assert(data);
    assert(datalen > 0);

    newdatasize = response->data_size;
    while (response->data_size+datalen > newdatasize) {
        newdatasize *= 2;
    }
    if (newdatasize != response->data_size) {
        response->data = realloc(response->data, newdatasize);
        assert(response->data);
    }
    memcpy(response->data+response->data_length, data, datalen);
    response->data_length += datalen;
}

http_response_t *
http_response_init(const char *protocol, int code, const char *message)
{
    http_response_t *response;
    char codestr[4];

    assert(code >= 100 && code < 1000);

    /* Convert code into string */
    memset(codestr, 0, sizeof(codestr));
    snprintf(codestr, sizeof(codestr), "%u", code);

    response = calloc(1, sizeof(http_response_t));
    if (!response) {
        return NULL;
    }

    /* Allocate response data */
    response->data_size = 1024;
    response->data = malloc(response->data_size);
    if (!response->data) {
        free(response);
        return NULL;
    }

    /* Add first line of response to the data array */
    http_response_add_data(response, protocol, strlen(protocol));
    http_response_add_data(response, " ", 1);
    http_response_add_data(response, codestr, strlen(codestr));
    http_response_add_data(response, " ", 1);
    http_response_add_data(response, message, strlen(message));
    http_response_add_data(response, "\r\n", 2);

    return response;
}

void
http_response_destroy(http_response_t *response)
{
    if (response) {
        free(response->data);
        free(response);
    }
}

void
http_response_add_header(http_response_t *response, const char *name, const char *value)
{
    assert(response);
    assert(name);
    assert(value);

    http_response_add_data(response, name, strlen(name));
    http_response_add_data(response, ": ", 2);
    http_response_add_data(response, value, strlen(value));
    http_response_add_data(response, "\r\n", 2);
}

void
http_response_finish(http_response_t *response, const char *data, int datalen)
{
    assert(response);
    assert(datalen==0 || (data && datalen > 0));

    if (data && datalen > 0) {
        const char *hdrname = "Content-Length";
        char hdrvalue[16];

        memset(hdrvalue, 0, sizeof(hdrvalue));
        snprintf(hdrvalue, sizeof(hdrvalue)-1, "%d", datalen);

        /* Add Content-Length header first */
        http_response_add_data(response, hdrname, strlen(hdrname));
        http_response_add_data(response, ": ", 2);
        http_response_add_data(response, hdrvalue, strlen(hdrvalue));
        http_response_add_data(response, "\r\n\r\n", 4);

        /* Add data to the end of response */
        http_response_add_data(response, data, datalen);
    } else {
        /* Add extra end of line after headers */
        http_response_add_data(response, "\r\n", 2);
    }
    response->complete = 1;
}

void
http_response_set_disconnect(http_response_t *response, int disconnect)
{
    assert(response);

    response->disconnect = !!disconnect;
}

int
http_response_get_disconnect(http_response_t *response)
{
    assert(response);

    return response->disconnect;
}

const char *
http_response_get_data(http_response_t *response, int *datalen)
{
    assert(response);
    assert(datalen);
    assert(response->complete);

    *datalen = response->data_length;
    return response->data;
}
