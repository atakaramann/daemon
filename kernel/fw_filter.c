// SPDX-License-Identifier: MIT
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include "fw_filter.h"

/*
 * The active rule set, published via RCU. Hooks read it under
 * rcu_read_lock(); fw_rules_set() swaps in a new copy and frees the old
 * one after a grace period. NULL means "no rules" (default-accept).
 */
static struct fw_ruleset __rcu *fw_rules;

/*
 * Does this rule match the packet's 5-tuple? All fields are in network
 * byte order on both sides, so no conversion is needed.
 */
static bool rule_matches(const struct fw_rule *r, __be32 src, __be32 dst,
			 __be16 sport, __be16 dport, u8 proto)
{
	return r->src_ip == src &&
		   r->dst_ip == dst &&
	       r->src_port == sport && 
		   r->dst_port == dport &&
	       r->protocol == proto;
}

/*
 * Core verdict. Extracts the 5-tuple, then scans the rule set: first match
 * drops (blocklist). TCP/UDP only; anything else is accepted.
 */
static unsigned int fw_packet_verdict(struct sk_buff *skb)
{
	const struct fw_ruleset *rules;
	const struct iphdr *iph;
	__be16 sport = 0, dport = 0;
	unsigned int verdict = NF_ACCEPT;
	u32 i;

	iph = ip_hdr(skb);
	if (!iph)
		return NF_ACCEPT;

	if (iph->protocol == IPPROTO_TCP) {
		const struct tcphdr *th = tcp_hdr(skb);
		sport = th->source;
		dport = th->dest;

	} else if (iph->protocol == IPPROTO_UDP) {
		const struct udphdr *uh = udp_hdr(skb);
		sport = uh->source;
		dport = uh->dest;

	} else {
		return NF_ACCEPT;
	}

	rcu_read_lock();
	rules = rcu_dereference(fw_rules);
	if (rules) {
		for (i = 0; i < rules->count; i++) {
			if (rule_matches(&rules->rules[i], iph->saddr,
					 iph->daddr, sport, dport,
					 iph->protocol)) {
				verdict = NF_DROP;
				break;
			}
		}
	}
	rcu_read_unlock();

	return verdict;
}

static unsigned int fw_hook_in(void *priv, struct sk_buff *skb,
			       const struct nf_hook_state *state)
{
	return fw_packet_verdict(skb);
}

static const struct nf_hook_ops fw_hooks[] = {
	{
		.hook		= fw_hook_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FIRST,
	},
};

int fw_rules_set(const struct fw_ruleset *src)
{
	struct fw_ruleset *new_rules;
	struct fw_ruleset *old;

	if (src->count > FW_RULE_MAX)
		return -EINVAL;

	new_rules = kmalloc(sizeof(*new_rules), GFP_KERNEL);
	if (!new_rules)
		return -ENOMEM;

	*new_rules = *src;

	old = rcu_replace_pointer(fw_rules, new_rules, true);
	synchronize_rcu();
	kfree(old);

	return 0;
}

int fw_filter_init(void)
{
	return nf_register_net_hooks(&init_net, fw_hooks,
				     ARRAY_SIZE(fw_hooks));
}

void fw_filter_exit(void)
{
	struct fw_ruleset *old;

	nf_unregister_net_hooks(&init_net, fw_hooks, ARRAY_SIZE(fw_hooks));

	/* Hooks are gone, so no RCU readers remain; free directly. */
	old = rcu_dereference_protected(fw_rules, true);
	rcu_assign_pointer(fw_rules, NULL);
	kfree(old);
}
