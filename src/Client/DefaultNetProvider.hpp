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
#include <chrono>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <string>
#include <string_view>

#include "Connection.hpp"
#include "Connector.hpp"
#include "NetworkEngine.hpp"
#include "../Utils/Timer.hpp"
#include "../Utils/rlist.h"

template<class BUFFER, class NetProvider>
class Connector;

template<class BUFFER, class NETWORK>
class DefaultNetProvider {
public:
	using NetProvider_t = DefaultNetProvider<BUFFER, NETWORK>;
	using Conn_t = Connection<BUFFER, NetProvider_t >;
	using Connector_t = Connector<BUFFER, NetProvider_t >;
	DefaultNetProvider();
	~DefaultNetProvider();
	int connect(Conn_t &conn, const std::string_view& addr, unsigned port,
		    size_t timeout);
	void close(Conn_t &conn);
	/** Add to @m_ready_to_write*/
	void readyToSend(Conn_t &conn);
	/** Read and write to sockets; polling using epoll. */
	int wait(int timeout);

	bool check(Conn_t &conn);
private:
	static constexpr size_t DEFAULT_TIMEOUT = 100;
	static constexpr size_t EVENT_POLL_COUNT_MAX = 64;
	static constexpr size_t EPOLL_QUEUE_LEN = 1024;
	static constexpr size_t EPOLL_EVENTS_MAX = 128;

	void send(Conn_t &conn);
	int recv(Conn_t &conn);

	int poll(struct ConnectionEvent *fds, size_t *fd_count,
		 int timeout = DEFAULT_TIMEOUT);
	int setPollSetting(int socket, int setting);
	int registerEpoll(int socket);

	/** <socket : connection> map. Contains both ready to read/send connections */
	std::unordered_map<int, Conn_t *> m_Connections;
	rlist m_ready_to_write;
	int m_EpollFd;
};

template<class BUFFER, class NETWORK>
DefaultNetProvider<BUFFER, NETWORK>::DefaultNetProvider()
{
	m_EpollFd = epoll_create(EPOLL_QUEUE_LEN);
	if (m_EpollFd == -1) {
		LOG_ERROR("Failed to initialize epoll: %s", strerror(errno));
		abort();
	}
	rlist_create(&m_ready_to_write);
}

template<class BUFFER, class NETWORK>
DefaultNetProvider<BUFFER, NETWORK>::~DefaultNetProvider()
{
	::close(m_EpollFd);
	m_EpollFd = 0;
	assert(rlist_empty(&m_ready_to_write));
}

template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::registerEpoll(int socket)
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

template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::setPollSetting(int socket, int setting)
{
	struct epoll_event event;
	event.events = setting;
	event.data.fd = socket;
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_MOD, socket, &event) != 0)
		return -1;
	return 0;
}


template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::connect(Conn_t &conn,
					     const std::string_view& addr,
					     unsigned port, size_t timeout)
{
	int socket = -1;
	socket = port == 0 ? NETWORK::connectUNIX(addr) :
			NETWORK::connectINET(addr, port, timeout);
	if (socket < 0) {
		/* There's no + operator for string and string_view ...*/
		conn.setError(std::string("Failed to establish connection to ") +
			      std::string(addr));
		return -1;
	}
	LOG_DEBUG("Connected to %s, socket is %d", std::string(addr).c_str(),
		  socket);
	/* Receive and decode greetings. */
	size_t iov_cnt = 0;
	struct iovec *iov =
		inBufferToIOV(conn, Iproto::GREETING_SIZE, &iov_cnt);
	LOG_DEBUG("Receiving greetings...");
	int read_bytes = NETWORK::recvall(socket, iov, iov_cnt, false);
	if (read_bytes < 0) {
		conn.setError(std::string("Failed to receive greetings: ") +
			      strerror(errno));
		::close(socket);
		return -1;
	}
	LOG_DEBUG("Greetings are received, read bytes %d", read_bytes);
	if (decodeGreeting(conn) != 0) {
		conn.setError(std::string("Failed to decode greetings"));
		::close(socket);
		return -1;
	}
	LOG_DEBUG("Greetings are decoded");
	LOG_DEBUG("Authentication processing...");
	//TODO: add authentication step.
	if (registerEpoll(socket) != 0) {
		conn.setError(std::string("Failed to register epoll watcher"));
		::close(socket);
		return -1;
	}
	conn.socket = socket;
	m_Connections[socket] = &conn;
	return 0;
}

