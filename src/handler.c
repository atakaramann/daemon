/* SPDX-License-Identifier: MIT */
#include "handler.h"

#include <string.h>

#include "logging.h"
#include "rules.h"

/*
 * One small handler per command keeps handler_dispatch() a pure routing
 * table: the switch says which command maps to which handler, and each
 * handler owns exactly one command's logic. Every rule change is logged,
 * so the daemon leaves an audit trail even though it has no terminal.
 */

static void handle_add(const struct fw_request *req, struct fw_response *resp)
{
	resp->status = rules_add(&req->rule);
	log_info("add rule -> status %u", resp->status);
}

static void handle_del(const struct fw_request *req, struct fw_response *resp)
{
	resp->status = rules_del(&req->rule);
	log_info("del rule -> status %u", resp->status);
}

static void handle_list(struct fw_response *resp)
{
	resp->count = (uint32_t)rules_list(resp->rules, FW_RULE_MAX);
	resp->status = FW_STATUS_OK;
	log_debug("list rules -> %u entries", resp->count);
}

static void handle_set_level(const struct fw_request *req,
			     struct fw_response *resp)
{
	if (logger_set_level((enum log_level)req->level) == -1) {
		resp->status = FW_STATUS_ERROR;
		log_error("set_level: bad level %u", req->level);
		return;
	}

	resp->status = FW_STATUS_OK;
}

void handler_dispatch(const struct fw_request *req, struct fw_response *resp)
{
	/* Zero the whole response so no uninitialised padding or stale rule
	 * slots leak back to the client. */
	memset(resp, 0, sizeof(*resp));

	switch (req->cmd) {
	case FW_CMD_ADD_RULE:
		handle_add(req, resp);
		break;
	case FW_CMD_DEL_RULE:
		handle_del(req, resp);
		break;
	case FW_CMD_LIST_RULES:
		handle_list(resp);
		break;
	case FW_CMD_SET_LEVEL:
		handle_set_level(req, resp);
		break;
	default:
		resp->status = FW_STATUS_BAD_MSG;
		log_error("unknown command %u", req->cmd);
		break;
	}
}