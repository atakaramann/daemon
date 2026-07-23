/* SPDX-License-Identifier: MIT */
#ifndef RULES_H
#define RULES_H

#include <stddef.h>
#include "protocol.h"

/*
 * Firewall rule container.
 *
 * The rule set is module-global state, hidden inside rules.c behind these
 * accessors -- the same encapsulation approach as signals.c and logging.c.
 * Callers never see the storage, so the table's internals (array, count)
 * cannot leak into handler.c. There is exactly one firewall rule set, so a
 * single hidden table is the right model; a caller-owned struct would only
 * add an instance flexibility this daemon never uses.
 *
 * The container's real job is uniqueness: add refuses an identical rule.
 */

/* Clear all stored rules (called once at startup). */
void rules_init(void);

/* Returns FW_STATUS_OK, FW_STATUS_DUPLICATE or FW_STATUS_FULL. */
enum fw_status rules_add(const struct fw_rule *rule);

/* Returns FW_STATUS_OK or FW_STATUS_NOT_FOUND. */
enum fw_status rules_del(const struct fw_rule *rule);

/*
 * Copy up to 'max' rules into 'out'. Returns the number actually copied.
 * This keeps the array layout inside rules.c: handler.c asks for a copy
 * instead of reaching into the table.
 */
size_t rules_list(struct fw_rule *out, size_t max);

/* Number of stored rules. */
size_t rules_count(void);

#endif /* RULES_H */