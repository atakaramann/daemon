// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include "kernel.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fw_uapi.h"
#include "logging.h"

#define FW_DEVICE_PATH "/dev/fw"

/* -1 until kernel_open() succeeds; sync is a no-op while it is -1. */
static int kernel_fd = -1;

int kernel_open(void)
{
	kernel_fd = open(FW_DEVICE_PATH, O_WRONLY | O_CLOEXEC);
	if (kernel_fd == -1)
		return -1;

	return 0;
}

int kernel_sync(void)
{
	struct fw_ruleset set;

	if (kernel_fd == -1)
		return 0;

	memset(&set, 0, sizeof(set));
	set.count = rules_list(set.rules, FW_RULE_MAX);

	if (ioctl(kernel_fd, FW_IOC_SET_RULES, &set) == -1) {
		log_error("kernel sync failed: %s", strerror(errno));
		return -1;
	}

	return 0;
}

void kernel_close(void)
{
	if (kernel_fd != -1) {
		close(kernel_fd);
		kernel_fd = -1;
	}
}
