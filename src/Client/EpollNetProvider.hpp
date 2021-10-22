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
class EpollNetProvider {
public:
	using NetProvider_t = EpollNetProvider<BUFFER, NETWORK>;
	using Conn_t = Connection<BUFFER, NetProvider_t >;
	using Connector_t = Connector<BUFFER, NetProvider_t >;
	EpollNetProvider(Connector_t &connector);
	~EpollNetProvider();
	int connect(Conn_t &conn, const std::string_view& addr, unsigned port,
		    size_t timeout);
	void close(int socket);
	/** Read and write to sockets; polling using epoll. */
	int wait(int timeout);

	bool check(Conn_t &conn);
private:
	static constexpr size_t DEFAULT_TIMEOUT = 100;
	static constexpr size_t EVENT_POLL_COUNT_MAX = 64;
	static constexpr size_t EPOLL_QUEUE_LEN = 1024;
	static constexpr size_t EPOLL_EVENTS_MAX = 128;

	//return 0 if all data from buffer was processed (sent or read);
	//return -1 in case of errors;
	//return 1 in case socket is blocked.
	int send(Conn_t &conn);
	int recv(Conn_t &conn);

	int poll(struct ConnectionEvent *fds, size_t *fd_count,
		 int timeout = DEFAULT_TIMEOUT);
	void setPollSetting(int socket, int setting);
	void registerEpoll(int socket);

	/** <socket : connection> map. Contains both ready to read/send connections */
	std::map<int, Conn_t > m_Connections;
	Connector_t &m_Connector;
	int m_EpollFd;
};

template<class BUFFER, class NETWORK>
EpollNetProvider<BUFFER, NETWORK>::EpollNetProvider(Connector_t &connector) :
	m_Connector(connector)
{
	m_EpollFd = epoll_create(EPOLL_QUEUE_LEN);
	if (m_EpollFd == -1) {
		LOG_ERROR("Failed to initialize epoll: ", strerror(errno));
		abort();
	}
}

template<class BUFFER, class NETWORK>
EpollNetProvider<BUFFER, NETWORK>::~EpollNetProvider()
{
	::close(m_EpollFd);
	m_EpollFd = 0;

	for (auto conn = m_Connections.begin(); conn != m_Connections.end();)
		conn = m_Connections.erase(conn);
}

