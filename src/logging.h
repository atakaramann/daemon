/* SPDX-License-Identifier: MIT */
#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

/* -l argument maps directly onto these: 0=ERROR, 1=INFO, 2=DEBUG. */
enum log_level {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_INFO  = 1,
	LOG_LEVEL_DEBUG = 2,
};

/* Initialize the logging backend to syslog or a file. */
int logger_init(const char *dest, enum log_level level);

/* All log messages pass through this function. */
void logger_write(enum log_level level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* Reopen the log destination. Does nothing for syslog. */
int logger_reopen(void);

/* Flush and release the destination. */
void logger_close(void);

/* Convenience logging macros. */
#define log_error(...) logger_write(LOG_LEVEL_ERROR, __VA_ARGS__)
#define log_info(...)  logger_write(LOG_LEVEL_INFO,  __VA_ARGS__)
#define log_debug(...) logger_write(LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif /* LOGGING_H */
