#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <string>
#include <string_view>

#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include "../Utils/Logger.hpp"

#ifdef offsetof
#undef offsetof
#endif
#define offsetof(type, member) ((size_t) &((type *)0)->member)

/** Simple wrapper to implement RAII over socket. */
struct Socket {
	Socket(int sockfd) : fd(sockfd) { };
	~Socket() { if (fd >= 0) close(fd); }
	int fd;
};

struct ConnectionEvent {
	int sock;
	uint32_t event;
};

static size_t
IOVCountBytes(struct iovec *iovecs, size_t iov_len)
{
	size_t iov_bytes = 0;
	for (size_t i = 0; i < iov_len; ++i)
		iov_bytes += iovecs[i].iov_len;
	return iov_bytes;
}

static inline bool
netWouldBlock(int err)
{
	return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
}

class NetworkEngine {
public:
	static int connectINET(const std::string_view& addr_str, unsigned port,
			       size_t timeout);
	static int connectUNIX(const std::string_view& path);
	static void close(int socket);

	static int send(int socket, struct iovec *iov, size_t iov_len);
	static int sendall(int socket, struct iovec *iov, size_t iov_len,
			   size_t *sent_bytes);
	static int recv(int socket, struct iovec *iov, size_t iov_len);
	static int recvall(int socket, struct iovec *iov, size_t iov_len,
			   bool dont_wait);
	static size_t readyToRecv(int socket);
};

inline int
NetworkEngine::connectINET(const std::string_view& addr_str, unsigned port,
			   size_t timeout)
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	std::string service = std::to_string(port);
	int err = getaddrinfo(std::string(addr_str).c_str(), service.c_str(),
			      &hints, &res);
	if (err != 0) {
		LOG_ERROR("getaddrinfo() failed: ", gai_strerror(err));
		return -1;
	}
	Socket soc(socket(res->ai_family, res->ai_socktype, res->ai_protocol));
	if (soc.fd < 0) {
		LOG_ERROR("Failed to create socket: ", strerror(errno));
		freeaddrinfo(res);
		return -1;
	}
	/* Set socket to non-blocking mode*/
	if (fcntl(soc.fd, F_SETFL, O_NONBLOCK) != 0) {
		LOG_ERROR("fcntl failed: ", strerror(errno));
		freeaddrinfo(res);
		return -1;
	}
	::connect(soc.fd, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	/*
	 * Now let's use select to timeout connect call. Once socket becomes
	 * writable - connection is established.
	 */
	fd_set fdset;
	struct timeval tv;
	FD_ZERO(&fdset);
	FD_SET(soc.fd, &fdset);
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	int rc = select(soc.fd + 1, NULL, &fdset, NULL, &tv);
	if (rc == -1) {
		LOG_ERROR("select() failed: ", strerror(errno));
		return -1;
	}
	if (rc == 0) {
		LOG_ERROR("connect() is timed out! Waited for ",
			  timeout, " seconds");
		return -1;
	}
	assert(rc == 1);
	int so_error;
	socklen_t len = sizeof(so_error);
	if (getsockopt(soc.fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) {
		LOG_ERROR("getsockopt() failed: ", strerror(errno));
		return -1;
	}
	if (so_error != 0) {
		LOG_ERROR("connect() failed: ", strerror(so_error));
		return -1;
	}
	if (fcntl(soc.fd, F_SETFL, O_NONBLOCK) != 0) {
		LOG_ERROR("fcntl() failed: ", strerror(errno));
		return -1;
	}
	/* Set to blocking mode again...*/
	int flags = fcntl(soc.fd, F_GETFL, NULL);
	if (flags < 0) {
		LOG_ERROR("fcntl() failed: ", strerror(errno));
		return -1;
	}
	flags &= (~O_NONBLOCK);
	if (fcntl(soc.fd, F_SETFL, flags) != 0) {
		LOG_ERROR("fcntl() failed: ", strerror(errno));
		return -1;
	}
	int sock = soc.fd;
	soc.fd = -1;
	return sock;
}

inline int
NetworkEngine::connectUNIX(const std::string_view& path)
{
	Socket soc(socket(AF_UNIX, SOCK_STREAM, 0));
	if (soc.fd < 0)
		return -1;
	struct sockaddr_un addr;
	std::memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	std::strcpy(addr.sun_path, std::string(path).c_str());
	if (::connect(soc.fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		return -1;
	int sock = soc.fd;
	soc.fd = -1;
	return sock;
}

inline void
NetworkEngine::close(int socket)
{
	::close(socket);
}

inline int
NetworkEngine::send(int socket, struct iovec *iov, size_t iov_len)
{
	struct msghdr msg;
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = MSG_DONTWAIT;
	return sendmsg(socket, &msg, flags);
}

inline int
NetworkEngine::sendall(int socket, struct iovec *iov, size_t iov_len,
		       size_t *sent_bytes)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = MSG_DONTWAIT;
	*sent_bytes = 0;
	size_t total_sz = IOVCountBytes(iov, iov_len);
	while (*sent_bytes < total_sz) {
		int rc = sendmsg(socket, &msg, flags);
		if (rc == -1)
			return -1;
		*sent_bytes += rc;
		while ((size_t) rc > iov->iov_len) {
			rc -= iov->iov_len;
			iov = iov + 1;
			assert(msg.msg_iovlen > 0);
			msg.msg_iovlen--;
		}
		msg.msg_iov = iov;
	}
	return 0;
}

inline int
NetworkEngine::recv(int socket, struct iovec *iov, size_t iov_len)
{
	struct msghdr msg;
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	return recvmsg(socket, &msg, flags);
}

inline int
NetworkEngine::recvall(int socket, struct iovec *iov, size_t iov_len,
		       bool dont_wait)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = dont_wait ? MSG_DONTWAIT : 0;
	int rc = recvmsg(socket, &msg, flags);
	if (rc == -1)
		return -1;
	int read_bytes = rc;
	while ((size_t) read_bytes > iov->iov_len) {
		read_bytes -= iov->iov_len;
		iov = iov + 1;
		assert(msg.msg_iovlen > 0);
		msg.msg_iovlen--;
	}
	msg.msg_iov = iov;
	return rc;
}

inline size_t
NetworkEngine::readyToRecv(int socket)
{
	int bytes = 0;
	if (ioctl(socket, FIONREAD, &bytes) < 0)
		return -1;
	return bytes;
}
