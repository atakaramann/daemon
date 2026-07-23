/* SPDX-License-Identifier: MIT */
#ifndef IPC_H
#define IPC_H

#include <sys/socket.h>
#include <sys/un.h>
#include "protocol.h"

/*
 * UNIX-domain datagram IPC. main.c and cli.c never touch the socket API
 * directly. The daemon binds the well-known path; the client binds a
 * private temporary path so the daemon has an address to reply to (a
 * datagram socket must be bound to be a reply target -- the classic
 * AF_UNIX gotcha).
 */

/*
 * Daemon: create the socket and bind FW_SOCKET_PATH with mode 0600 so only
 * root can send firewall commands. Removes a stale socket left by a
 * previous crash. Returns the fd, or -1 on error.
 */
int ipc_server_open(void);

/*
 * Client: create the socket, bind a unique private path, and connect the
 * peer to the daemon. Returns the fd, or -1 on error.
 */
int ipc_client_open(void);

/* Server: receive one request and remember who sent it, for the reply. */
int ipc_server_recv(int fd, struct fw_request *req,
		    struct sockaddr_un *from, socklen_t *fromlen);

/* Server: send one response to the address captured by ipc_server_recv. */
int ipc_server_send(int fd, const struct fw_response *resp,
		    const struct sockaddr_un *to, socklen_t tolen);

/* Client: send a request to the daemon (peer connected at open). */
int ipc_client_send(int fd, const struct fw_request *req);

/* Client: wait for the daemon's response. */
int ipc_client_recv(int fd, struct fw_response *resp);

/* Server: close the socket and unlink the well-known path. */
void ipc_server_close(int fd);

/* Client: close the socket and unlink its private bound path. */
void ipc_client_close(int fd);

#endif /* IPC_H */