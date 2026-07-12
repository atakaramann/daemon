// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <limits.h>

enum log_backend {
	BACKEND_SYSLOG,
	BACKEND_FILE,
};

static struct {
	enum log_backend backend;
	enum log_level   level;      /* threshold from -l */
	FILE            *fp;         /* file backend only */
	char             path[PATH_MAX]; /* absolute path, kept for reopen() */
} logger;

static const int syslog_priority[] = {
	[LOG_LEVEL_ERROR] = LOG_ERR,
	[LOG_LEVEL_INFO]  = LOG_INFO,
	[LOG_LEVEL_DEBUG] = LOG_DEBUG,
};

/* Human-readable tag for the file backend. */
static const char *level_name[] = {
	[LOG_LEVEL_ERROR] = "ERROR",
	[LOG_LEVEL_INFO]  = "INFO",
	[LOG_LEVEL_DEBUG] = "DEBUG",
};

static bool should_log(enum log_level level)
{
	return level <= logger.level;
}

static int open_file(void)
{
	/* Append, never truncate: on reopen()/rotation we must not wipe an
	 * existing log. */
	logger.fp = fopen(logger.path, "a");
	if (logger.fp == NULL)
		return -1;

	return 0;
}

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

	/* snprintf both copies and null-terminates; a return >= size means
	 * the path was too long and got truncated, which we reject rather
	 * than log to the wrong file. */
	n = snprintf(logger.path, sizeof(logger.path), "%s", dest);
	if (n < 0 || (size_t)n >= sizeof(logger.path))
		return -1;

	return open_file();
}

static void write_file(enum log_level level, const char *fmt, va_list ap)
{
	char      ts[32];
	time_t    now = time(NULL);
	struct tm tm;

	/* localtime_r: thread-safe variant, honest about a firewall daemon
	 * possibly growing threads later. */
	localtime_r(&now, &tm);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

	/* We prepend timestamp + level + pid ourselves, because a raw file
	 * has nobody else to add them (unlike syslogd). */
	fprintf(logger.fp, "%s [%s] pid=%d: ", ts, level_name[level],
		(int)getpid());
	vfprintf(logger.fp, fmt, ap);
	fputc('\n', logger.fp);

	/* _IOLBF is not guaranteed for regular files, so flush explicitly
	 * instead of trusting line buffering. */
	fflush(logger.fp);
}

void logger_write(enum log_level level, const char *fmt, ...)
{
	va_list ap;

	/* Guard the table index before anything else: an out-of-range level
	 * would read past syslog_priority[]/level_name[] (undefined). */
	if (level < LOG_LEVEL_ERROR || level > LOG_LEVEL_DEBUG)
		return;

	if (!should_log(level))     /* the one gate; both backends respect it */
		return;

	va_start(ap, fmt);

	if (logger.backend == BACKEND_FILE)
		write_file(level, fmt, ap);
	else
		vsyslog(syslog_priority[level], fmt, ap);

	va_end(ap);
}

int logger_reopen(void)
{
	/* syslog keeps its socket open; there is nothing to rotate. */
	if (logger.backend == BACKEND_SYSLOG)
		return 0;

	if (logger.fp != NULL)
		fclose(logger.fp);

	return open_file();     /* recreates the file if it was rotated away */
}

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
