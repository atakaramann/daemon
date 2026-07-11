// SPDX-License-Identifier: MIT
#include "daemonize.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CLOSE_GUESS 8192

int become_daemon(void)
{
	int fd;
	long maxfd, i;

	switch (fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
	}

	if (setsid() == -1)
		return -1;

	switch (fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
	}

	umask(027);

	if (chdir("/") == -1)
		return -1;

	maxfd = sysconf(_SC_OPEN_MAX);

	if (maxfd == -1)
		maxfd = MAX_CLOSE_GUESS;

	for (i = 0; i < maxfd; i++)
		close(i);

	fd = open("/dev/null", O_RDWR);

	if (fd != STDIN_FILENO)
		return -1;

	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
		return -1;

	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
		return -1;

	return 0;
}
