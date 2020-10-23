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

template<class BUFFER>
class DefaultNetProvider {
public:
	DefaultNetProvider();
	~DefaultNetProvider();
	int connect(Connection<BUFFER, DefaultNetProvider> &conn,
		    const std::string_view& addr, unsigned port);
	void close(Connection<BUFFER, DefaultNetProvider> &conn);
	/** Add to @m_ready_to_write*/
	void readyToSend(Connection<BUFFER, DefaultNetProvider> &conn);
	/** Read and write to sockets; polling using epoll. */
	int wait(Connector<BUFFER, DefaultNetProvider> &connector, int timeout);

	bool check(Connection<BUFFER, DefaultNetProvider> &conn);
private:
	static constexpr int DEFAULT_TIMEOUT = 100;
	static constexpr size_t EVENT_POLL_COUNT_MAX = 64;

	void send(Connection<BUFFER, DefaultNetProvider> &conn);
	int recv(Connection<BUFFER, DefaultNetProvider> &conn);

	NetworkEngine m_NetworkEngine;
	/** <socket : connection> map. Contains both ready to read/send connections */
	std::unordered_map<int, Connection<BUFFER, DefaultNetProvider> *> m_Connections;
	rlist m_ready_to_write;
};

template<class BUFFER>
DefaultNetProvider<BUFFER>::DefaultNetProvider() : m_NetworkEngine()
{
	rlist_create(&m_ready_to_write);
}

template<class BUFFER>
DefaultNetProvider<BUFFER>::~DefaultNetProvider()
{
	assert(rlist_empty(&m_ready_to_write));
}

template<class BUFFER>
int
DefaultNetProvider<BUFFER>::connect(Connection<BUFFER, DefaultNetProvider> &conn,
				    const std::string_view& addr, unsigned port)
{
	int socket = -1;
	socket = port == 0 ? m_NetworkEngine.connectUNIX(addr) :
			     m_NetworkEngine.connectINET(addr, port);
	LOG_DEBUG("Connected to %s, socket is %d", std::string(addr).c_str(), socket);
	if (socket < 0) {
		conn.setError(std::string("Failed to establish connection: ") +
			      strerror(errno));
		return -1;
	}
	/* Receive and decode greetings. */
	size_t iov_cnt = 0;
	struct iovec *iov =
		inBufferToIOV(conn, Iproto::GREETING_SIZE, &iov_cnt);
	size_t read_bytes = 0;
	LOG_DEBUG("Receiving greetings...");
	int rc = m_NetworkEngine.recvall(socket, iov, iov_cnt,
					 Iproto::GREETING_SIZE, &read_bytes,
					 false);
	if (rc < 0) {
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
	conn.socket = socket;
	m_Connections[socket] = &conn;
	return 0;
}

template<class BUFFER>
void
DefaultNetProvider<BUFFER>::close(Connection<BUFFER, DefaultNetProvider> &connection)
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
	m_NetworkEngine.close(connection.socket);
	m_Connections.erase(connection.socket);
	connection.socket = -1;
}

template<class BUFFER>
void
DefaultNetProvider<BUFFER>::readyToSend(Connection<BUFFER, DefaultNetProvider> &conn)
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

template<class BUFFER>
int
DefaultNetProvider<BUFFER>::recv(Connection<BUFFER, DefaultNetProvider> &conn)
{
	assert(! conn.status.is_failed);
	//TODO: refactor this part. Get rid of readyToRecv and pass
	//input buffer directly to recv allocating new blocks on demand.
	size_t total = m_NetworkEngine.readyToRecv(conn.socket);
	if (total == 0)
		return 0;
	size_t read_bytes = 0;
	size_t iov_cnt = 0;
	struct iovec *iov =
		inBufferToIOV(conn, total, &iov_cnt);
	int rc = m_NetworkEngine.recvall(conn.socket, iov, iov_cnt,
					 total, &read_bytes, true);
	hasNotRecvBytes(conn, total - read_bytes);
	LOG_DEBUG("read %d bytes from %d socket", read_bytes, conn.socket);
	if (rc != 0) {
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
	return 0;
}

template<class BUFFER>
void
DefaultNetProvider<BUFFER>::send(Connection<BUFFER, DefaultNetProvider> &conn)
{
	assert(! conn.status.is_failed);
	while (hasDataToSend(conn)) {
		size_t sent_bytes = 0;
		size_t iov_cnt = 0;
		struct iovec *iov = outBufferToIOV(conn, &iov_cnt);
		int rc = m_NetworkEngine.sendall(conn.socket, iov, iov_cnt,
						 &sent_bytes);
		hasSentBytes(conn, sent_bytes);
		LOG_DEBUG("send %d bytes to the %d socket", sent_bytes, conn.socket);
		if (rc != 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				int setting = EPOLLIN | EPOLLOUT;
				m_NetworkEngine.setPollSetting(conn.socket,
							       setting);
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
		m_NetworkEngine.setPollSetting(conn.socket, EPOLLIN);
		conn.status.is_send_blocked = false;
	}
}

template<class BUFFER>
int
DefaultNetProvider<BUFFER>::wait(Connector<BUFFER, DefaultNetProvider> &connector,
				 int timeout)
{
	(void) connector;
	assert(timeout >= 0);
	if (timeout == 0)
		timeout = DEFAULT_TIMEOUT;
	Timer timer{timeout};
	int poll_timeout = timeout/2;
	timer.start();
	LOG_DEBUG("Network engine wait for %d milliseconds", poll_timeout);
	do {
		/* Firstly poll connections to point out if there's data to read. */
		static struct ConnectionEvent events[EVENT_POLL_COUNT_MAX];
		size_t event_cnt = 0;
		//LOG_DEBUG("Poll for %d milliseconds", poll_timeout);
		if (m_NetworkEngine.poll((ConnectionEvent *)&events, &event_cnt,
					 poll_timeout) != 0) {
			LOG_ERROR("Poll failed: %s", strerror(errno));
			return -1;
		}
		for (size_t i = 0; i < event_cnt; ++i) {
			Connection<BUFFER, DefaultNetProvider> *conn =
				m_Connections[events[i].sock];
			if ((events[i].event & EPOLLIN) != 0) {
				LOG_DEBUG("Registered poll event: %d socket is ready to read",
					  conn->socket);
				if (recv(*conn) == 0)
					connector.readyToDecode(*conn);
			}
			if ((events[i].event & EPOLLOUT) != 0) {
				/* We are watching only for blocked sockets. */
				LOG_DEBUG("Registered poll event: %d socket is ready to write",
					  conn->socket);
				assert(conn->status.is_send_blocked);
				send(*conn);
			}
		}
		/* Then send pending requests. */
		if (!rlist_empty(&m_ready_to_write)) {
			Connection<BUFFER, DefaultNetProvider> *conn, *tmp;
			rlist_foreach_entry_safe(conn, &m_ready_to_write,
						 m_in_write, tmp) {
				send(*conn);
			}
		}
		poll_timeout = std::max((timeout - timer.elapsed())/2, 0);
	} while (! timer.isExpired());
	return 0;
}

template<class BUFFER>
bool
DefaultNetProvider<BUFFER>::check(Connection<BUFFER, DefaultNetProvider> &connection)
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