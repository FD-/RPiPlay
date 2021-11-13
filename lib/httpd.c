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
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "httpd.h"
#include "netutils.h"
#include "http_request.h"
#include "compat.h"
#include "logger.h"

struct http_connection_s {
    int connected;

    int socket_fd;
    void *user_data;
    http_request_t *request;
};
typedef struct http_connection_s http_connection_t;

struct httpd_s {
    logger_t *logger;
    httpd_callbacks_t callbacks;

    int max_connections;
    int open_connections;
    http_connection_t *connections;

    /* These variables only edited mutex locked */
    int running;
    int joined;
    thread_handle_t thread;
    mutex_handle_t run_mutex;

    /* Server fds for accepting connections */
    int server_fd4;
    int server_fd6;
};

httpd_t *
httpd_init(logger_t *logger, httpd_callbacks_t *callbacks, int max_connections)
{
    httpd_t *httpd;

    assert(logger);
    assert(callbacks);
    assert(max_connections > 0);

    /* Allocate the httpd_t structure */
    httpd = calloc(1, sizeof(httpd_t));
    if (!httpd) {
        return NULL;
    }

    httpd->max_connections = max_connections;
    httpd->connections = calloc(max_connections, sizeof(http_connection_t));
    if (!httpd->connections) {
        free(httpd);
        return NULL;
    }

    /* Use the logger provided */
    httpd->logger = logger;

    /* Save callback pointers */
    memcpy(&httpd->callbacks, callbacks, sizeof(httpd_callbacks_t));

    /* Initial status joined */
    httpd->running = 0;
    httpd->joined = 1;

    return httpd;
}

void
httpd_destroy(httpd_t *httpd)
{
    if (httpd) {
        httpd_stop(httpd);

        free(httpd->connections);
        free(httpd);
    }
}

static int
httpd_add_connection(httpd_t *httpd, int fd, unsigned char *local, int local_len, unsigned char *remote, int remote_len)
{
    void *user_data;
    int i;

    for (i=0; i<httpd->max_connections; i++) {
        if (!httpd->connections[i].connected) {
            break;
        }
    }
    if (i == httpd->max_connections) {
        /* This code should never be reached, we do not select server_fds when full */
        logger_log(httpd->logger, LOGGER_INFO, "Max connections reached");
        return -1;
    }

    user_data = httpd->callbacks.conn_init(httpd->callbacks.opaque, local, local_len, remote, remote_len);
    if (!user_data) {
        logger_log(httpd->logger, LOGGER_ERR, "Error initializing HTTP request handler");
        return -1;
    }

    httpd->open_connections++;
    httpd->connections[i].socket_fd = fd;
    httpd->connections[i].connected = 1;
    httpd->connections[i].user_data = user_data;
    return 0;
}

static int
httpd_accept_connection(httpd_t *httpd, int server_fd, int is_ipv6)
{
    struct sockaddr_storage remote_saddr;
    socklen_t remote_saddrlen;
    struct sockaddr_storage local_saddr;
    socklen_t local_saddrlen;
    unsigned char *local, *remote;
    int local_len, remote_len;
    int ret, fd;

    remote_saddrlen = sizeof(remote_saddr);
    fd = accept(server_fd, (struct sockaddr *)&remote_saddr, &remote_saddrlen);
    if (fd == -1) {
        /* FIXME: Error happened */
        return -1;
    }

    local_saddrlen = sizeof(local_saddr);
    ret = getsockname(fd, (struct sockaddr *)&local_saddr, &local_saddrlen);
    if (ret == -1) {
        shutdown(fd, SHUT_RDWR);
        closesocket(fd);
        return 0;
    }

    logger_log(httpd->logger, LOGGER_INFO, "Accepted %s client on socket %d",
               (is_ipv6 ? "IPv6"  : "IPv4"), fd);
    local = netutils_get_address(&local_saddr, &local_len);
    remote = netutils_get_address(&remote_saddr, &remote_len);

    ret = httpd_add_connection(httpd, fd, local, local_len, remote, remote_len);
    if (ret == -1) {
        shutdown(fd, SHUT_RDWR);
        closesocket(fd);
        return 0;
    }
    return 1;
}

static void
httpd_remove_connection(httpd_t *httpd, http_connection_t *connection)
{
    if (connection->request) {
        http_request_destroy(connection->request);
        connection->request = NULL;
    }
    httpd->callbacks.conn_destroy(connection->user_data);
    shutdown(connection->socket_fd, SHUT_WR);
    closesocket(connection->socket_fd);
    connection->connected = 0;
    httpd->open_connections--;
}

