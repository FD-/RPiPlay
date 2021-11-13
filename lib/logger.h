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

#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Define syslog style log levels */
#define LOGGER_EMERG       0       /* system is unusable */
#define LOGGER_ALERT       1       /* action must be taken immediately */
#define LOGGER_CRIT        2       /* critical conditions */
#define LOGGER_ERR         3       /* error conditions */
#define LOGGER_WARNING     4       /* warning conditions */
#define LOGGER_NOTICE      5       /* normal but significant condition */
#define LOGGER_INFO        6       /* informational */
#define LOGGER_DEBUG       7       /* debug-level messages */

typedef void (*logger_callback_t)(void *cls, int level, const char *msg);

typedef struct logger_s logger_t;

logger_t *logger_init();
void logger_destroy(logger_t *logger);

void logger_set_level(logger_t *logger, int level);
void logger_set_callback(logger_t *logger, logger_callback_t callback, void *cls);

void logger_log(logger_t *logger, int level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
