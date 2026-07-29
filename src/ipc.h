/* SPDX-License-Identifier: MIT */
#ifndef IPC_H
#define IPC_H

#include <sys/socket.h>
#include <sys/un.h>
#include "protocol.h"

/*
 * UNIX-domain datagram transport. Callers use this interface instead of
 * dealing with the socket API directly.
 */

/* Create and bind the daemon control socket. */
int ipc_server_open(void);

/* Create a client socket connected to the daemon. */
int ipc_client_open(void);

/* Receive one request and record the sender's address. */
int ipc_server_recv(int fd, struct fw_request *req,
		    struct sockaddr_un *from, socklen_t *fromlen);

/* Send one reply to the recorded sender. */
int ipc_server_send(int fd, const struct fw_response *resp,
		    const struct sockaddr_un *to, socklen_t tolen);

/* Send one request to the daemon. */
int ipc_client_send(int fd, const struct fw_request *req);

/* Receive one reply from the daemon. */
int ipc_client_recv(int fd, struct fw_response *resp);

/* Close the socket and remove its bound pathname. */
void ipc_server_close(int fd);
void ipc_client_close(int fd);

#endif /* IPC_H */
