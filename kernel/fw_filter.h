/* SPDX-License-Identifier: MIT */
#ifndef FW_FILTER_H
#define FW_FILTER_H

#include "fw_uapi.h"

/*
 * Data plane: the netfilter hooks and the RCU-protected rule set they read.
 * The control plane (ioctl) only needs to register/unregister the hooks and
 * replace the rule set; everything else stays private to fw_filter.c.
 */

/* Register the LOCAL_IN hook. Returns 0 or a negative errno. */
int fw_filter_init(void);

/* Unregister the hook and free the current rule set. */
void fw_filter_exit(void);

/*
 * Atomically replace the active rule set with a copy of 'src'
 * (src->count rules). Safe against concurrent Netfilter readers.
 * Returns 0 or a negative errno.
 */
int fw_rules_set(const struct fw_ruleset *src);

#endif /* FW_FILTER_H */
