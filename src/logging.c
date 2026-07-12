// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <limits.h>

#include "logging.h"

enum log_backend {
	BACKEND_SYSLOG,
	BACKEND_FILE,
};

/* Global logger state. */
static struct {
	enum log_backend backend;	/* active logging backend. */
	enum log_level   level;      /* threshold from -l */
	FILE            *fp;         /* file backend only */
	char             path[PATH_MAX]; /* absolute path, kept for reopen() */
} logger;

/* Map internal log levels to syslog priorities. */
static const int syslog_priority[] = {
	[LOG_LEVEL_ERROR] = LOG_ERR,
	[LOG_LEVEL_INFO]  = LOG_INFO,
	[LOG_LEVEL_DEBUG] = LOG_DEBUG,
};

/* Human-readable tags for the file backend. */
static const char *level_name[] = {
	[LOG_LEVEL_ERROR] = "ERROR",
	[LOG_LEVEL_INFO]  = "INFO",
	[LOG_LEVEL_DEBUG] = "DEBUG",
};

/* Check whether this message should be logged. */
static bool should_log(enum log_level level)
{
	return level <= logger.level;
}

/* Open the log file for append; rotation must never truncate it. */
static int open_file(void)
{
	logger.fp = fopen(logger.path, "a");
	if (logger.fp == NULL)
		return -1;

	return 0;
}

/* Initialize the logging backend. */
int logger_init(const char *dest, enum log_level level)
{
	int n;

	logger.level = level;

	if (strcmp(dest, "syslog") == 0) {
		logger.backend = BACKEND_SYSLOG;
		openlog("bat-daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
		return 0;
	}

	logger.backend = BACKEND_FILE;

	/* Save the path for future reopen requests. */
	n = snprintf(logger.path, sizeof(logger.path), "%s", dest);
	if (n < 0 || (size_t)n >= sizeof(logger.path))
		return -1;

	return open_file();
}

/* File backend: add the timestamp, level and pid to each log line. */
static void write_file(enum log_level level, const char *fmt, va_list ap)
{
	char      ts[32];
	time_t    now = time(NULL);
	struct tm tm;

	/* Timestamp; localtime_r stays safe if the daemon grows threads. */
	localtime_r(&now, &tm);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

	fprintf(logger.fp, "%s [%s] pid=%d: ", ts, level_name[level],
		(int)getpid());
	vfprintf(logger.fp, fmt, ap);
	fputc('\n', logger.fp);

	/* Flush buffered output to avoid losing recent log messages. */
	fflush(logger.fp);
}

/* Write a log message to the active backend. */
void logger_write(enum log_level level, const char *fmt, ...)
{
	va_list ap;

	/* Reject invalid log levels before indexing the lookup tables. */
	if (level < LOG_LEVEL_ERROR || level > LOG_LEVEL_DEBUG)
		return;

	if (!should_log(level))
		return;

	va_start(ap, fmt);

	if (logger.backend == BACKEND_FILE)
		write_file(level, fmt, ap);
	else
		vsyslog(syslog_priority[level], fmt, ap);

	va_end(ap);
}

/* Reopen the active log destination. */
int logger_reopen(void)
{
	/* syslog manages its own connection. */
	if (logger.backend == BACKEND_SYSLOG)
		return 0;

	if (logger.fp != NULL)
		fclose(logger.fp);

	return open_file();
}

/* Flush and release the active backend. */
void logger_close(void)
{
	if (logger.backend == BACKEND_SYSLOG) {
		closelog();
		return;
	}

	if (logger.fp != NULL) {
		fclose(logger.fp);
		logger.fp = NULL;
	}
}
