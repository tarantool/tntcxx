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
#define EV_STANDALONE 1
#include "libev/ev.c"

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
#include "../Utils/rlist.h"

template<class BUFFER, class NETWORK>
class Connector;

struct WaitWatcher {
	struct ev_io in;
	struct ev_io out;
	struct ev_timer *timer;
	void *connection;
	void *provider;
};

static inline void
timerDisable(struct ev_loop *loop, struct ev_timer *timer)
{
	if (ev_is_active(timer))
		ev_timer_stop(loop, timer);
}

template<class BUFFER, class NETWORK>
class LibevNetProvider {
public:
	using NetProvider_t = LibevNetProvider<BUFFER, NETWORK>;
	using Conn_t = Connection<BUFFER, NetProvider_t >;

	LibevNetProvider(struct ev_loop *loop = nullptr);
	int connect(Conn_t &conn, const std::string_view& addr, unsigned port,
		    size_t timeout);
	void close(Conn_t &conn);
	void readyToSend(Conn_t &conn);
	int wait(int timeout);
	bool check(Conn_t &conn);

	~LibevNetProvider();

private:
	static constexpr float MILLISECONDS = 1000.f;

	int registerWatchers(Conn_t *conn, int fd);
	void releaseWatchers(int fd);

	std::map<int, WaitWatcher *> m_Watchers;
	struct ev_loop *m_Loop;
	struct ev_timer m_TimeoutWatcher;

	rlist m_ready_to_write;
	bool m_IsOwnLoop;
};

template<class BUFFER, class NETWORK>
void
LibevNetProvider<BUFFER, NETWORK>::releaseWatchers(int fd)
{
	assert(fd >= 0);
	assert(m_Watchers.find(fd) != m_Watchers.end());
	struct WaitWatcher *watcher = m_Watchers[fd];
	ev_io_stop(m_Loop, &watcher->in);
	ev_io_stop(m_Loop, &watcher->out);
	free(watcher);
	m_Watchers.erase(fd);
}

template<class BUFFER, class NETWORK>
static inline int
connectionReceive(Connection<BUFFER,  LibevNetProvider<BUFFER, NETWORK>> &conn)
{
	assert(! conn.status.is_failed);
	size_t total = NETWORK::readyToRecv(conn.socket);
	if (total < 0) {
		LOG_ERROR("Failed to check socket: ioctl returned errno %s",
			  strerror(errno));
		return -1;
	}
	size_t iov_cnt = 0;
	struct iovec *iov =
		inBufferToIOV(conn, total, &iov_cnt);
	int read_bytes = NETWORK::recvall(conn.socket, iov, iov_cnt, true);
	hasNotRecvBytes(conn, total - read_bytes);
	if (read_bytes < 0) {
		if (netWouldBlock(errno)) {
			return 1;
		}
		conn.setError(std::string("Failed to receive response: ") +
			       strerror(errno));
		return -1;
	}
	return total - read_bytes;
}

template<class BUFFER, class NETWORK>
static void
recv_cb(struct ev_loop *loop, struct ev_io *watcher, int /* revents */)
{
	using NetProvider_t = LibevNetProvider<BUFFER, NETWORK>;
	struct WaitWatcher *waitWatcher =
		reinterpret_cast<WaitWatcher *>(watcher->data);
	assert(&waitWatcher->in == watcher);
	Connection<BUFFER, NetProvider_t> *conn =
		reinterpret_cast<Connection<BUFFER, NetProvider_t> *>(waitWatcher->connection);
	int fd = waitWatcher->in.fd;
	assert(fd == conn->socket);

	timerDisable(loop, waitWatcher->timer);
	int rc = connectionReceive(*conn);
	if (rc < 0) {
		NetProvider_t *provider =
			reinterpret_cast<NetProvider_t *>(waitWatcher->provider);
		provider->close(*conn);
		return;
	}
	if (rc == 0)
		conn->readyToDecode();
}

