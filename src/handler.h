/* SPDX-License-Identifier: MIT */
#ifndef HANDLER_H
#define HANDLER_H

#include "protocol.h"

/*
 * Execute one firewall request and fill in the response.
 *
 * This is the daemon's business logic layer: it knows nothing about
 * sockets or IPC, and only translates a request into a response. Depending
 * on the command it updates the module-global rule set (add/del/list) or
 * the runtime log level (set_level). Keeping it separate from main.c lets
 * the event loop stay purely about I/O.
 */
void handler_dispatch(const struct fw_request *req, struct fw_response *resp);

#endif /* HANDLER_H */