template<class BUFFER, class NETWORK>
void
DefaultNetProvider<BUFFER, NETWORK>::close(Conn_t &connection)
{
#ifndef NDEBUG
	struct sockaddr sa;
	socklen_t sa_len = sizeof(sa);
	if (getsockname(connection.socket, &sa, &sa_len) != -1) {
		char addr[120];
		if (sa.sa_family == AF_INET) {
			struct sockaddr_in *sa_in = (struct sockaddr_in *) &sa;
			snprintf(addr, 120, "%s:%d", inet_ntoa(sa_in->sin_addr),
				 ntohs(sa_in->sin_port));
		} else {
			struct sockaddr_un *sa_un = (struct sockaddr_un *) &sa;
			snprintf(addr, 120, "%s", sa_un->sun_path);
		}
		LOG_DEBUG("Closed connection to socket %d corresponding to "
			  "address %s", connection.socket, addr);
	}
#endif
	NETWORK::close(connection.socket);
	if (connection.socket >= 0) {
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = connection.socket;
		/*
		 * Descriptor is automatically removed from epoll handler
		 * when all descriptors are closed. So in case
		 * there's other descriptors on open socket, invoke
		 * epoll_ctl manually.
		 */
		epoll_ctl(m_EpollFd, EPOLL_CTL_DEL, connection.socket, &event);
	}
	m_Connections.erase(connection.socket);
	connection.socket = -1;
}

template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::poll(struct ConnectionEvent *fds,
					  size_t *fd_count, int timeout)
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

template<class BUFFER, class NETWORK>
void
DefaultNetProvider<BUFFER, NETWORK>::readyToSend(Conn_t &conn)
{
	if (conn.status.is_send_blocked) {
#ifndef NDEBUG
		// If connection's send is blocked, then it must be in
		// the write list anyway.
		Connection<BUFFER, DefaultNetProvider> *tmp;
		rlist_foreach_entry(tmp, &m_ready_to_write, m_in_write) {
			if (tmp->socket == conn.socket)
				return;
		}
		assert(0);
#endif
		return;
	}

	Connection<BUFFER, DefaultNetProvider> *tmp;
	rlist_foreach_entry(tmp, &m_ready_to_write, m_in_write) {
		if (tmp == &conn)
			return;
	}
	rlist_add_tail(&m_ready_to_write, &conn.m_in_write);
	conn.status.is_ready_to_send = true;
}

template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::recv(Conn_t &conn)
{
	assert(! conn.status.is_failed);
	size_t total = NETWORK::readyToRecv(conn.socket);
	if (total < 0) {
		LOG_ERROR("Failed to check socket: ioctl returned errno %s",
			  strerror(errno));
		return -1;
	}
	if (total == 0) {
		LOG_DEBUG("Socket %d has no data to read", conn.socket);
		return -1;
	}
	size_t iov_cnt = 0;
	struct iovec *iov =
		inBufferToIOV(conn, total, &iov_cnt);
	int read_bytes = NETWORK::recvall(conn.socket, iov, iov_cnt, true);
	hasNotRecvBytes(conn, total - read_bytes);
	LOG_DEBUG("read %d bytes from %d socket", read_bytes, conn.socket);
	if (read_bytes < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -1;
		conn.setError(std::string("Failed to receive response: ") +
					  strerror(errno));
		if (errno == EBADF || errno == ENOTCONN ||
		    errno == ENOTSOCK || errno == EINVAL) {
				close(conn);
		}
		return -1;
	}
	return total - read_bytes;
}