template<class BUFFER, class NETWORK>
static inline int
connectionSend(Connection<BUFFER,  LibevNetProvider<BUFFER, NETWORK>> &conn)
{
	assert(! conn.status.is_failed);
	while (hasDataToSend(conn)) {
		size_t sent_bytes = 0;
		size_t iov_cnt = 0;
		struct iovec *iov = outBufferToIOV(conn, &iov_cnt);
		int rc = NETWORK::sendall(conn.socket, iov, iov_cnt,
					  &sent_bytes);
		hasSentBytes(conn, sent_bytes);
		if (rc != 0) {
			if (netWouldBlock(errno)) {
				conn.status.is_send_blocked = true;
				return 1;
			}
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno));
			return -1;
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class NETWORK>
static void
send_cb(struct ev_loop *loop, struct ev_io *watcher, int /* revents */)
{
	using NetProvider_t = LibevNetProvider<BUFFER, NETWORK>;
	struct WaitWatcher *waitWatcher =
		reinterpret_cast<struct WaitWatcher *>(watcher->data);
	assert(&waitWatcher->out == watcher);
	Connection<BUFFER, NetProvider_t> *conn =
		reinterpret_cast<Connection<BUFFER, NetProvider_t> *>(waitWatcher->connection);
	assert(watcher->fd == conn->socket);
	timerDisable(loop, waitWatcher->timer);
	int rc = connectionSend(*conn);
	if (rc < 0) {
		NetProvider_t *provider =
			reinterpret_cast<NetProvider_t *>(waitWatcher->provider) ;
		provider->close(*conn);
		return;
	}
	if (rc > 0) {
		/* Send is not complete, setting the write watcher. */
		LOG_DEBUG("Send is not complete, setting the write watcher");
		ev_io_start(loop, watcher);
		return;
	}
	/*
	 * send() call actually sends the whole content of buffer, so we can
	 * stop watcher for now. Rearm read watchers since we are going to
	 * receive responses from that socket.
	 */
	assert(rc == 0);
	if (ev_is_active(watcher))
		ev_io_stop(loop, watcher);
}

template<class BUFFER, class NETWORK>
LibevNetProvider<BUFFER, NETWORK>::LibevNetProvider(struct ev_loop *loop) :
	m_Loop(loop), m_IsOwnLoop(false)
{
	if (m_Loop == nullptr) {
		m_Loop = ev_default_loop(0);
		m_IsOwnLoop = true;
	}
	assert(m_Loop != nullptr);
	memset(&m_TimeoutWatcher, 0, sizeof(m_TimeoutWatcher));
	rlist_create(&m_ready_to_write);
}

template<class BUFFER, class NETWORK>
LibevNetProvider<BUFFER, NETWORK>::~LibevNetProvider()
{
	for (auto &w : m_Watchers) {
		releaseWatchers(w.first);
	}
	ev_timer_stop(m_Loop, &m_TimeoutWatcher);
	if (m_IsOwnLoop)
		ev_loop_destroy(m_Loop);
	m_Loop = nullptr;
}

template<class BUFFER, class NETWORK>
int
LibevNetProvider<BUFFER, NETWORK>::registerWatchers(Conn_t *conn, int fd)
{
	WaitWatcher *watcher = (WaitWatcher *) calloc(1, sizeof(WaitWatcher));
	if (watcher == nullptr) {
		conn->setError(std::string("Failed to allocate memory for WaitWatcher"));
		return -1;
	}

	watcher->in.data = watcher;
	watcher->out.data = watcher;
	watcher->timer = &m_TimeoutWatcher;
	watcher->connection = conn;
	watcher->provider = this;
	ev_io_init(&watcher->in, (&recv_cb<BUFFER, NETWORK>), fd, EV_READ);
	ev_io_init(&watcher->out, (&send_cb<BUFFER, NETWORK>), fd, EV_WRITE);

	m_Watchers.insert({fd, watcher});
	ev_io_start(m_Loop, &watcher->in);
	ev_io_start(m_Loop ,&watcher->out);
	return 0;
}

template<class BUFFER, class NETWORK>
int
LibevNetProvider<BUFFER, NETWORK>::connect(Conn_t &conn,
					   const std::string_view& addr,
					   unsigned port, size_t timeout)
{
	int socket = -1;
	socket = port == 0 ? NETWORK::connectUNIX(addr) :
		 NETWORK::connectINET(addr, port, timeout);
	if (socket < 0) {
		conn.setError(std::string("Failed to establish connection to ") +
			      std::string(addr));
		return -1;
	}
	LOG_DEBUG("Connected to %s, socket is %d", std::string(addr).c_str(),
		  socket);
	/* Receive and decode greetings. */
	size_t iov_cnt = 0;
	struct iovec *iov = inBufferToIOV(conn, Iproto::GREETING_SIZE, &iov_cnt);
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
	if (registerWatchers(&conn, socket) != 0) {
		conn.setError(std::string("Failed to register libev watchers"));
		::close(socket);
		return -1;
	}

	conn.socket = socket;
	return 0;
}

template<class BUFFER, class NETWORK>
void
LibevNetProvider<BUFFER, NETWORK>::close(Conn_t &conn)
{
	NETWORK::close(conn.socket);
	if (conn.socket >= 0) {
		releaseWatchers(conn.socket);
	}
	conn.socket = -1;
}

template<class BUFFER, class NETWORK>
void
LibevNetProvider<BUFFER, NETWORK>::readyToSend(Conn_t &conn)
{
	if (conn.status.is_send_blocked)
		return;
	Connection<BUFFER, LibevNetProvider> *tmp;
	/* Check if connection is already queued to be send. */
	rlist_foreach_entry(tmp, &m_ready_to_write, m_in_write) {
		if (tmp == &conn)
			return;
	}
	rlist_add_tail(&m_ready_to_write, &conn.m_in_write);
	conn.status.is_ready_to_send = true;
}

static void
timeout_cb(EV_P_ ev_timer *w, int /* revents */)
{
	(void) w;
	LOG_ERROR("Libev timed out!");
	/* Stop external loop */
	ev_break(EV_A_ EVBREAK_ONE);
}

template<class BUFFER, class NETWORK>
int
LibevNetProvider<BUFFER, NETWORK>::wait(int timeout)
{
	assert(timeout >= 0);
	ev_timer_init(&m_TimeoutWatcher, &timeout_cb, timeout / MILLISECONDS, 0 /* repeat */);
	ev_timer_start(m_Loop, &m_TimeoutWatcher);
	/* Queue pending connections to be send. */
	if (! rlist_empty(&m_ready_to_write)) {
		Connection<BUFFER, LibevNetProvider> *conn;
		rlist_foreach_entry(conn, &m_ready_to_write, m_in_write) {
			auto w = m_Watchers.find(conn->socket);
			assert(w != m_Watchers.end());
			if (! ev_is_active(&w->second->out))
				ev_feed_event(m_Loop, &w->second->out, EV_WRITE);
		}
	}
	ev_run(m_Loop, EVRUN_ONCE);
	return 0;
}

template<class BUFFER, class NETWORK>
bool
LibevNetProvider<BUFFER, NETWORK>::check(Conn_t &connection)
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
