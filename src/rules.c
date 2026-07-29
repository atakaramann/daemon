// SPDX-License-Identifier: MIT
#include "rules.h"

#include <string.h>

/*
 * Rules occupy entries [0, count). With at most FW_RULE_MAX entries,
 * a linear search is sufficient.
 */
static struct {
	struct fw_rule rules[FW_RULE_MAX];
	size_t         count;
} table;

void rules_init(void)
{
	table.count = 0;
}

/*
 * We compare fields explicitly rather than memcmp so the result
 * never depends on padding.
 */
static int rule_equals(const struct fw_rule *a, const struct fw_rule *b)
{
	return a->src_ip == b->src_ip &&
	       a->dst_ip == b->dst_ip &&
	       a->src_port == b->src_port &&
	       a->dst_port == b->dst_port &&
	       a->protocol == b->protocol;
}

/* Returns the matching index, or table.count if no rule matches. */
static size_t find(const struct fw_rule *r)
{
	size_t i;

	for (i = 0; i < table.count; i++)
		if (rule_equals(&table.rules[i], r))
			return i;

	return table.count;
}

enum fw_status rules_add(const struct fw_rule *rule)
{
	if (find(rule) != table.count)
		return FW_STATUS_DUPLICATE;
	if (table.count >= FW_RULE_MAX)
		return FW_STATUS_FULL;

	table.rules[table.count++] = *rule;
	return FW_STATUS_OK;
}

enum fw_status rules_del(const struct fw_rule *rule)
{
	size_t i = find(rule);

	if (i == table.count)
		return FW_STATUS_NOT_FOUND;

	/*
	 * Rule order is irrelevant. Fill the hole with the last rule instead of
	 * shifting the remaining entries.
	 */
	table.rules[i] = table.rules[--table.count];
	return FW_STATUS_OK;
}

size_t rules_list(struct fw_rule *out, size_t max)
{
	size_t n = table.count < max ? table.count : max;

	memcpy(out, table.rules, n * sizeof(*out));
	return n;
}

size_t rules_count(void)
{
	return table.count;
}
