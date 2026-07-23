/* SPDX-License-Identifier: MIT */
#include "rules.h"

#include <string.h>

/*
 * Module-global rule set, hidden from the rest of the program. A flat
 * fixed-capacity array with a linear scan is the right size for FW_RULE_MAX
 * entries; a list or hash table would be complexity the task does not
 * need, since the task measures the IPC, not the data structure.
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
 * Two rules are equal when every match field is equal. We compare fields
 * explicitly rather than memcmp so the result never depends on padding.
 */
static int rule_equals(const struct fw_rule *a, const struct fw_rule *b)
{
	return a->src_ip == b->src_ip &&
	       a->dst_ip == b->dst_ip &&
	       a->src_port == b->src_port &&
	       a->dst_port == b->dst_port &&
	       a->protocol == b->protocol;
}

static size_t find(const struct fw_rule *r)
{
	size_t i;

	for (i = 0; i < table.count; i++)
		if (rule_equals(&table.rules[i], r))
			return i;

	return table.count;	/* not found */
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

	/* Fill the gap with the last element: order is irrelevant for a
	 * match set, so this deletes in O(1) after the find. */
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