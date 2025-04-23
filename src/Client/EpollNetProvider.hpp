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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <assert.h>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <string>
#include <string_view>

#include "Connection.hpp"

template<class BUFFER, class NetProvider>
class Connector;

template<class BUFFER, class Stream>
class EpollNetProvider {
public:
	using Buffer_t = BUFFER;
	using Stream_t = Stream;
	using NetProvider_t = EpollNetProvider<BUFFER, Stream>;
	using Conn_t = Connection<BUFFER, NetProvider_t >;
	using Connector_t = Connector<BUFFER, NetProvider_t >;
	EpollNetProvider(Connector_t &connector);
	~EpollNetProvider();
	int connect(Conn_t &conn, const ConnectOptions &opts);
	void close(Stream_t &strm);
	/** Read and write to sockets; polling using epoll. */
	int wait(int timeout);

private:
	static constexpr int TIMEOUT_INFINITY = -1;
	static constexpr size_t EPOLL_EVENTS_MAX = 128;

	//return 0 if all data from buffer was processed (sent or read);
	//return -1 in case of errors;
	//return 1 in case socket is blocked.
	int send(Conn_t &conn);
	int recv(Conn_t &conn);

	void setPollSetting(Conn_t &conn, int setting);
	void registerEpoll(Conn_t &conn);

	/** <socket : connection> map. Contains both ready to read/send connections */
	Connector_t &m_Connector;
	int m_EpollFd;
};