static THREAD_RETVAL
httpd_thread(void *arg)
{
    httpd_t *httpd = arg;
    char buffer[1024];
    int i;

    assert(httpd);

    while (1) {
        fd_set rfds;
        struct timeval tv;
        int nfds=0;
        int ret;

        MUTEX_LOCK(httpd->run_mutex);
        if (!httpd->running) {
            MUTEX_UNLOCK(httpd->run_mutex);
            break;
        }
        MUTEX_UNLOCK(httpd->run_mutex);

        /* Set timeout value to 5ms */
        tv.tv_sec = 1;
        tv.tv_usec = 5000;

        /* Get the correct nfds value and set rfds */
        FD_ZERO(&rfds);
        if (httpd->open_connections < httpd->max_connections) {
            if (httpd->server_fd4 != -1) {
                FD_SET(httpd->server_fd4, &rfds);
                if (nfds <= httpd->server_fd4) {
                    nfds = httpd->server_fd4+1;
                }
            }
            if (httpd->server_fd6 != -1) {
                FD_SET(httpd->server_fd6, &rfds);
                if (nfds <= httpd->server_fd6) {
                    nfds = httpd->server_fd6+1;
                }
            }
        }
        for (i=0; i<httpd->max_connections; i++) {
            int socket_fd;
            if (!httpd->connections[i].connected) {
                continue;
            }
            socket_fd = httpd->connections[i].socket_fd;
            FD_SET(socket_fd, &rfds);
            if (nfds <= socket_fd) {
                nfds = socket_fd+1;
            }
        }

        ret = select(nfds, &rfds, NULL, NULL, &tv);
        if (ret == 0) {
            /* Timeout happened */
            continue;
        } else if (ret == -1) {
            logger_log(httpd->logger, LOGGER_ERR, "httpd error in select");
            break;
        }

        if (httpd->open_connections < httpd->max_connections &&
            httpd->server_fd4 != -1 && FD_ISSET(httpd->server_fd4, &rfds)) {
            ret = httpd_accept_connection(httpd, httpd->server_fd4, 0);
            if (ret == -1) {
                logger_log(httpd->logger, LOGGER_ERR, "httpd error in accept ipv4");
                break;
            } else if (ret == 0) {
                continue;
            }
        }
        if (httpd->open_connections < httpd->max_connections &&
            httpd->server_fd6 != -1 && FD_ISSET(httpd->server_fd6, &rfds)) {
            ret = httpd_accept_connection(httpd, httpd->server_fd6, 1);
            if (ret == -1) {
                logger_log(httpd->logger, LOGGER_ERR, "httpd error in accept ipv6");
                break;
            } else if (ret == 0) {
                continue;
            }
        }
        for (i=0; i<httpd->max_connections; i++) {
            http_connection_t *connection = &httpd->connections[i];

            if (!connection->connected) {
                continue;
            }
            if (!FD_ISSET(connection->socket_fd, &rfds)) {
                continue;
            }

            /* If not in the middle of request, allocate one */
            if (!connection->request) {
                connection->request = http_request_init();
                assert(connection->request);
            }

            logger_log(httpd->logger, LOGGER_DEBUG, "httpd receiving on socket %d", connection->socket_fd);
            ret = recv(connection->socket_fd, buffer, sizeof(buffer), 0);
            if (ret == 0) {
                logger_log(httpd->logger, LOGGER_INFO, "Connection closed for socket %d", connection->socket_fd);
                httpd_remove_connection(httpd, connection);
                continue;
            }

            /* Parse HTTP request from data read from connection */
            http_request_add_data(connection->request, buffer, ret);
            if (http_request_has_error(connection->request)) {
                logger_log(httpd->logger, LOGGER_ERR, "httpd error in parsing: %s", http_request_get_error_name(connection->request));
                httpd_remove_connection(httpd, connection);
                continue;
            }

            /* If request is finished, process and deallocate */
            if (http_request_is_complete(connection->request)) {
                http_response_t *response = NULL;
                // Callback the received data to raop
                httpd->callbacks.conn_request(connection->user_data, connection->request, &response);
                http_request_destroy(connection->request);
                connection->request = NULL;

                if (response) {
                    const char *data;
                    int datalen;
                    int written;
                    int ret;

                    /* Get response data and datalen */
                    data = http_response_get_data(response, &datalen);

                    written = 0;
                    while (written < datalen) {
                        ret = send(connection->socket_fd, data+written, datalen-written, 0);
                        if (ret == -1) {
                            logger_log(httpd->logger, LOGGER_ERR, "httpd error in sending data");
                            break;
                        }
                        written += ret;
                    }

                    if (http_response_get_disconnect(response)) {
                        logger_log(httpd->logger, LOGGER_INFO, "Disconnecting on software request");
                        httpd_remove_connection(httpd, connection);
                    }
                } else {
                    logger_log(httpd->logger, LOGGER_WARNING, "httpd didn't get response");
                }
                http_response_destroy(response);
            } else {
                logger_log(httpd->logger, LOGGER_DEBUG, "Request not complete, waiting for more data...");
            }
        }
    }

    /* Remove all connections that are still connected */
    for (i=0; i<httpd->max_connections; i++) {
        http_connection_t *connection = &httpd->connections[i];

        if (!connection->connected) {
            continue;
        }
        logger_log(httpd->logger, LOGGER_INFO, "Removing connection for socket %d", connection->socket_fd);
        httpd_remove_connection(httpd, connection);
    }

    /* Close server sockets since they are not used any more */
    if (httpd->server_fd4 != -1) {
        shutdown(httpd->server_fd4, SHUT_RDWR);
        closesocket(httpd->server_fd4);
        httpd->server_fd4 = -1;
    }
    if (httpd->server_fd6 != -1) {
        shutdown(httpd->server_fd6, SHUT_RDWR);
        closesocket(httpd->server_fd6);
        httpd->server_fd6 = -1;
    }

    // Ensure running reflects the actual state
    MUTEX_LOCK(httpd->run_mutex);
    httpd->running = 0;
    MUTEX_UNLOCK(httpd->run_mutex);

    logger_log(httpd->logger, LOGGER_DEBUG, "Exiting HTTP thread");

    return 0;
}