template<class BUFFER, class NETWORK>
void
EpollNetProvider<BUFFER, NETWORK>::registerEpoll(int socket)
{
	/* Configure epoll with new socket. */
	assert(m_EpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = socket;
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_ADD, socket, &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class NETWORK>
void
EpollNetProvider<BUFFER, NETWORK>::setPollSetting(int socket, int setting) {
	struct epoll_event event;
	event.events = setting;
	event.data.fd = socket;
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_MOD, socket, &event) != 0) {
		LOG_ERROR("Failed to change epoll mode: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class NETWORK>
int
EpollNetProvider<BUFFER, NETWORK>::connect(Conn_t &conn,
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
	LOG_DEBUG("Connected to ", addr, ", socket is ", socket);
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
	LOG_DEBUG("Greetings are received, read bytes ", read_bytes);
	if (decodeGreeting(conn) != 0) {
		conn.setError(std::string("Failed to decode greetings"));
		::close(socket);
		return -1;
	}
	LOG_DEBUG("Greetings are decoded");
	LOG_DEBUG("Authentication processing...");
	//TODO: add authentication step.
	registerEpoll(socket);
	conn.setSocket(socket);
	m_Connections.insert({socket, conn});
	return 0;
}

template<class BUFFER, class NETWORK>
void
EpollNetProvider<BUFFER, NETWORK>::close(int socket)
{
	assert(socket >= 0);
#ifndef NDEBUG
	struct sockaddr sa;
	socklen_t sa_len = sizeof(sa);
	if (getsockname(socket, &sa, &sa_len) != -1) {
		char addr[120];
		if (sa.sa_family == AF_INET) {
			struct sockaddr_in *sa_in = (struct sockaddr_in *) &sa;
			snprintf(addr, 120, "%s:%d", inet_ntoa(sa_in->sin_addr),
				 ntohs(sa_in->sin_port));
		} else {
			struct sockaddr_un *sa_un = (struct sockaddr_un *) &sa;
			snprintf(addr, 120, "%s", sa_un->sun_path);
		}
		LOG_DEBUG("Closed connection to socket ", socket,
			  " corresponding to address ", addr);
	}
#endif
	NETWORK::close(socket);
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
	//close can be called during epoll provider destruction. In this case
	//all connections staying alive only due to the presence in m_Connections
	//map. While cleaning up m_Connections destructors of connections will be
	//called. So to avoid double-free presence check in m_Connections is required.
	if (m_Connections.find(socket) != m_Connections.end()) {
		assert(m_Connections.find(socket)->second.getSocket() == socket);
		m_Connections.erase(socket);
	}
}

template<class BUFFER, class NETWORK>
int
EpollNetProvider<BUFFER, NETWORK>::poll(struct ConnectionEvent *fds,
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
int
EpollNetProvider<BUFFER, NETWORK>::recv(Conn_t &conn)
{
	size_t total = NETWORK::readyToRecv(conn.getSocket());
	if (total < 0) {
		LOG_ERROR("Failed to check socket: ioctl returned errno ",
			  strerror(errno));
		return -1;
	}
	if (total == 0) {
		LOG_DEBUG("Socket ", conn.getSocket(), " has no data to read");
		return 0;
	}
	size_t iov_cnt = 0;
	/* Get IO vectors array pointing to the input buffer. */
	struct iovec *iov = inBufferToIOV(conn, total, &iov_cnt);
	int has_read_bytes = NETWORK::recvall(conn.getSocket(), iov, iov_cnt, true);
	if (has_read_bytes < 0) {
		//Don't consider EWOULDBLOCK to be an error.
		if (netWouldBlock(errno))
			return 0;
		conn.setError(std::string("Failed to receive response: ") +
					  strerror(errno), errno);
		return -1;
	}
	hasNotRecvBytes(conn, total - has_read_bytes);
	return total - has_read_bytes;
}

template<class BUFFER, class NETWORK>
int
EpollNetProvider<BUFFER, NETWORK>::send(Conn_t &conn)
{
	while (hasDataToSend(conn)) {
		size_t sent_bytes = 0;
		size_t iov_cnt = 0;
		struct iovec *iov = outBufferToIOV(conn, &iov_cnt);
		int rc = NETWORK::sendall(conn.getSocket(), iov, iov_cnt,
						 &sent_bytes);
		hasSentBytes(conn, sent_bytes);
		if (rc != 0) {
			if (netWouldBlock(errno)) {
				setPollSetting(conn.getSocket(), EPOLLIN | EPOLLOUT);
				return 1;
			}
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);

			return -1;
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class NETWORK>
int
EpollNetProvider<BUFFER, NETWORK>::wait(int timeout)
{
	assert(timeout >= 0);
	if (timeout == 0)
		timeout = DEFAULT_TIMEOUT;
	LOG_DEBUG("Network engine wait for ", timeout, " milliseconds");
	/* Send pending requests. */
	for (auto conn = m_Connector.m_ReadyToSend.begin();
	     conn != m_Connector.m_ReadyToSend.end();) {
		Conn_t to_be_send(*conn);
		(void) send(to_be_send);
		conn = m_Connector.m_ReadyToSend.erase(conn);
	}

	/* Firstly poll connections to point out if there's data to read. */
	static struct ConnectionEvent events[EVENT_POLL_COUNT_MAX];
	size_t event_cnt = 0;
	if (poll((ConnectionEvent *)&events, &event_cnt, timeout) != 0) {
		//Poll error doesn't belong to any connection so just global
		//log it.
		LOG_ERROR("Poll failed: ", strerror(errno));
		return -1;
	}
	for (size_t i = 0; i < event_cnt; ++i) {
		assert(m_Connections.find(events[i].sock) != m_Connections.end());
		if ((events[i].event & EPOLLIN) != 0) {
			Conn_t conn = m_Connections.find(events[i].sock)->second;
			assert(conn.getSocket() == events[i].sock);
			LOG_DEBUG("Registered poll event ", i, ": ",
				  conn.getSocket(), " socket is ready to read");
			/*
			 * Once we read all bytes from socket connection
			 * becomes ready to decode.
			 */
			int rc = recv(conn);
			if (rc < 0)
				return -1;
			if (rc == 0)
				m_Connector.readyToDecode(conn);
		}

		if ((events[i].event & EPOLLOUT) != 0) {
			Conn_t conn = m_Connections.find(events[i].sock)->second;
			assert(conn.getSocket() == events[i].sock);
			LOG_DEBUG("Registered poll event ", i, ": ",
				  conn.getSocket(), " socket is ready to write");
			int rc = send(conn);
			if (rc < 0)
				return -1;
			/* All data from connection has been successfully written. */
			if (rc == 0) {
				m_Connector.finishSend(conn);
				setPollSetting(conn.getSocket(), EPOLLIN);
			}
		}
	}
	return 0;
}

template<class BUFFER, class NETWORK>
bool
EpollNetProvider<BUFFER, NETWORK>::check(Conn_t &connection)
{
	int error = 0;
	socklen_t len = sizeof(error);
	int rc = getsockopt(connection.getSocket(), SOL_SOCKET, SO_ERROR, &error, &len);
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
