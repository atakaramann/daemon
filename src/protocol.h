/* SPDX-License-Identifier: MIT */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * Wire protocol shared by fwd (daemon) and fwctl (client).
 *
 * Transport is an AF_UNIX SOCK_DGRAM socket: message boundaries are
 * preserved by the kernel and, unlike UDP, local datagrams are never
 * lost, reordered or duplicated. One request == one datagram, one
 * reply == one datagram, so we need no framing of our own.
 *
 * The CLI parses user input and fills these structs; the daemon never
 * parses strings. Parsing lives on the CLI side because only the CLI has
 * a terminal to report errors to.
 *
 * Fixed-width types (uint32_t, not enum/int) keep the layout identical on
 * both sides regardless of compiler, and make the format reusable if it
 * is later shared with a kernel module.
 */

/* Daemon's well-known control socket (filesystem, so it can be chmod'd). */
#define FW_SOCKET_PATH  "/run/fwd.sock"

/* Client binds a private path so the daemon has somewhere to reply. */
#define FW_CLIENT_FMT   "/tmp/fwctl.%ld"

/* Max rules stored; also bounds a LIST reply to a single datagram. */
#define FW_RULE_MAX  128

/*
 * Log level. Defined here, not in logging.h, because FW_CMD_SET_LEVEL
 * carries it over the wire (req.level), so it is part of the protocol
 * that both the daemon and the CLI share.
 */
enum log_level {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_INFO  = 1,
	LOG_LEVEL_DEBUG = 2,
};

enum fw_cmd {
	FW_CMD_SET_LEVEL  = 1,	/* change log level at runtime */
	FW_CMD_ADD_RULE   = 2,
	FW_CMD_DEL_RULE   = 3,
	FW_CMD_LIST_RULES = 4,
};

enum fw_status {
	FW_STATUS_OK        = 0,
	FW_STATUS_ERROR     = 1,	/* generic / unknown command */
	FW_STATUS_FULL      = 2,	/* rule table is full */
	FW_STATUS_DUPLICATE = 3,	/* rule already exists */
	FW_STATUS_NOT_FOUND = 4,	/* rule to delete does not exist */
	FW_STATUS_BAD_MSG   = 5,	/* malformed request */
};

/*
 * A firewall rule: IP1:IP2:sport:dport:protocol. IP addresses are stored
 * in network byte order, which is exactly what inet_pton() produces and
 * inet_ntop() consumes, so the CLI needs no ntoh/hton conversion. Ports
 * are host-order integers. Storing IPs as integers (not strings) makes
 * deduplication reliable: "192.168.1.1" and "192.168.001.1" collapse to
 * the same value, so a duplicate is really caught. Byte order does not
 * affect dedup, which only tests equality. protocol is an IPPROTO_* value.
 */
struct fw_rule {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t  protocol;
	uint8_t  _pad[3];	/* explicit padding: deterministic layout */
};

/*
 * CLI -> daemon. A command carries EITHER a rule (add/del) OR a level
 * (set_level), never both, so they share storage via an anonymous union.
 */
struct fw_request {
	uint32_t cmd;		/* enum fw_cmd */
	union {
		struct fw_rule rule;	/* FW_CMD_ADD_RULE / FW_CMD_DEL_RULE */
		uint32_t       level;	/* FW_CMD_SET_LEVEL */
	};
};

/*
 * daemon -> CLI. For FW_CMD_LIST_RULES, 'count' rules are returned in the
 * array; for every other command 'status' carries the result and count
 * is 0. A fixed array lets the whole rule set travel in one datagram.
 */
struct fw_response {
	uint32_t status;	/* enum fw_status */
	uint32_t count;		/* valid entries in rules[] */
	struct fw_rule rules[FW_RULE_MAX];
};

#endif /* PROTOCOL_H */
