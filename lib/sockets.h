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

#ifndef SOCKETS_H
#define SOCKETS_H

#if defined(WIN32)
typedef int socklen_t;

#ifndef SHUT_RD
#  define SHUT_RD SD_RECEIVE
#endif
#ifndef SHUT_WR
#  define SHUT_WR SD_SEND
#endif
#ifndef SHUT_RDWR
#  define SHUT_RDWR SD_BOTH
#endif

#define SOCKET_GET_ERROR()      WSAGetLastError()
#define SOCKET_SET_ERROR(value) WSASetLastError(value)
#define SOCKET_ERRORNAME(name)  WSA##name

#define WSAEAGAIN WSAEWOULDBLOCK
#define WSAENOMEM WSA_NOT_ENOUGH_MEMORY

#else

#define closesocket close
#define ioctlsocket ioctl

#define SOCKET_GET_ERROR()      (errno)
#define SOCKET_SET_ERROR(value) (errno = (value))
#define SOCKET_ERRORNAME(name)  name

#endif

#endif