template<class BUFFER, class Stream>
EpollNetProvider<BUFFER, Stream>::EpollNetProvider(Connector_t &connector) :
	m_Connector(connector)
{
	m_EpollFd = epoll_create1(EPOLL_CLOEXEC);
	if (m_EpollFd == -1) {
		LOG_ERROR("Failed to initialize epoll: ", strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
EpollNetProvider<BUFFER, Stream>::~EpollNetProvider()
{
	::close(m_EpollFd);
	m_EpollFd = -1;
}

template<class BUFFER, class Stream>
void
EpollNetProvider<BUFFER, Stream>::registerEpoll(Conn_t &conn)
{
	/* Configure epoll with new socket. */
	assert(m_EpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = conn.getImpl();
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_ADD, conn.get_strm().get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
void
EpollNetProvider<BUFFER, Stream>::setPollSetting(Conn_t &conn, int setting) {
	struct epoll_event event;
	event.events = setting;
	event.data.ptr = conn.getImpl();
	if (epoll_ctl(m_EpollFd, EPOLL_CTL_MOD, conn.get_strm().get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to change epoll mode: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
int
EpollNetProvider<BUFFER, Stream>::connect(Conn_t &conn,
					  const ConnectOptions &opts)
{
	auto &strm = conn.get_strm();
	if (strm.connect(opts) < 0) {
		conn.setError("Failed to establish connection to " +
			      opts.address);
		return -1;
	}
	LOG_DEBUG("Connected to ", opts.address, ", socket is ", strm.get_fd());

	registerEpoll(conn);
	return 0;
}

template<class BUFFER, class Stream>
void
EpollNetProvider<BUFFER, Stream>::close(Stream_t& strm)
{
	int was_fd = strm.get_fd();
	assert(was_fd >= 0);
	strm.close();
#ifndef NDEBUG
	struct sockaddr sa;
	socklen_t sa_len = sizeof(sa);
	if (getsockname(was_fd, &sa, &sa_len) != -1) {
		char addr[120];
		if (sa.sa_family == AF_INET) {
			struct sockaddr_in *sa_in = (struct sockaddr_in *) &sa;
			snprintf(addr, 120, "%s:%d", inet_ntoa(sa_in->sin_addr),
				 ntohs(sa_in->sin_port));
		} else {
			struct sockaddr_un *sa_un = (struct sockaddr_un *) &sa;
			snprintf(addr, 120, "%s", sa_un->sun_path);
		}
		LOG_DEBUG("Closed connection to socket ", was_fd,
			  " corresponding to address ", addr);
	}
#endif
	/*
	 * Descriptor is automatically removed from epoll handler
	 * when all descriptors are closed. So in case
	 * there's other descriptors on open socket, invoke
	 * epoll_ctl manually.
	 */
	epoll_ctl(m_EpollFd, EPOLL_CTL_DEL, was_fd, nullptr);
}

template<class BUFFER, class Stream>
int
EpollNetProvider<BUFFER, Stream>::recv(Conn_t &conn)
{
	auto &buf = conn.getInBuf();
	auto itr = buf.template end<true>();
	buf.write({CONN_READAHEAD});
	struct iovec iov[IOVEC_MAX_SIZE];
	size_t iov_cnt = buf.getIOV(itr, iov, IOVEC_MAX_SIZE);

	ssize_t rcvd = conn.get_strm().recv(iov, iov_cnt);
	hasNotRecvBytes(conn, CONN_READAHEAD - (rcvd < 0 ? 0 : rcvd));
	if (rcvd < 0) {
		conn.setError(std::string("Failed to receive response: ") +
					  strerror(errno), errno);
		return -1;
	}

	if (rcvd == 0) {
		assert(conn.get_strm().has_status(SS_NEED_EVENT_FOR_READ));
		if (conn.get_strm().has_status(SS_NEED_WRITE_EVENT_FOR_READ))
			setPollSetting(conn, EPOLLIN | EPOLLOUT);
	}

	if (!conn.getImpl()->is_greeting_received) {
		if ((size_t) rcvd < Iproto::GREETING_SIZE)
			return 0;
		/* Receive and decode greetings. */
		LOG_DEBUG("Greetings are received, read bytes ", rcvd);
		if (decodeGreeting(conn) != 0) {
			conn.setError("Failed to decode greetings");
			return -1;
		}
		LOG_DEBUG("Greetings are decoded");
		rcvd -= Iproto::GREETING_SIZE;
		if (conn.getImpl()->is_auth_required) {
			// Finalize auth request in buffer.
			conn.commit_auth(conn.get_strm().get_opts().user,
					 conn.get_strm().get_opts().passwd);
		}
	}

	return 0;
}

template<class BUFFER, class Stream>
int
EpollNetProvider<BUFFER, Stream>::send(Conn_t &conn)
{
	while (hasDataToSend(conn)) {
		struct iovec iov[IOVEC_MAX_SIZE];
		auto &buf = conn.getOutBuf();
		size_t iov_cnt = buf.getIOV(buf.template begin<true>(),
					    iov, IOVEC_MAX_SIZE);

		ssize_t sent = conn.get_strm().send(iov, iov_cnt);
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);
			return -1;
		} else if (sent == 0) {
			assert(conn.get_strm().has_status(SS_NEED_EVENT_FOR_WRITE));
			if (conn.get_strm().has_status(SS_NEED_WRITE_EVENT_FOR_WRITE))
				setPollSetting(conn, EPOLLIN | EPOLLOUT);
			return 1;
		} else {
			hasSentBytes(conn, sent);
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class Stream>
int
EpollNetProvider<BUFFER, Stream>::wait(int timeout)
{
	assert(timeout >= -1);
	if (timeout == -1)
		timeout = TIMEOUT_INFINITY;
	LOG_DEBUG("Network engine wait for ", timeout, " milliseconds");
	/* Send pending requests. */
	for (auto conn = m_Connector.m_ReadyToSend.begin();
	     conn != m_Connector.m_ReadyToSend.end();) {
		Conn_t to_be_send(*conn);
		(void) send(to_be_send);
		conn = m_Connector.m_ReadyToSend.erase(conn);
	}

	/* Firstly poll connections to point out if there's data to read. */
	struct epoll_event events[EPOLL_EVENTS_MAX];
	int event_cnt = epoll_wait(m_EpollFd, events, EPOLL_EVENTS_MAX, timeout);
	if (event_cnt < 0) {
		//Poll error doesn't belong to any connection so just global
		//log it.
		LOG_ERROR("Poll failed: ", strerror(errno));
		return -1;
	}
	for (int i = 0; i < event_cnt; ++i) {
		Conn_t conn((typename Conn_t::Impl_t *)events[i].data.ptr);
		if ((events[i].events & EPOLLIN) != 0) {
			LOG_DEBUG("Registered poll event ", i, ": ",
				  conn.get_strm().get_fd(),
				  " socket is ready to read");
			if (conn.get_strm().has_status(SS_NEED_READ_EVENT_FOR_WRITE)) {
				int rc = send(conn);
				if (rc < 0)
					return -1;
			}
			/*
			 * Once we read all bytes from socket connection
			 * becomes ready to decode.
			 */
			int rc = recv(conn);
			if (rc < 0)
				return -1;
			if (hasDataToDecode(conn))
				m_Connector.readyToDecode(conn);
		}

		if ((events[i].events & EPOLLOUT) != 0) {
			LOG_DEBUG("Registered poll event ", i, ": ",
				  conn.get_strm().get_fd(),
				  " socket is ready to write");
			if (conn.get_strm().has_status(SS_NEED_WRITE_EVENT_FOR_READ)) {
				int rc = recv(conn);
				if (rc < 0)
					return -1;
			}
			int rc = send(conn);
			if (rc < 0)
				return -1;
			/* All data from connection has been successfully written. */
			if (rc == 0) {
				m_Connector.finishSend(conn);
				setPollSetting(conn, EPOLLIN);
			}
		}
	}
	return 0;
}
