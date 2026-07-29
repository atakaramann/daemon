/* SPDX-License-Identifier: MIT */
#ifndef RULES_H
#define RULES_H

#include <stddef.h>
#include "protocol.h"

/*
 * Rules are kept private to rules.c. Callers manipulate the rule set only
 * through these helpers; duplicate rules are rejected.
 */

/* Start with an empty rule set. */
void rules_init(void);

/* Returns FW_STATUS_OK, FW_STATUS_DUPLICATE or FW_STATUS_FULL. */
enum fw_status rules_add(const struct fw_rule *rule);

/* Returns FW_STATUS_OK or FW_STATUS_NOT_FOUND. */
enum fw_status rules_del(const struct fw_rule *rule);

/*
 * Copy at most @max rules into @out.
 *
 * Returns the number copied.
 */
size_t rules_list(struct fw_rule *out, size_t max);

/* Number of stored rules. */
size_t rules_count(void);

#endif /* RULES_H */
