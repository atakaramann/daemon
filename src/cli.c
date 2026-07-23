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
		"\n"
		"PROTO is tcp or udp.\n",
		prog, prog, prog, prog);
}

/* Map a protocol name to its IPPROTO_* value. */
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

/* Parse a decimal port (0..65535). strtol, not atoi, so junk and
 * out-of-range values are rejected instead of silently truncated. */
static int parse_port(const char *str, uint16_t *port)
{
	char *end;
	long  v;

	errno = 0;
	v = strtol(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0' || v < 0 || v > 65535)
		return -1;

	*port = (uint16_t)v;
	return 0;
}

/*
 * Parse "SRC_IP:DST_IP:SPORT:DPORT:PROTO" into a fw_rule. All parsing and
 * validation happens on the client, the side with a terminal to report
 * errors to. IPs are kept in network byte order: inet_pton produces that
 * directly and inet_ntop consumes it, so no ntoh/hton is needed here.
 */
static int parse_rule(const char *text, struct fw_rule *rule)
{
	char *copy;
	char *tok, *save;

	memset(rule, 0, sizeof(*rule));

	copy = strdup(text);
	if (copy == NULL)
		return -1;

	tok = strtok_r(copy, ":", &save);
	if (tok == NULL || inet_pton(AF_INET, tok, &rule->src_ip) != 1)
		goto fail;

	tok = strtok_r(NULL, ":", &save);
	if (tok == NULL || inet_pton(AF_INET, tok, &rule->dst_ip) != 1)
		goto fail;

	tok = strtok_r(NULL, ":", &save);
	if (tok == NULL || parse_port(tok, &rule->src_port) == -1)
		goto fail;

	tok = strtok_r(NULL, ":", &save);
	if (tok == NULL || parse_port(tok, &rule->dst_port) == -1)
		goto fail;

	tok = strtok_r(NULL, ":", &save);
	if (tok == NULL || parse_protocol(tok, &rule->protocol) == -1)
		goto fail;

	/* Reject trailing junk: there must be no sixth field. */
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
	return "?";
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

/* Build the request from argv. Returns 0 on success, -1 on usage error. */
static int build_request(int argc, char *argv[], struct fw_request *req)
{
	int opt;

	memset(req, 0, sizeof(*req));

	opt = getopt(argc, argv, "A:D:dl:");
	if (opt == -1)
		return -1;

	switch (opt) {
	case 'A':
		req->cmd = FW_CMD_ADD_RULE;
		if (parse_rule(optarg, &req->rule) == -1) {
			fprintf(stderr, "Invalid rule: %s\n", optarg);
			return -1;
		}
		break;
	case 'D':
		req->cmd = FW_CMD_DEL_RULE;
		if (parse_rule(optarg, &req->rule) == -1) {
			fprintf(stderr, "Invalid rule: %s\n", optarg);
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
			fprintf(stderr, "Invalid level: %s\n", optarg);
			return -1;
		}
		req->cmd = FW_CMD_SET_LEVEL;
		req->level = (uint32_t)v;
		break;
	}
	default:
		return -1;
	}

	/* One command per invocation: reject a second option. */
	if (getopt(argc, argv, "A:D:dl:") != -1)
		return -1;

	/* Reject leftover non-option arguments, e.g. "fwctl -d junk". */
	if (optind < argc)
		return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	struct fw_request  req;
	struct fw_response resp;
	int                fd;

	if (build_request(argc, argv, &req) == -1) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	fd = ipc_client_open();
	if (fd == -1) {
		fprintf(stderr, "Cannot reach daemon (is fwd running?): %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}

	if (ipc_client_send(fd, &req) == -1) {
		fprintf(stderr, "send failed: %s\n", strerror(errno));
		ipc_client_close(fd);
		return EXIT_FAILURE;
	}

	if (ipc_client_recv(fd, &resp) == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			fprintf(stderr,
				"daemon did not respond (is fwd running?)\n");
		else
			fprintf(stderr, "no reply: %s\n", strerror(errno));
		ipc_client_close(fd);
		return EXIT_FAILURE;
	}

	ipc_client_close(fd);

	if (req.cmd == FW_CMD_LIST_RULES)
		print_rules(&resp);
	else
		printf("%s\n", fw_status_str(resp.status));

	return (resp.status == FW_STATUS_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}