template<class BUFFER, class NETWORK>
void
DefaultNetProvider<BUFFER, NETWORK>::send(Conn_t &conn)
{
	assert(! conn.status.is_failed);
	while (hasDataToSend(conn)) {
		size_t sent_bytes = 0;
		size_t iov_cnt = 0;
		struct iovec *iov = outBufferToIOV(conn, &iov_cnt);
		int rc = NETWORK::sendall(conn.socket, iov, iov_cnt,
						 &sent_bytes);
		hasSentBytes(conn, sent_bytes);
		LOG_DEBUG("send %d bytes to the %d socket", sent_bytes, conn.socket);
		if (rc != 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				int setting = EPOLLIN | EPOLLOUT;
				if (setPollSetting(conn.socket, setting) != 0) {
					LOG_ERROR("Failed to change epoll mode: "
						  "epoll_ctl() returned with errno: %s",
						  strerror(errno));
					abort();
				}
				conn.status.is_send_blocked = true;
			} else {
				conn.setError(std::string("Failed to send request: ") +
					      strerror(errno));
				if (errno == EBADF || errno == ENOTSOCK ||
				    errno == EFAULT || errno == EINVAL ||
				    errno == EPIPE) {
					close(conn);
				}
			}
			return;
		}
	}
	/* All data from connection has been successfully written. */
	if (conn.status.is_send_blocked) {
		if (setPollSetting(conn.socket, EPOLLIN) != 0) {
			LOG_ERROR("Failed to change epoll mode: epoll_ctl() "
				  "returned with errno: %s", strerror(errno));
			abort();
		}
		conn.status.is_send_blocked = false;
	}
}

template<class BUFFER, class NETWORK>
int
DefaultNetProvider<BUFFER, NETWORK>::wait(int timeout)
{
	assert(timeout >= 0);
	if (timeout == 0)
		timeout = DEFAULT_TIMEOUT;
	LOG_DEBUG("Network engine wait for %d milliseconds", timeout);
	/* Send pending requests. */
	if (!rlist_empty(&m_ready_to_write)) {
		Connection<BUFFER, DefaultNetProvider> *conn, *tmp;
		rlist_foreach_entry_safe(conn, &m_ready_to_write, m_in_write, tmp) {
			send(*conn);
		}
	}
	/* Firstly poll connections to point out if there's data to read. */
	static struct ConnectionEvent events[EVENT_POLL_COUNT_MAX];
	size_t event_cnt = 0;
	if (poll((ConnectionEvent *)&events, &event_cnt, timeout) != 0) {
		LOG_ERROR("Poll failed: %s", strerror(errno));
		return -1;
	}
	for (size_t i = 0; i < event_cnt; ++i) {
		Connection<BUFFER, DefaultNetProvider> *conn =
			m_Connections[events[i].sock];
		if ((events[i].event & EPOLLIN) != 0) {
			LOG_DEBUG("Registered poll event %d: %d socket is ready to read",
				  i, conn->socket);
			if (recv(*conn) == 0)
				conn->readyToDecode();
		}
		if ((events[i].event & EPOLLOUT) != 0) {
			/* We are watching only for blocked sockets. */
			LOG_DEBUG("Registered poll event %d: %d socket is ready to write",
				  i, conn->socket);
			assert(conn->status.is_send_blocked);
			send(*conn);
		}
	}
	return 0;
}

template<class BUFFER, class NETWORK>
bool
DefaultNetProvider<BUFFER, NETWORK>::check(Conn_t &connection)
{
	int error = 0;
	socklen_t len = sizeof(error);
	int rc = getsockopt(connection.socket, SOL_SOCKET, SO_ERROR, &error, &len);
	if (rc != 0) {
		connection.setError(strerror(rc));
		return false;
	}
	if (error != 0) {
		connection.setError(strerror(error));
		return false;
	}
	return true;
}