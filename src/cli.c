// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipc.h"
#include "protocol.h"

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s -A \"SRC_IP:DST_IP:SPORT:DPORT:PROTO\"   add a rule\n"
		"  %s -D \"SRC_IP:DST_IP:SPORT:DPORT:PROTO\"   delete a rule\n"
		"  %s -d                                       list all rules\n"
		"  %s -l <0|1|2>                               set log level\n"
		"  %s -h                                       show this help\n"
		"\n"
		"PROTO is tcp or udp.\n",
		prog, prog, prog, prog, prog);
}

static int parse_protocol(const char *str, uint8_t *proto)
{
	if (strcmp(str, "tcp") == 0) {
		*proto = IPPROTO_TCP;
		return 0;
	}
	if (strcmp(str, "udp") == 0) {
		*proto = IPPROTO_UDP;
		return 0;
	}
	return -1;
}

/* strtol, not atoi, so junk and out-of-range values are rejected. */
static int parse_port(const char *str, uint16_t *port)
{
	char *end;
	long  v;

	errno = 0;
	v = strtol(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0' || v < 0 || v > UINT16_MAX)
		return -1;

	*port = (uint16_t)v;
	return 0;
}

/*
 * Parse "SRC_IP:DST_IP:SPORT:DPORT:PROTO" into a fw_rule. IPs go in via
 * inet_pton, which already produces network byte order.
 */
static int parse_rule(const char *text, struct fw_rule *rule)
{
	char *copy;
	char *token, *save;

	memset(rule, 0, sizeof(*rule));

	/* strtok_r writes into its input, so work on a copy of argv. */
	copy = strdup(text);
	if (copy == NULL)
		return -1;

	token = strtok_r(copy, ":", &save);
	if (token == NULL || inet_pton(AF_INET, token, &rule->src_ip) != 1)
		goto fail;

	token = strtok_r(NULL, ":", &save);
	if (token == NULL || inet_pton(AF_INET, token, &rule->dst_ip) != 1)
		goto fail;

	token = strtok_r(NULL, ":", &save);
	if (token == NULL || parse_port(token, &rule->src_port) == -1)
		goto fail;

	token = strtok_r(NULL, ":", &save);
	if (token == NULL || parse_port(token, &rule->dst_port) == -1)
		goto fail;

	token = strtok_r(NULL, ":", &save);
	if (token == NULL || parse_protocol(token, &rule->protocol) == -1)
		goto fail;

	/* There must be no sixth field. */
	if (strtok_r(NULL, ":", &save) != NULL)
		goto fail;

	free(copy);
	return 0;

fail:
	free(copy);
	return -1;
}

static const char *fw_status_str(uint32_t status)
{
	switch (status) {
	case FW_STATUS_OK:        return "ok";
	case FW_STATUS_ERROR:     return "error";
	case FW_STATUS_FULL:      return "rule table full";
	case FW_STATUS_DUPLICATE: return "rule already exists";
	case FW_STATUS_NOT_FOUND: return "rule not found";
	case FW_STATUS_BAD_MSG:   return "malformed request";
	default:                  return "unknown status";
	}
}

static const char *fw_proto_str(uint8_t proto)
{
	if (proto == IPPROTO_TCP)
		return "tcp";
	if (proto == IPPROTO_UDP)
		return "udp";
	return "unknown";
}

static void print_rules(const struct fw_response *resp)
{
	uint32_t i;

	if (resp->count == 0) {
		printf("no rules\n");
		return;
	}

	printf("%-15s %-15s %-6s %-6s %s\n",
	       "SRC", "DST", "SPORT", "DPORT", "PROTO");
	for (i = 0; i < resp->count; i++) {
		const struct fw_rule *r = &resp->rules[i];
		char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &r->src_ip, src, sizeof(src));
		inet_ntop(AF_INET, &r->dst_ip, dst, sizeof(dst));

		printf("%-15s %-15s %-6u %-6u %s\n",
		       src, dst, r->src_port, r->dst_port,
		       fw_proto_str(r->protocol));
	}
}

/* argv -> request. Returns 0, 1 for -h (nothing to send), -1 on error. */
static int build_request(int argc, char *argv[], struct fw_request *req)
{
	int opt;
	int commands = 0;

	memset(req, 0, sizeof(*req));

	while ((opt = getopt(argc, argv, "A:D:l:dh")) != -1) {
		if (opt == 'h')		/* -h is a help request, not a command */
			return 1;

		commands++;

		switch (opt) {
		case 'A':
			req->cmd = FW_CMD_ADD_RULE;
			if (parse_rule(optarg, &req->rule) == -1) {
				fprintf(stderr, "%s: invalid rule: %s\n", argv[0], optarg);
				return -1;
			}
			break;
		case 'D':
			req->cmd = FW_CMD_DEL_RULE;
			if (parse_rule(optarg, &req->rule) == -1) {
				fprintf(stderr, "%s: invalid rule: %s\n", argv[0], optarg);
				return -1;
			}
			break;
		case 'd':
			req->cmd = FW_CMD_LIST_RULES;
			break;
		case 'l': {
			char *end;
			long  v;

			errno = 0;
			v = strtol(optarg, &end, 10);
			if (errno != 0 || end == optarg || *end != '\0' ||
			    v < LOG_LEVEL_ERROR || v > LOG_LEVEL_DEBUG) {
				fprintf(stderr, "%s: invalid level: %s\n", argv[0], optarg);
				return -1;
			}
			req->cmd = FW_CMD_SET_LEVEL;
			req->level = (uint32_t)v;
			break;
		}
		default:
			return -1;
		}
	}

	/* Exactly one command, and no leftover positional arguments. */
	if (commands != 1 || optind < argc)
		return -1;

	return 0;
}

/*
 * Open a connection, send req, receive resp, close. Returns 0 on success
 * or -1 with a message already printed to stderr.
 */
static int transact(const char *prog, const struct fw_request *req,
		    struct fw_response *resp)
{
	int fd;

	fd = ipc_client_open();
	if (fd == -1) {
		if (errno == ECONNREFUSED)
			fprintf(stderr, "%s: daemon is not running\n", prog);
		else
			fprintf(stderr, "%s: cannot reach daemon: %s\n",
				prog, strerror(errno));
		return -1;
	}

	if (ipc_client_send(fd, req) == -1) {
		fprintf(stderr, "%s: send failed: %s\n", prog, strerror(errno));
		ipc_client_close(fd);
		return -1;
	}

	if (ipc_client_recv(fd, resp) == -1) {
		/* SO_RCVTIMEO expiry shows up as EAGAIN/EWOULDBLOCK. */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			fprintf(stderr, "%s: daemon did not respond\n", prog);
		else
			fprintf(stderr, "%s: no reply: %s\n",
				prog, strerror(errno));
		ipc_client_close(fd);
		return -1;
	}

	ipc_client_close(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	struct fw_request  req;
	struct fw_response resp;
	int                r;

	r = build_request(argc, argv, &req);
	if (r == 1) {			/* -h */
		usage(argv[0]);
		return EXIT_SUCCESS;
	}
	if (r == -1) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (transact(argv[0], &req, &resp) == -1)
		return EXIT_FAILURE;

	if (resp.status != FW_STATUS_OK) {
		fprintf(stderr, "%s: %s\n", argv[0], fw_status_str(resp.status));
		return EXIT_FAILURE;
	}

	/* Success is silent except for LIST, which is all output. */
	if (req.cmd == FW_CMD_LIST_RULES)
		print_rules(&resp);

	return EXIT_SUCCESS;
}
