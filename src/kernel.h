/* SPDX-License-Identifier: MIT */
#ifndef KERNEL_H
#define KERNEL_H

#include "rules.h"

/*
 * Talks to the fw kernel module over /dev/fw. The daemon pushes the full
 * rule set after every change; the module applies it in its netfilter
 * hooks. If the module is not loaded the daemon still runs (degraded): the
 * open fails, and sync becomes a no-op.
 */

/* Open /dev/fw. Returns 0 on success, -1 if the module is not available. */
int kernel_open(void);

/* Push the current rule set to the kernel. No-op if /dev/fw is not open. */
int kernel_sync(void);

/* Close /dev/fw if it was open. */
void kernel_close(void);

#endif /* KERNEL_H */
