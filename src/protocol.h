/* SPDX-License-Identifier: MIT */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * Wire protocol shared by fwd (daemon) and fwctl (client), exchanged over
 * an AF_UNIX SOCK_DGRAM socket: one request per datagram, one reply per
 * datagram. Fixed-width types keep the layout identical on both sides.
 */

/* Daemon's control socket; filesystem path so it can be chmod'd 0600. */
#define FW_SOCKET_PATH  "/run/fwd.sock"

/* Template for the client's private reply socket. */
#define FW_CLIENT_FMT   "/tmp/fwctl.%ld"

/* Rule capacity; also bounds a LIST reply to one datagram. */
#define FW_RULE_MAX  128

/*
 * Part of the wire protocol: SET_LEVEL carries one of these values in
 * struct fw_request.
 */
enum log_level {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_INFO  = 1,
	LOG_LEVEL_DEBUG = 2,
};

/* Values start at 1 so a zeroed request reads as an invalid command. */
enum fw_cmd {
	FW_CMD_SET_LEVEL  = 1,
	FW_CMD_ADD_RULE   = 2,
	FW_CMD_DEL_RULE   = 3,
	FW_CMD_LIST_RULES = 4,
};

/* Status codes returned in every daemon reply. */
enum fw_status {
	FW_STATUS_OK        = 0,
	FW_STATUS_ERROR     = 1,
	FW_STATUS_FULL      = 2,
	FW_STATUS_DUPLICATE = 3,
	FW_STATUS_NOT_FOUND = 4,
	FW_STATUS_BAD_MSG   = 5,
};

/*
 * A rule: IP1:IP2:sport:dport:protocol. IPs are in network byte order
 * (as inet_pton/inet_ntop use them); ports are host order. protocol is
 * an IPPROTO_* value.
 */
struct fw_rule {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t  protocol;
	uint8_t  _pad[3];	/* explicit padding, zeroed for a stable layout */
};

/* cmd determines which union member is valid. */
struct fw_request {
	uint32_t cmd;
	union {
		struct fw_rule rule;	/* ADD_RULE / DEL_RULE */
		uint32_t       level;	/* SET_LEVEL */
	};
};

/*
 * status is valid for every reply.
 * rules[] and count are used only by LIST_RULES.
 */
struct fw_response {
	uint32_t status;
	uint32_t count;
	struct fw_rule rules[FW_RULE_MAX];
};

#endif /* PROTOCOL_H */
