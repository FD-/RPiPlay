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
#include <assert.h>

#include "compat.h"

int
netutils_init()
{
#ifdef WIN32
    WORD wVersionRequested;
	WSADATA wsaData;
	int ret;

	wVersionRequested = MAKEWORD(2, 2);
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret) {
		return -1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 ||
	    HIBYTE(wsaData.wVersion) != 2) {
		/* Version mismatch, requested version not found */
		return -1;
	}
#endif
    return 0;
}

void
netutils_cleanup()
{
#ifdef WIN32
    WSACleanup();
#endif
}

unsigned char *
netutils_get_address(void *sockaddr, int *length)
{
    unsigned char ipv4_prefix[] = { 0,0,0,0,0,0,0,0,0,0,255,255 };
    struct sockaddr *address = sockaddr;

    assert(address);
    assert(length);

    if (address->sa_family == AF_INET) {
        struct sockaddr_in *sin;

        sin = (struct sockaddr_in *)address;
        *length = sizeof(sin->sin_addr.s_addr);
        return (unsigned char *)&sin->sin_addr.s_addr;
    } else if (address->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin6;

        sin6 = (struct sockaddr_in6 *)address;
        if (!memcmp(sin6->sin6_addr.s6_addr, ipv4_prefix, 12)) {
            /* Actually an embedded IPv4 address */
            *length = sizeof(sin6->sin6_addr.s6_addr)-12;
            return (sin6->sin6_addr.s6_addr+12);
        }
        *length = sizeof(sin6->sin6_addr.s6_addr);
        return sin6->sin6_addr.s6_addr;
    }

    *length = 0;
    return NULL;
}

int
netutils_init_socket(unsigned short *port, int use_ipv6, int use_udp)
{
    int family = use_ipv6 ? AF_INET6 : AF_INET;
    int type = use_udp ? SOCK_DGRAM : SOCK_STREAM;
    int proto = use_udp ? IPPROTO_UDP : IPPROTO_TCP;

    struct sockaddr_storage saddr;
    socklen_t socklen;
    int server_fd;
    int ret;
    int reuseaddr = 1;

    assert(port);

    server_fd = socket(family, type, proto);
    if (server_fd == -1) {
        goto cleanup;
    }

    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof (reuseaddr));
    if (ret == -1) {
        goto cleanup;
    }

    memset(&saddr, 0, sizeof(saddr));
    if (use_ipv6) {
        struct sockaddr_in6 *sin6ptr = (struct sockaddr_in6 *)&saddr;
        int v6only = 1;

        /* Initialize sockaddr for bind */
        sin6ptr->sin6_family = family;
        sin6ptr->sin6_addr = in6addr_any;
        sin6ptr->sin6_port = htons(*port);

#ifndef WIN32
        /* Make sure we only listen to IPv6 addresses */
        setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                   (char *) &v6only, sizeof(v6only));
#endif

        socklen = sizeof(*sin6ptr);
        ret = bind(server_fd, (struct sockaddr *)sin6ptr, socklen);
        if (ret == -1) {
            goto cleanup;
        }

        ret = getsockname(server_fd, (struct sockaddr *)sin6ptr, &socklen);
        if (ret == -1) {
            goto cleanup;
        }
        *port = ntohs(sin6ptr->sin6_port);
    } else {
        struct sockaddr_in *sinptr = (struct sockaddr_in *)&saddr;

        /* Initialize sockaddr for bind */
        sinptr->sin_family = family;
        sinptr->sin_addr.s_addr = INADDR_ANY;
        sinptr->sin_port = htons(*port);

        socklen = sizeof(*sinptr);
        ret = bind(server_fd, (struct sockaddr *)sinptr, socklen);
        if (ret == -1) {
            goto cleanup;
        }

        ret = getsockname(server_fd, (struct sockaddr *)sinptr, &socklen);
        if (ret == -1) {
            goto cleanup;
        }
        *port = ntohs(sinptr->sin_port);
    }
    return server_fd;

    cleanup:
    ret = SOCKET_GET_ERROR();
    if (server_fd != -1) {
        closesocket(server_fd);
    }
    SOCKET_SET_ERROR(ret);
    return -1;
}

// Src is the ip address
int
netutils_parse_address(int family, const char *src, void *dst, int dstlen)
{
    struct addrinfo *result;
    struct addrinfo *ptr;
    struct addrinfo hints;
    int length;
    int ret;

    if (family != AF_INET && family != AF_INET6) {
        return -1;
    }
    if (!src || !dst) {
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

    ret = getaddrinfo(src, NULL, &hints, &result);
    if (ret != 0) {
        return -1;
    }

    length = -1;
    for (ptr=result; ptr!=NULL; ptr=ptr->ai_next) {
        if (family == ptr->ai_family && (unsigned int)dstlen >= ptr->ai_addrlen) {
            memcpy(dst, ptr->ai_addr, ptr->ai_addrlen);
            length = ptr->ai_addrlen;
            break;
        }
    }
    freeaddrinfo(result);
    return length;
}
