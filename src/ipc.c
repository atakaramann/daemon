/* SPDX-License-Identifier: MIT */
#include "ipc.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

/*
 * How long the client waits for the daemon's reply. A CLI tool must never
 * hang forever: if the daemon is down (or a stale socket file survives a
 * crash), recv() would otherwise block indefinitely.
 */
#define CLIENT_TIMEOUT_SECS 3

/* The client's private bound path, remembered so we can unlink it. */
static char client_path[sizeof(((struct sockaddr_un *)0)->sun_path)];

/*
 * Fill a sockaddr_un for 'path'. snprintf (not strcpy) both copies and
 * bounds the write, and reports truncation via its return value, so this
 * stays correct even if 'path' ever comes from argv or a config file
 * rather than a compile-time macro. Returns 0, or -1 if the path is too
 * long for sun_path.
 */
static int fill_sockaddr(struct sockaddr_un *addr, const char *path)
{
	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	if (snprintf(addr->sun_path, sizeof(addr->sun_path), "%s", path)
	    >= (int)sizeof(addr->sun_path))
		return -1;

	return 0;
}

int ipc_server_open(void)
{
	int fd;
	struct sockaddr_un addr;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return -1;

	/* A leftover socket file would make bind() fail with EADDRINUSE, so
	 * remove it first. ENOENT is fine. */
	if (unlink(FW_SOCKET_PATH) == -1 && errno != ENOENT) {
		close(fd);
		return -1;
	}

	if (fill_sockaddr(&addr, FW_SOCKET_PATH) == -1) {
		close(fd);
		return -1;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}

	/* Firewall control must be root-only: no group/other access. */
	if (chmod(FW_SOCKET_PATH, S_IRUSR | S_IWUSR) == -1) {
		close(fd);
		unlink(FW_SOCKET_PATH);
		return -1;
	}

	return fd;
}

int ipc_client_open(void)
{
	int fd;
	int saved_errno;
	struct sockaddr_un addr;
	char path[sizeof(addr.sun_path)];
	struct timeval tv;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return -1;

	/* Bind a unique private path so the daemon can reply to us. The pid
	 * keeps it unique across concurrent CLI invocations. */
	snprintf(path, sizeof(path), FW_CLIENT_FMT, (long)getpid());
	if (fill_sockaddr(&addr, path) == -1) {
		close(fd);
		return -1;
	}

	unlink(addr.sun_path);		/* may not exist; ignore result */
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}
	strcpy(client_path, addr.sun_path);

	/* connect() fixes the peer, enabling send()/recv() and making the
	 * kernel drop datagrams from anyone but the daemon. */
	if (fill_sockaddr(&addr, FW_SOCKET_PATH) == -1) {
		close(fd);
		unlink(client_path);
		return -1;
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		saved_errno = errno;
		close(fd);
		unlink(client_path);
		errno = saved_errno;	/* cleanup must not mask connect's errno */
		return -1;
	}

	/*
	 * Bound the wait for a reply so the CLI fails cleanly when the
	 * daemon is not running, instead of blocking in recv() forever.
	 */
	tv.tv_sec = CLIENT_TIMEOUT_SECS;
	tv.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
		saved_errno = errno;
		close(fd);
		unlink(client_path);
		errno = saved_errno;
		return -1;
	}

	return fd;
}

int ipc_server_recv(int fd, struct fw_request *req,
		    struct sockaddr_un *from, socklen_t *fromlen)
{
	ssize_t n;

	*fromlen = sizeof(*from);
	n = recvfrom(fd, req, sizeof(*req), 0,
		     (struct sockaddr *)from, fromlen);
	if (n == -1)
		return -1;

	/* A short datagram is malformed; reject rather than act on it. */
	if ((size_t)n < sizeof(*req)) {
		errno = EBADMSG;
		return -1;
	}

	return 0;
}

int ipc_server_send(int fd, const struct fw_response *resp,
		    const struct sockaddr_un *to, socklen_t tolen)
{
	if (sendto(fd, resp, sizeof(*resp), 0,
		   (const struct sockaddr *)to, tolen) == -1)
		return -1;

	return 0;
}

int ipc_client_send(int fd, const struct fw_request *req)
{
	if (send(fd, req, sizeof(*req), 0) == -1)
		return -1;

	return 0;
}

int ipc_client_recv(int fd, struct fw_response *resp)
{
	ssize_t n;

	n = recv(fd, resp, sizeof(*resp), 0);
	if (n == -1)
		return -1;
	if ((size_t)n < sizeof(*resp)) {
		errno = EBADMSG;
		return -1;
	}

	return 0;
}

void ipc_server_close(int fd)
{
	close(fd);
	unlink(FW_SOCKET_PATH);
}

void ipc_client_close(int fd)
{
	close(fd);
	if (client_path[0] != '\0')
		unlink(client_path);
}