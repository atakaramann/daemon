// SPDX-License-Identifier: MIT
#include "handler.h"

#include <string.h>

/*
 * One helper per command keeps handler_dispatch() focused on routing.
 * Each helper implements a single command and records the operation in
 * the log, so the daemon keeps an audit trail without mixing business
 * logic into the dispatcher.
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
	log_info("log level changed to %u", req->level);
}

void handler_dispatch(const struct fw_request *req, struct fw_response *resp)
{
	/*
	 * Clear the entire reply before filling it. This avoids leaking stale
	 * rule data or uninitialised padding.
	 */
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

	/* Push the new rule set to the kernel only when it actually changed. */
	if (resp->status == FW_STATUS_OK &&
		(req->cmd == FW_CMD_ADD_RULE || req->cmd == FW_CMD_DEL_RULE))
		kernel_sync();
}
