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

#include "raop.h"
#include "raop_rtp.h"
#include "raop_rtp.h"
#include "pairing.h"
#include "httpd.h"

#include "global.h"
#include "fairplay.h"
#include "netutils.h"
#include "logger.h"
#include "compat.h"
#include "raop_rtp_mirror.h"
#include "raop_ntp.h"

struct raop_s {
    /* Callbacks for audio and video */
    raop_callbacks_t callbacks;

    /* Logger instance */
    logger_t *logger;

    /* Pairing, HTTP daemon and RSA key */
    pairing_t *pairing;
    httpd_t *httpd;

    dnssd_t *dnssd;

    unsigned short port;
};

struct raop_conn_s {
    raop_t *raop;
    raop_ntp_t *raop_ntp;
    raop_rtp_t *raop_rtp;
    raop_rtp_mirror_t *raop_rtp_mirror;
    fairplay_t *fairplay;
    pairing_session_t *pairing;

    unsigned char *local;
    int locallen;

    unsigned char *remote;
    int remotelen;

};
typedef struct raop_conn_s raop_conn_t;

#include "raop_handlers.h"

static void *
conn_init(void *opaque, unsigned char *local, int locallen, unsigned char *remote, int remotelen) {
    raop_t *raop = opaque;
    raop_conn_t *conn;

    assert(raop);

    conn = calloc(1, sizeof(raop_conn_t));
    if (!conn) {
        return NULL;
    }
    conn->raop = raop;
    conn->raop_rtp = NULL;
    conn->raop_ntp = NULL;
    conn->fairplay = fairplay_init(raop->logger);

    if (!conn->fairplay) {
        free(conn);
        return NULL;
    }
    conn->pairing = pairing_session_init(raop->pairing);
    if (!conn->pairing) {
        fairplay_destroy(conn->fairplay);
        free(conn);
        return NULL;
    }

    if (locallen == 4) {
        logger_log(conn->raop->logger, LOGGER_INFO,
                   "Local: %d.%d.%d.%d",
                   local[0], local[1], local[2], local[3]);
    } else if (locallen == 16) {
        logger_log(conn->raop->logger, LOGGER_INFO,
                   "Local: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                   local[0], local[1], local[2], local[3], local[4], local[5], local[6], local[7],
                   local[8], local[9], local[10], local[11], local[12], local[13], local[14], local[15]);
    }
    if (remotelen == 4) {
        logger_log(conn->raop->logger, LOGGER_INFO,
                   "Remote: %d.%d.%d.%d",
                   remote[0], remote[1], remote[2], remote[3]);
    } else if (remotelen == 16) {
        logger_log(conn->raop->logger, LOGGER_INFO,
                   "Remote: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                   remote[0], remote[1], remote[2], remote[3], remote[4], remote[5], remote[6], remote[7],
                   remote[8], remote[9], remote[10], remote[11], remote[12], remote[13], remote[14], remote[15]);
    }

    conn->local = malloc(locallen);
    assert(conn->local);
    memcpy(conn->local, local, locallen);

    conn->remote = malloc(remotelen);
    assert(conn->remote);
    memcpy(conn->remote, remote, remotelen);

    conn->locallen = locallen;
    conn->remotelen = remotelen;

    if (raop->callbacks.conn_init) {
        raop->callbacks.conn_init(raop->callbacks.cls);
    }

    return conn;
}

