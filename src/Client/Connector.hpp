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
#include "Connection.hpp"
#include "DefaultNetProvider.hpp"
#include "../Utils/Timer.hpp"

template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER, NetworkEngine>>
class Connector
{
public:
	Connector();
	~Connector();
	Connector(const Connector& connector) = delete;
	Connector& operator = (const Connector& connector) = delete;

	int connect(Connection<BUFFER, NetProvider> &conn,
		    const std::string_view& addr, unsigned port,
		    size_t timeout = DEFAULT_CONNECT_TIMEOUT);
	void close(Connection<BUFFER, NetProvider> &conn);

	int wait(Connection<BUFFER, NetProvider> &conn, rid_t future,
		 int timeout = 0);
	void waitAll(Connection<BUFFER, NetProvider> &conn, rid_t *futures,
		     size_t future_count, int timeout = 0);
	Connection<BUFFER, NetProvider>* waitAny(int timeout = 0);

	/**
	 * Add to @m_ready_to_read queue and parse response.
	 * Invoked by NetworkProvider.
	 * */
	void readyToDecode(Connection<BUFFER, NetProvider> &conn);
	void readyToSend(Connection<BUFFER, NetProvider> &conn);

	constexpr static size_t DEFAULT_CONNECT_TIMEOUT = 2;
private:
	NetProvider m_NetProvider;
	/**
	 * Lists of asynchronous connections which are ready to send
	 * requests or read responses.
	 */
	struct rlist m_ready_to_read;
};

template<class BUFFER, class NetProvider>
Connector<BUFFER, NetProvider>::Connector() : m_NetProvider()
{
	rlist_create(&m_ready_to_read);
}

template<class BUFFER, class NetProvider>
Connector<BUFFER, NetProvider>::~Connector()
{
	assert(rlist_empty(&m_ready_to_read));
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connect(Connection<BUFFER, NetProvider> &conn,
					const std::string_view& addr,
					unsigned port, size_t timeout)
{
	if (conn.socket >= 0 && m_NetProvider.check(conn)) {
		LOG_ERROR("Current connection to %d is alive! Please close it "
			  "before connecting to the new address", conn.socket);
		return -1;
	}
	if (m_NetProvider.connect(conn, addr, port, timeout) != 0) {
		LOG_ERROR("Failed to connect to %s:%d",
			  std::string(addr).c_str(), port);
		LOG_ERROR("Reason: %s", conn.getError().c_str());
		return -1;
	}
	LOG_DEBUG("Connected to %s:%d has been established",
		  std::string(addr).c_str(), port);
	return 0;
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(Connection<BUFFER, NetProvider> &conn)
{
	m_NetProvider.close(conn);
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::wait(Connection<BUFFER, NetProvider> &conn,
				     rid_t future, int timeout)
{
	LOG_DEBUG("Waiting for the future %d with timeout %d", future, timeout);
	Timer timer{timeout};
	timer.start();
	while (hasDataToDecode(conn)) {
		if (conn.status.is_failed) {
			LOG_ERROR("Connection has failed. Please, handle error"
				  "and reset connection status.");
			return -1;
		}
		//TODO: skip data that can't be decoded.
		if (decodeResponse(conn) != 0)
			return -1;
	}
	if (! m_NetProvider.check(conn)) {
		LOG_ERROR("Connection has been lost: %s. Please re-connect "
			  "to the host", conn.getError().c_str());
		return -1;
	}
	while (! conn.futureIsReady(future) && !timer.isExpired()) {
		if (m_NetProvider.wait(timeout - timer.elapsed()) != 0) {
			return -1;
		}
		if (conn.status.is_failed != 0) {
			LOG_ERROR("Connection got error during wait: %s",
				  conn.getError().c_str());
			return -1;
		}
		if (conn.status.is_ready_to_decode) {
			while (hasDataToDecode(conn)) {
				LOG_DEBUG("Connection has data to decode...");
				if (decodeResponse(conn) != 0) {
					LOG_ERROR("Failed to decode response: %s",
						  conn.getError().c_str());
					return -1;
				}
			}
		}
	}
	if (! conn.futureIsReady(future)) {
		LOG_ERROR("Connection has been timed out: future is not ready");
		return -1;
	}
	LOG_DEBUG("Feature %d is ready and decoded", future);
	return 0;
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::waitAll(Connection<BUFFER, NetProvider> &conn,
					rid_t *futures, size_t future_count,
					int timeout)
{
	Timer timer{timeout};
	timer.start();
	for (size_t i = 0; i < future_count && !timer.isExpired(); ++i) {
		if (wait(conn, futures[i], timeout - timer.elapsed()) != 0) {
			conn.setError("Failed to poll: " + std::to_string(errno));
			return;
		}
		if (conn.status.is_failed) {
			LOG_ERROR("wait() on connection %p %s has failed:",
				  &conn, conn.getError());
		}
		if (timer.isExpired()) {
			LOG_WARNING("waitAll() is timed out! Only %d futures are handled", i);
			return;
		}
	}
}

//std::optional with Connection&
template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider> *
Connector<BUFFER, NetProvider>::waitAny(int timeout)
{
	Timer timer{timeout};
	timer.start();
	while (rlist_empty(&m_ready_to_read) && !timer.isExpired()) {
		m_NetProvider.wait(timeout - timer.elapsed());
	}
	using Conn_t = Connection<BUFFER, NetProvider>;
	Connection<BUFFER, NetProvider> *conn =
		rlist_first_entry(&m_ready_to_read, Conn_t, m_in_read);
	assert(conn->status.is_ready_to_decode);
	while (hasDataToDecode(*conn)) {
		if (decodeResponse(*conn) != 0)
			return nullptr;
	}
	return conn;
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToSend(Connection<BUFFER, NetProvider> &conn)
{
	LOG_DEBUG("Connection %s is ready to send", conn.toString().c_str());
	m_NetProvider.readyToSend(conn);
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToDecode(Connection<BUFFER, NetProvider> &conn)
{
	//LOG_DEBUG("Connection to socket %d is ready to decode", conn.socket);
	rlist_add_tail(&m_ready_to_read, &conn.m_in_read);
	conn.status.is_ready_to_decode = true;
}
