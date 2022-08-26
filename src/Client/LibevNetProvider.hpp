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
#include <unistd.h>

#include <assert.h>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <string>
#include <string_view>

#include "Connection.hpp"
#include "ev.h"

template<class BUFFER, class Stream>
class Connector;

template<class BUFFER, class Stream>
class LibevNetProvider;

template<class BUFFER, class Stream>
struct WaitWatcher {
	WaitWatcher(Connector<BUFFER, LibevNetProvider<BUFFER, Stream>> *client,
		    Connection<BUFFER, LibevNetProvider<BUFFER, Stream>> conn,
		    struct ev_timer *t);

	struct ev_io in;
	struct ev_io out;
	Connector<BUFFER, LibevNetProvider<BUFFER, Stream>> *connector;
	Connection<BUFFER, LibevNetProvider<BUFFER, Stream>> connection;
	struct ev_timer *timer;
};

template<class BUFFER, class Stream>
WaitWatcher<BUFFER, Stream>::WaitWatcher(Connector<BUFFER, LibevNetProvider<BUFFER, Stream>> *client,
					  Connection<BUFFER, LibevNetProvider<BUFFER, Stream>> conn,
					  struct ev_timer *t) : connector(client), connection(conn),
							        timer(t)
{
	in.data = this;
	out.data = this;
}

static inline void
timerDisable(struct ev_loop *loop, struct ev_timer *timer)
{
	if (ev_is_active(timer))
		ev_timer_stop(loop, timer);
}

template<class BUFFER, class Stream>
class LibevNetProvider {
public:
	using Buffer_t = BUFFER;
	using Stream_t = Stream;
	using NetProvider_t = LibevNetProvider<BUFFER, Stream>;
	using Conn_t = Connection<BUFFER, NetProvider_t >;
	using Connector_t = Connector<BUFFER, NetProvider_t >;

	LibevNetProvider(Connector_t &connector, struct ev_loop *loop = nullptr);
	int connect(Conn_t &conn, const std::string& addr, uint16_t port);
	void close(Conn_t &conn);
	int wait(int timeout);

	~LibevNetProvider();

private:
	static constexpr float MILLISECONDS = 1000.f;

	void registerWatchers(Conn_t &conn, int fd);
	void stopWatchers(WaitWatcher<BUFFER, Stream> *watcher);

	Connector_t &m_Connector;
	std::map<int, WaitWatcher<BUFFER, Stream> *> m_Watchers;
	struct ev_loop *m_Loop;
	struct ev_timer m_TimeoutWatcher;

	bool m_IsOwnLoop;
};

template<class BUFFER, class Stream>
void
LibevNetProvider<BUFFER, Stream>::stopWatchers(struct WaitWatcher<BUFFER, Stream> *watcher)
{
	ev_io_stop(m_Loop, &watcher->in);
	ev_io_stop(m_Loop, &watcher->out);
}

template<class BUFFER, class Stream>
static inline int
connectionReceive(Connection<BUFFER,  LibevNetProvider<BUFFER, Stream>> &conn)
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
			       strerror(errno));
		return -1;
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
	}

	return 0;
}

template<class BUFFER, class Stream>
static void
recv_cb(struct ev_loop *loop, struct ev_io *watcher, int /* revents */)
{
	using NetProvider_t = LibevNetProvider<BUFFER, Stream>;
	using Connector_t = Connector<BUFFER, LibevNetProvider<BUFFER, Stream>>;

	struct WaitWatcher<BUFFER, Stream> *waitWatcher =
		reinterpret_cast<WaitWatcher<BUFFER, Stream> *>(watcher->data);
	assert(&waitWatcher->in == watcher);
	Connection<BUFFER, NetProvider_t> conn = waitWatcher->connection;
	assert(waitWatcher->in.fd == conn.get_strm().get_fd());

	timerDisable(loop, waitWatcher->timer);
	int rc = connectionReceive(conn);
	Connector_t *connector = waitWatcher->connector;
	if (rc != 0)
		return;
	if (hasDataToDecode(conn))
		connector->readyToDecode(conn);
}

