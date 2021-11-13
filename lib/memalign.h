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

#ifndef MEMALIGN_H
#define MEMALIGN_H

#if defined(WIN32)

#define SYSTEM_GET_PAGESIZE(ret) do {\
	SYSTEM_INFO si;\
	GetSystemInfo(&si);\
	ret = si.dwPageSize;\
} while(0)
#define SYSTEM_GET_TIME(ret) ret = timeGetTime()

#define ALIGNED_MALLOC(memptr, alignment, size) do {\
	char *ptr = malloc(sizeof(void*) + (size) + (alignment)-1);\
	memptr = NULL;\
	if (ptr) {\
		size_t ptrval = (size_t)ptr + sizeof(void*) + (alignment)-1;\
		ptrval = ptrval / (alignment) * (alignment);\
		memptr = (void *)ptrval;\
		*(((void **)memptr)-1) = ptr;\
	}\
} while(0)
#define ALIGNED_FREE(memptr) free(*(((void **)memptr)-1))

#else

#define SYSTEM_GET_PAGESIZE(ret) ret = sysconf(_SC_PAGESIZE)
#define SYSTEM_GET_TIME(ret) do {\
    struct timeval tv;\
    gettimeofday(&tv, NULL);\
    ret = (unsigned int)(tv.tv_sec*1000 + tv.tv_usec/1000);\
} while(0)

#define ALIGNED_MALLOC(memptr, alignment, size) if (posix_memalign((void **)&memptr, alignment, size)) memptr = NULL
#define ALIGNED_FREE(memptr) free(memptr)

#endif

#endif
