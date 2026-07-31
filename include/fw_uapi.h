/* SPDX-License-Identifier: MIT */
#ifndef FW_UAPI_H
#define FW_UAPI_H

/*
 * Contract shared by the kernel module and userspace (daemon + CLI): the
 * rule layout and the ioctl used to push rules into the kernel. Kept in
 * __u* types and with explicit padding so the struct is byte-identical on
 * both sides of the syscall boundary.
 */

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <linux/types.h>
#include <sys/ioctl.h>
#endif

/* Maximum rules the kernel stores and the daemon may push at once. */
#define FW_RULE_MAX 128

/*
 * One firewall rule. IPs and ports are in network byte order, matching the
 * packet headers the kernel hook inspects. protocol is an IPPROTO_* value
 * (only TCP and UDP are matched). _pad keeps the layout stable and is
 * always zeroed.
 */
struct fw_rule {
	__u32 src_ip;
	__u32 dst_ip;
	__u16 src_port;
	__u16 dst_port;
	__u8  protocol;
	__u8  _pad[3];
};

/* The full rule set pushed to the kernel in one ioctl (full replace). */
struct fw_ruleset {
	__u32 count;
	struct fw_rule rules[FW_RULE_MAX];
};

/* ioctl interface on /dev/fw. */
#define FW_IOC_MAGIC 'F'
#define FW_IOC_SET_RULES _IOW(FW_IOC_MAGIC, 1, struct fw_ruleset)

#endif /* FW_UAPI_H */
