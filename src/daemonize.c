// SPDX-License-Identifier: MIT
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "daemonize.h"

#define MAX_CLOSE_GUESS 8192

int become_daemon(void)
{
	int fd;
	long maxfd, i;

	/* First fork: the parent exits and the shell gets its prompt back. */
	switch (fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
	}

	/* Create a new session with no controlling terminal. */
	if (setsid() == -1)
		return -1;

	/* Second fork: a non-session-leader can never regain a terminal. */
	switch (fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
	}

	/* Set secure default file permissions. */
	umask(027);

	/* Run from / so the daemon never pins a mount point. */
	if (chdir("/") == -1)
		return -1;

	/* Close every descriptor inherited from the parent. */
	maxfd = sysconf(_SC_OPEN_MAX);

	if (maxfd == -1)
		maxfd = MAX_CLOSE_GUESS;

	for (i = 0; i < maxfd; i++)
		close(i);

	/* Point stdin at /dev/null; with all fds closed, open() gives 0. */
	fd = open("/dev/null", O_RDWR);

	if (fd != STDIN_FILENO)
		return -1;

	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
		return -1;

	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
		return -1;

	return 0;
}