static void
conn_request(void *ptr, http_request_t *request, http_response_t **response) {
    raop_conn_t *conn = ptr;
    logger_log(conn->raop->logger, LOGGER_DEBUG, "conn_request");
    const char *method;
    const char *url;
    const char *cseq;

    char *response_data = NULL;
    int response_datalen = 0;

    method = http_request_get_method(request);
    url = http_request_get_url(request);
    cseq = http_request_get_header(request, "CSeq");
    if (!method || !cseq) {
        return;
    }

    *response = http_response_init("RTSP/1.0", 200, "OK");

    http_response_add_header(*response, "CSeq", cseq);
    //http_response_add_header(*response, "Apple-Jack-Status", "connected; type=analog");
    http_response_add_header(*response, "Server", "AirTunes/220.68");

    logger_log(conn->raop->logger, LOGGER_DEBUG, "Handling request %s with URL %s", method, url);
    raop_handler_t handler = NULL;
    if (!strcmp(method, "GET") && !strcmp(url, "/info")) {
        handler = &raop_handler_info;
    } else if (!strcmp(method, "POST") && !strcmp(url, "/pair-setup")) {
        handler = &raop_handler_pairsetup;
    } else if (!strcmp(method, "POST") && !strcmp(url, "/pair-verify")) {
        handler = &raop_handler_pairverify;
    } else if (!strcmp(method, "POST") && !strcmp(url, "/fp-setup")) {
        handler = &raop_handler_fpsetup;
    } else if (!strcmp(method, "OPTIONS")) {
        handler = &raop_handler_options;
    } else if (!strcmp(method, "SETUP")) {
        handler = &raop_handler_setup;
    } else if (!strcmp(method, "GET_PARAMETER")) {
        handler = &raop_handler_get_parameter;
    } else if (!strcmp(method, "SET_PARAMETER")) {
        handler = &raop_handler_set_parameter;
    } else if (!strcmp(method, "POST") && !strcmp(url, "/feedback")) {
        handler = &raop_handler_feedback;
    } else if (!strcmp(method, "RECORD")) {
        handler = &raop_handler_record;
    } else if (!strcmp(method, "FLUSH")) {
        const char *rtpinfo;
        int next_seq = -1;

        rtpinfo = http_request_get_header(request, "RTP-Info");
        if (rtpinfo) {
            logger_log(conn->raop->logger, LOGGER_DEBUG, "Flush with RTP-Info: %s", rtpinfo);
            if (!strncmp(rtpinfo, "seq=", 4)) {
                next_seq = strtol(rtpinfo + 4, NULL, 10);
            }
        }
        if (conn->raop_rtp) {
            raop_rtp_flush(conn->raop_rtp, next_seq);
        } else {
            logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP not initialized at FLUSH");
        }
    } else if (!strcmp(method, "TEARDOWN")) {
        logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP teardown start");
        //http_response_add_header(*response, "Connection", "close");
        if (conn->raop_rtp != NULL && raop_rtp_is_running(conn->raop_rtp)) {
            /* Destroy our RTP session */
        	logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP destroying our RTP session");
            raop_rtp_stop(conn->raop_rtp);
        	logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP destroyed our RTP session");
        } else if (conn->raop_rtp_mirror) {
            /* Destroy our sessions */
        	logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP destroying our sessions");
            raop_rtp_destroy(conn->raop_rtp);
            conn->raop_rtp = NULL;
            raop_rtp_mirror_destroy(conn->raop_rtp_mirror);
            conn->raop_rtp_mirror = NULL;
        	logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP destroyed our sessions");
        }
        logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP teardown done");
    }
    if (handler != NULL) {
        handler(conn, request, *response, &response_data, &response_datalen);
    }
    http_response_finish(*response, response_data, response_datalen);
    if (response_data) {
        free(response_data);
        response_data = NULL;
        response_datalen = 0;
    }
    logger_log(conn->raop->logger, LOGGER_DEBUG, "conn_request done");
}