int
httpd_start(httpd_t *httpd, unsigned short *port)
{
    /* How many connection attempts are kept in queue */
    int backlog = 5;

    assert(httpd);
    assert(port);

    MUTEX_LOCK(httpd->run_mutex);
    if (httpd->running || !httpd->joined) {
        MUTEX_UNLOCK(httpd->run_mutex);
        return 0;
    }

    httpd->server_fd4 = netutils_init_socket(port, 0, 0);
    if (httpd->server_fd4 == -1) {
        logger_log(httpd->logger, LOGGER_ERR, "Error initialising socket %d", SOCKET_GET_ERROR());
        MUTEX_UNLOCK(httpd->run_mutex);
        return -1;
    }
    httpd->server_fd6 = -1;/*= netutils_init_socket(port, 1, 0);
	if (httpd->server_fd6 == -1) {
		logger_log(httpd->logger, LOGGER_WARNING, "Error initialising IPv6 socket %d", SOCKET_GET_ERROR());
		logger_log(httpd->logger, LOGGER_WARNING, "Continuing without IPv6 support");
	}*/

    if (httpd->server_fd4 != -1 && listen(httpd->server_fd4, backlog) == -1) {
        logger_log(httpd->logger, LOGGER_ERR, "Error listening to IPv4 socket");
        closesocket(httpd->server_fd4);
        closesocket(httpd->server_fd6);
        MUTEX_UNLOCK(httpd->run_mutex);
        return -2;
    }
    if (httpd->server_fd6 != -1 && listen(httpd->server_fd6, backlog) == -1) {
        logger_log(httpd->logger, LOGGER_ERR, "Error listening to IPv6 socket");
        closesocket(httpd->server_fd4);
        closesocket(httpd->server_fd6);
        MUTEX_UNLOCK(httpd->run_mutex);
        return -2;
    }
    logger_log(httpd->logger, LOGGER_INFO, "Initialized server socket(s)");

    /* Set values correctly and create new thread */
    httpd->running = 1;
    httpd->joined = 0;
    THREAD_CREATE(httpd->thread, httpd_thread, httpd);
    MUTEX_UNLOCK(httpd->run_mutex);

    return 1;
}

int
httpd_is_running(httpd_t *httpd)
{
    int running;

    assert(httpd);

    MUTEX_LOCK(httpd->run_mutex);
    running = httpd->running || !httpd->joined;
    MUTEX_UNLOCK(httpd->run_mutex);

    return running;
}

void
httpd_stop(httpd_t *httpd)
{
    assert(httpd);

    MUTEX_LOCK(httpd->run_mutex);
    if (!httpd->running || httpd->joined) {
        MUTEX_UNLOCK(httpd->run_mutex);
        return;
    }
    httpd->running = 0;
    MUTEX_UNLOCK(httpd->run_mutex);

    THREAD_JOIN(httpd->thread);

    MUTEX_LOCK(httpd->run_mutex);
    httpd->joined = 1;
    MUTEX_UNLOCK(httpd->run_mutex);
}

