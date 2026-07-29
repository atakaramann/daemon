/* SPDX-License-Identifier: MIT */
#ifndef HANDLER_H
#define HANDLER_H

#include "protocol.h"

/*
 * Execute one request and populate the response. Command handlers update
 * the rule table or runtime log level without depending on the IPC layer.
 */
void handler_dispatch(const struct fw_request *req, struct fw_response *resp);

#endif /* HANDLER_H */
