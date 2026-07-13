// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "daemonize.h"
#include "logging.h"

#define DEFAULT_LOG_DEST  "syslog"
#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

/* Request flags: written by the handler, polled by the main loop. */
static volatile sig_atomic_t shutdown_requested;
static volatile sig_atomic_t reload_requested;

/* Only set flags here; almost nothing else is async-signal-safe. */
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		/* Request a clean shutdown. */
		shutdown_requested = 1;
		break;
	case SIGHUP:
		/* Request log reopening. */
		reload_requested = 1;
		break;
	}
}

/* Install the signal handlers used by this daemon. */
static int signals_init(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	/* Restart interrupted system calls automatically. */
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGTERM, &sa, NULL) == -1)
		return -1;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		return -1;
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		return -1;

	return 0;
}

/* Print command-line usage information. */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-h] [-o <syslog|logfile>] [-l <0|1|2>]\n"
		"\n"
		"  -h          Show this help message\n"
		"  -o <dest>   Logging destination (syslog or file path)\n"
		"  -l <level>  Log level: 0=ERROR, 1=INFO, 2=DEBUG\n",
		prog);
}

/* Convert the -l argument into an internal log level. */
static int parse_level(const char *arg, enum log_level *level)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(arg, &end, 10);

	/* Reject invalid or out-of-range values. */
	if (errno != 0 || end == arg || *end != '\0')
		return -1;
	if (value < LOG_LEVEL_ERROR || value > LOG_LEVEL_DEBUG)
		return -1;

	*level = (enum log_level)value;
	return 0;
}

/*
 * Resolve the logging destination.
 * Relative file paths are converted to absolute paths before daemonization.
 */
static int resolve_log_path(const char *input, char *output, size_t size)
{
	char cwd[PATH_MAX];

	/* Keep the special syslog backend unchanged. */
	if (strcmp(input, "syslog") == 0) {
		if (snprintf(output, size, "%s", input) >= (int)size)
			return -1;
		return 0;
	}

	/* Absolute paths require no further processing. */
	if (input[0] == '/') {
		if (snprintf(output, size, "%s", input) >= (int)size)
			return -1;
		return 0;
	}

	/* Convert relative paths before the daemon switches to "/". */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return -1;
	if (snprintf(output, size, "%s/%s", cwd, input) >= (int)size)
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	char log_dest[PATH_MAX] = DEFAULT_LOG_DEST;
	enum log_level log_level = DEFAULT_LOG_LEVEL;
	int opt;

	/* Parse command-line options. */
	while ((opt = getopt(argc, argv, "ho:l:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
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

	/* Reject unexpected positional arguments. */
	if (optind < argc) {
		fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (become_daemon() == -1) {
		perror("become_daemon");
		return EXIT_FAILURE;
	}

	if (signals_init() == -1)
		return EXIT_FAILURE;

	if (logger_init(log_dest, log_level) == -1)
		return EXIT_FAILURE;

	log_info("Daemon started (level=%d)", log_level);

	/* Wait for signals until shutdown is requested. */
	while (!shutdown_requested) {
		pause();

		if (reload_requested) {
			log_info("SIGHUP received, reloading");
			if (logger_reopen() == -1) {
				log_error("Failed to reopen log");
				break;
			}
			reload_requested = 0;
			log_info("Reload completed");
		}
	}

	log_info("Daemon shutting down");
	logger_close();

	return EXIT_SUCCESS;
}
