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
#include <sys/epoll.h>
#include <sys/ioctl.h>
//#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
//#include <netinet/in.h>
#include <arpa/inet.h>

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

class NetworkEngine {
public:
	NetworkEngine();
	int connectINET(const std::string_view& addr_str, unsigned port);
	int connectUNIX(const std::string_view& path);
	void close(int socket);

	int send(int socket, struct iovec *iov, size_t iov_len);
	int sendall(int socket, struct iovec *iov, size_t iov_len,
		    size_t *sent_bytes);
	int recv(int socket, struct iovec *iov, size_t iov_len);
	int recvall(int socket, struct iovec *iov, size_t iov_len,
		    size_t total, size_t *read_bytes, bool dont_wait);
	size_t readyToRecv(int socket) /* throw */;

	int poll(struct ConnectionEvent *fds, size_t *fd_count,
		 int timeout = EPOLL_DEFAULT_TIMEOUT);
	void setPollSetting(int socket, int setting);
	void resetPollTimeout();
	size_t poll_timeout;
private:
	constexpr static size_t EPOLL_QUEUE_LEN = 1024;
	constexpr static size_t EPOLL_DEFAULT_TIMEOUT = 1000;
	constexpr static size_t EPOLL_EVENTS_MAX = 128;

	int registerEpoll(int socket);
	int m_EpollFd;
};

NetworkEngine::NetworkEngine()
{
	m_EpollFd = epoll_create(EPOLL_QUEUE_LEN);
	if (m_EpollFd == -1){
		LOG_ERROR("Failed to initialize epoll: %s", strerror(errno));
		abort();
	}
}

int
NetworkEngine::registerEpoll(int socket)
{
	/* Configure epoll with new socket. */
	assert(m_EpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = socket;
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_ADD, socket, &event) != 0)
		return -1;
	return 0;
}

void
NetworkEngine::setPollSetting(int socket, int setting)
{
	struct epoll_event event;
	event.events = setting;
	event.data.fd = socket;
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_MOD, socket, &event) != 0) {
		throw std::runtime_error(std::string("Failed to change epoll mode: "
						     "epoll_ctl() returned with errno " +
						     std::to_string(errno)));
	}
}

int
NetworkEngine::connectINET(const std::string_view& addr_str, unsigned port)
{
	Socket soc(socket(AF_INET, SOCK_STREAM, 0));
	if (soc.fd < 0)
		return -1;
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	//TODO: use getaddrinfo
	if (inet_pton(AF_INET, std::string(addr_str).c_str(),
		      &addr.sin_addr) <= 0)
		return -1;
	if (::connect(soc.fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		return -1;
	}
	if (registerEpoll(soc.fd) != 0)
		return -1;
	int sock = soc.fd;
	soc.fd = -1;
	return sock;
}
int
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
	if (registerEpoll(soc.fd) != 0)
		return -1;
	int sock = soc.fd;
	soc.fd = -1;
	return sock;
}

int
NetworkEngine::send(int socket, struct iovec *iov, size_t iov_len)
{
	struct msghdr msg;
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = MSG_DONTWAIT;
	return sendmsg(socket, &msg, flags);
}

int
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

int
NetworkEngine::recv(int socket, struct iovec *iov, size_t iov_len)
{
	struct msghdr msg;
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	return recvmsg(socket, &msg, flags);
}

int
NetworkEngine::recvall(int socket, struct iovec *iov, size_t iov_len,
		       size_t total, size_t *read_bytes, bool dont_wait)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;

	int flags = dont_wait ? MSG_DONTWAIT : 0;
	*read_bytes = 0;
	while (total > *read_bytes) {
		int rc = recvmsg(socket, &msg, flags);
		if (rc == -1)
			return -1;
		*read_bytes += rc;
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

size_t
NetworkEngine::readyToRecv(int socket)
{
	int bytes = 0;
	if (ioctl(socket, FIONREAD, &bytes) < 0) {
		throw std::runtime_error(std::string("Failed to check socket: "
						     "ioctl retured errno " +
						     std::to_string(errno)));
	}
	return bytes;
}

void
NetworkEngine::close(int socket)
{
	if (socket >= 0) {
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = socket;
		/*
		 * Descriptor is automatically removed from epoll handler
		 * when all descriptors are closed. So in case
		 * there's other descriptors on open socket, invoke
		 * epoll_ctl manually.
		 */
		epoll_ctl(m_EpollFd, EPOLL_CTL_DEL, socket, &event);
	}
	::close(socket);
}

int
NetworkEngine::poll(struct ConnectionEvent *fds, size_t *fd_count, int timeout)
{
	static struct epoll_event events[EPOLL_EVENTS_MAX];
	*fd_count = 0;
	int event_cnt = epoll_wait(m_EpollFd, events, EPOLL_EVENTS_MAX,
				   timeout);
	if (event_cnt == -1)
		return -1;
	assert(event_cnt >= 0);
	for (int i = 0; i < event_cnt; ++i) {
		fds[*fd_count].sock = events[i].data.fd;
		fds[*fd_count].event = events[i].events;
		(*fd_count)++;
	}
	assert(*fd_count == (size_t) event_cnt);
	return 0;
}
