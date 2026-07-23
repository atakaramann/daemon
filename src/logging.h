/* SPDX-License-Identifier: MIT */
#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>
#include "protocol.h"	/* enum log_level lives here (shared over the wire) */

int logger_init(const char *dest, enum log_level level);

void logger_write(enum log_level level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

int logger_reopen(void);

void logger_close(void);

/* Change the level at runtime (FW_CMD_SET_LEVEL). Returns 0, or -1 if the
 * level is out of range. */
int logger_set_level(enum log_level level);

#define log_error(...) logger_write(LOG_LEVEL_ERROR, __VA_ARGS__)
#define log_info(...)  logger_write(LOG_LEVEL_INFO,  __VA_ARGS__)
#define log_debug(...) logger_write(LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif