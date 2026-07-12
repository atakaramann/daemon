// SPDX-License-Identifier: MIT
#define _XOPEN_SOURCE 700

#include <signal.h>
#include <string.h>

#include "signals.h"

/* Request flags: written by the handler, polled by the main loop. */
static volatile sig_atomic_t shutdown_requested;
static volatile sig_atomic_t reload_requested;

/* Only set flags here; almost nothing else is async-signal-safe. */
static void signal_handler(int signo)
{
	switch (signo) {
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

/* Register handlers for SIGTERM, SIGINT and SIGHUP. */
int signals_init(void)
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

bool signals_shutdown_requested(void)
{
	return shutdown_requested != 0;
}

bool signals_reload_requested(void)
{
	return reload_requested != 0;
}

void signals_clear_reload_request(void)
{
	reload_requested = 0;
}
