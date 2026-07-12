// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "daemonize.h"
#include "logging.h"
#include "signals.h"

#define DEFAULT_LOG_DEST  "syslog"
#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-o <syslog|logfile>] [-l <0|1|2>]\n"
		"\n"
		"  -o <dest>   Logging destination (syslog or file path)\n"
		"  -l <level>  Log level: 0=ERROR, 1=INFO, 2=DEBUG\n",
		prog);
}

static int parse_level(const char *arg, enum log_level *level)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(arg, &end, 10);

	if (errno != 0 || end == arg || *end != '\0')
		return -1;
	if (value < LOG_LEVEL_ERROR || value > LOG_LEVEL_DEBUG)
		return -1;

	*level = (enum log_level)value;
	return 0;
}

static int resolve_log_path(const char *input, char *output, size_t size)
{
	char cwd[PATH_MAX];

	if (strcmp(input, "syslog") == 0) {
		if (snprintf(output, size, "%s", input) >= (int)size)
			return -1;
		return 0;
	}

	if (input[0] == '/') {
		if (snprintf(output, size, "%s", input) >= (int)size)
			return -1;
		return 0;
	}

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return -1;
	if (snprintf(output, size, "%s/%s", cwd, input) >= (int)size)
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	char           log_dest[PATH_MAX] = DEFAULT_LOG_DEST;
	enum log_level log_level = DEFAULT_LOG_LEVEL;
	int            opt;

	while ((opt = getopt(argc, argv, "o:l:")) != -1) {
		switch (opt) {
		case 'o':
			if (resolve_log_path(optarg, log_dest,
					     sizeof(log_dest)) < 0) {
				fprintf(stderr, "Invalid log destination\n");
				return EXIT_FAILURE;
			}
			break;
		case 'l':
			if (parse_level(optarg, &log_level) < 0) {
				fprintf(stderr, "Invalid log level\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (become_daemon() == -1) {
		perror("become_daemon");
		return EXIT_FAILURE;
	}

	/*
	 * Install signal handlers before opening the log, so a signal that
	 * arrives during startup is not lost. There is no terminal and no
	 * log yet, so a failure here can only exit.
	 */
	if (signals_init() == -1)
		return EXIT_FAILURE;

	/*
	 * Open the log. openlog()/fopen() run here, after become_daemon()
	 * has closed the inherited file descriptors.
	 */
	if (logger_init(log_dest, log_level) == -1)
		return EXIT_FAILURE;

	log_info("Daemon started (level=%d)", log_level);

	/*
	 * Event loop. There is no periodic work, so pause() sleeps at zero
	 * CPU until a signal arrives. pause() always returns on a caught
	 * signal, even with SA_RESTART, so we then inspect the flags. All
	 * real work happens here, never in a handler: fprintf()/vsyslog()
	 * are not async-signal-safe, so the handlers only set a flag.
	 */
	while (!signals_shutdown_requested()) {
		pause();

		if (signals_reload_requested()) {
			log_info("SIGHUP received, reloading");
			if (logger_reopen() == -1) {
				log_error("Failed to reopen log");
				break;	/* single cleanup point is below */
			}
			signals_clear_reload_request();
			log_info("Reload completed");
		}
	}

	/* Graceful shutdown: record the exit, then release the log sink. */
	log_info("Daemon shutting down");
	logger_close();

	return EXIT_SUCCESS;
}