template<class BUFFER, class Stream>
static inline int
connectionSend(Connection<BUFFER,  LibevNetProvider<BUFFER, Stream>> &conn)
{
	while (hasDataToSend(conn)) {
		struct iovec iov[IOVEC_MAX_SIZE];
		auto &buf = conn.getOutBuf();
		size_t iov_cnt = buf.getIOV(buf.template begin<true>(),
					    iov, IOVEC_MAX_SIZE);

		ssize_t sent = conn.get_strm().send(iov, iov_cnt);
		hasSentBytes(conn, (sent < 0 ? 0 : sent));
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno));
			return -1;
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class Stream>
static void
send_cb(struct ev_loop *loop, struct ev_io *watcher, int /* revents */)
{
	using NetProvider_t = LibevNetProvider<BUFFER, Stream>;
	using Connector_t = Connector<BUFFER, LibevNetProvider<BUFFER, Stream>>;

	struct WaitWatcher<BUFFER, Stream> *waitWatcher =
		reinterpret_cast<struct WaitWatcher<BUFFER, Stream> *>(watcher->data);
	assert(&waitWatcher->out == watcher);
	Connector_t *connector = waitWatcher->connector;
	Connection<BUFFER, NetProvider_t> &conn = waitWatcher->connection;
	assert(watcher->fd == conn.get_strm().get_fd());

	timerDisable(loop, waitWatcher->timer);
	int rc = connectionSend(conn);
	if (rc < 0) {
		connector->finishSend(conn);
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
	connector->finishSend(conn);
	if (ev_is_active(watcher))
		ev_io_stop(loop, watcher);
}

template<class BUFFER, class Stream>
LibevNetProvider<BUFFER, Stream>::LibevNetProvider(Connector_t &connector,
						    struct ev_loop *loop) :
	m_Connector(connector), m_Loop(loop), m_IsOwnLoop(false)
{
	if (m_Loop == nullptr) {
		m_Loop = ev_default_loop(0);
		m_IsOwnLoop = true;
	}
	assert(m_Loop != nullptr);
	memset(&m_TimeoutWatcher, 0, sizeof(m_TimeoutWatcher));
}

template<class BUFFER, class Stream>
LibevNetProvider<BUFFER, Stream>::~LibevNetProvider()
{
	for (auto w = m_Watchers.begin(); w != m_Watchers.end();) {
		WaitWatcher<BUFFER, Stream> *to_delete = w->second;
		stopWatchers(to_delete);
		assert(to_delete->connection.get_strm().get_fd() == w->first);
		w = m_Watchers.erase(w);
		delete to_delete;
	}

	ev_timer_stop(m_Loop, &m_TimeoutWatcher);
	if (m_IsOwnLoop)
		ev_loop_destroy(m_Loop);
	m_Loop = nullptr;
}

template<class BUFFER, class Stream>
void
LibevNetProvider<BUFFER, Stream>::registerWatchers(Conn_t &conn, int fd)
{
	WaitWatcher<BUFFER, Stream> *watcher =
		new (std::nothrow) WaitWatcher<BUFFER, Stream>(&m_Connector,
								conn, &m_TimeoutWatcher);
	if (watcher == nullptr) {
		LOG_ERROR("Failed to allocate memory for WaitWatcher");
		abort();
	}

	ev_io_init(&watcher->in, (&recv_cb<BUFFER, Stream>), fd, EV_READ);
	ev_io_init(&watcher->out, (&send_cb<BUFFER, Stream>), fd, EV_WRITE);

	m_Watchers.insert({fd, watcher});
	ev_io_start(m_Loop, &watcher->in);
	ev_io_start(m_Loop ,&watcher->out);
}

template<class BUFFER, class Stream>
int
LibevNetProvider<BUFFER, Stream>::connect(Conn_t &conn, const std::string &addr,
					  uint16_t port)
{
	auto &strm = conn.get_strm();
	std::string service = port == 0 ? std::string{} : std::to_string(port);
	if (strm.connect({
				 .address = addr,
				 .service = service,
			 }) < 0) {
		conn.setError(
			std::string("Failed to establish connection to ") +
			std::string(addr));
		return -1;
	}
	LOG_DEBUG("Connected to ", addr, ", socket is ", strm.get_fd());
	conn.getImpl()->is_greeting_received = false;

	registerWatchers(conn, strm.get_fd());
	return 0;
}

template<class BUFFER, class Stream>
void
LibevNetProvider<BUFFER, Stream>::close(Conn_t &conn)
{
	int was_fd = conn.get_strm().get_fd();
	conn.get_strm().close();
	//close can be called during libev provider destruction. In this case
	//all connections staying alive only due to the presence in m_Watchers
	//map. While cleaning up m_Watchers destructors of connections will be
	//called. So to avoid double-free presence check in m_Watchers is required.
	if (m_Watchers.find(was_fd) != m_Watchers.end()) {
		WaitWatcher<BUFFER, Stream> *w = m_Watchers[was_fd];
		stopWatchers(w);
		m_Watchers.erase(was_fd);
		delete w;
	}
}

static void
timeout_cb(EV_P_ ev_timer *w, int /* revents */)
{
	(void) w;
	LOG_ERROR("Libev timed out!");
	/* Stop external loop */
	ev_break(EV_A_ EVBREAK_ONE);
}

template<class BUFFER, class Stream>
int
LibevNetProvider<BUFFER, Stream>::wait(int timeout)
{
	assert(timeout >= 0);
	ev_timer_init(&m_TimeoutWatcher, &timeout_cb, timeout / MILLISECONDS, 0 /* repeat */);
	ev_timer_start(m_Loop, &m_TimeoutWatcher);
	/* Queue pending connections to be send. */
	for (auto conn = m_Connector.m_ReadyToSend.begin();
	     conn != m_Connector.m_ReadyToSend.end();) {
		auto w = m_Watchers.find(conn->get_strm().get_fd());
		if (w != m_Watchers.end()) {
			if (!ev_is_active(&w->second->out))
				ev_feed_event(m_Loop, &w->second->out, EV_WRITE);
			++conn;
		} else {
			conn = m_Connector.m_ReadyToSend.erase(conn);

		}
	}
	ev_run(m_Loop, EVRUN_ONCE);
	return 0;
}