static void
conn_destroy(void *ptr) {
    raop_conn_t *conn = ptr;

    logger_log(conn->raop->logger, LOGGER_INFO, "Destroying connection");

    if (conn->raop->callbacks.conn_destroy) {
        conn->raop->callbacks.conn_destroy(conn->raop->callbacks.cls);
    }

    if (conn->raop_ntp) {
        raop_ntp_destroy(conn->raop_ntp);
    }
    if (conn->raop_rtp) {
        /* This is done in case TEARDOWN was not called */
        raop_rtp_destroy(conn->raop_rtp);
    }
    if (conn->raop_rtp_mirror) {
        /* This is done in case TEARDOWN was not called */
        raop_rtp_mirror_destroy(conn->raop_rtp_mirror);
    }

    logger_log(conn->raop->logger, LOGGER_INFO, "  video_flush");
    conn->raop->callbacks.video_flush(conn->raop->callbacks.cls);

    free(conn->local);
    free(conn->remote);

    logger_log(conn->raop->logger, LOGGER_INFO, "  Destroying session");

    pairing_session_destroy(conn->pairing);
    fairplay_destroy(conn->fairplay);

    logger_log(conn->raop->logger, LOGGER_INFO, "Destroyed connection");

    free(conn);

	return;
}

raop_t *
raop_init(int max_clients, raop_callbacks_t *callbacks) {
    raop_t *raop;
    pairing_t *pairing;
    httpd_t *httpd;
    httpd_callbacks_t httpd_cbs;

    assert(callbacks);
    assert(max_clients > 0);
    assert(max_clients < 100);

    /* Initialize the network */
    if (netutils_init() < 0) {
        return NULL;
    }

    /* Validate the callbacks structure */
    if (!callbacks->audio_process ||
        !callbacks->video_process) {
        return NULL;
    }

    /* Allocate the raop_t structure */
    raop = calloc(1, sizeof(raop_t));
    if (!raop) {
        return NULL;
    }

    /* Initialize the logger */
    raop->logger = logger_init();
    pairing = pairing_init_generate();
    if (!pairing) {
        free(raop);
        return NULL;
    }

    /* Set HTTP callbacks to our handlers */
    memset(&httpd_cbs, 0, sizeof(httpd_cbs));
    httpd_cbs.opaque = raop;
    httpd_cbs.conn_init = &conn_init;
    httpd_cbs.conn_request = &conn_request;
    httpd_cbs.conn_destroy = &conn_destroy;

    /* Initialize the http daemon */
    httpd = httpd_init(raop->logger, &httpd_cbs, max_clients);
    if (!httpd) {
        pairing_destroy(pairing);
        free(raop);
        return NULL;
    }
    /* Copy callbacks structure */
    memcpy(&raop->callbacks, callbacks, sizeof(raop_callbacks_t));
    raop->pairing = pairing;
    raop->httpd = httpd;

    return raop;
}

void
raop_destroy(raop_t *raop) {
    logger_log(raop->logger, LOGGER_INFO, "Destroying raop");

    if (raop) {
        raop_stop(raop);
        pairing_destroy(raop->pairing);
        httpd_destroy(raop->httpd);
        logger_destroy(raop->logger);
        free(raop);

        /* Cleanup the network */
        netutils_cleanup();
    }

    logger_log(raop->logger, LOGGER_INFO, "Destroyed raop");

	return;
}

int
raop_is_running(raop_t *raop) {
    assert(raop);

    return httpd_is_running(raop->httpd);
}

void
raop_set_log_level(raop_t *raop, int level) {
    assert(raop);

    logger_set_level(raop->logger, level);
}

void
raop_set_port(raop_t *raop, unsigned short port) {
    assert(raop);
    raop->port = port;
}

unsigned short
raop_get_port(raop_t *raop) {
    assert(raop);
    return raop->port;
}

void *
raop_get_callback_cls(raop_t *raop) {
    assert(raop);
    return raop->callbacks.cls;
}

void
raop_set_log_callback(raop_t *raop, raop_log_callback_t callback, void *cls) {
    assert(raop);

    logger_set_callback(raop->logger, callback, cls);
}

void
raop_set_dnssd(raop_t *raop, dnssd_t *dnssd) {
    assert(dnssd);
    raop->dnssd = dnssd;
}


int
raop_start(raop_t *raop, unsigned short *port) {
    assert(raop);
    assert(port);
    return httpd_start(raop->httpd, port);
}

void
raop_stop(raop_t *raop) {
    assert(raop);
    httpd_stop(raop->httpd);
}
