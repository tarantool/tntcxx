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

#ifdef TNTCXX_ENABLE_SSL
#include "UnixSSLStream.hpp"
#else
#include "UnixPlainStream.hpp"
#endif

#include "../Utils/Timer.hpp"

#include <set>

#ifdef TNTCXX_ENABLE_SSL
using DefaultStream = UnixSSLStream;
#else
using DefaultStream = UnixPlainStream;
#endif

/**
 * MacOS does not have epoll so let's use Libev as default network provider.
 */
#ifdef __linux__
#include "EpollNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = EpollNetProvider<BUFFER, DefaultStream>;
#else
#include "LibevNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = LibevNetProvider<BUFFER, DefaultStream>;
#endif

template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>>
class Connector
{
public:
	Connector();
	~Connector();
	Connector(const Connector& connector) = delete;
	Connector& operator = (const Connector& connector) = delete;
	//////////////////////////////Main API//////////////////////////////////
	int connect(Connection<BUFFER, NetProvider> &conn,
		    const ConnectOptions &opts);
	int connect(Connection<BUFFER, NetProvider> &conn,
		    const std::string& addr, unsigned port);

	int wait(Connection<BUFFER, NetProvider> &conn, rid_t future,
		 int timeout = 0, Response<BUFFER> *result = nullptr);
	int waitAll(Connection<BUFFER, NetProvider> &conn,
		    const std::vector<rid_t > &futures, int timeout = 0);
	int waitCount(Connection<BUFFER, NetProvider> &conn,
		      size_t feature_count, int timeout = 0);
	////////////////////////////Service interfaces//////////////////////////
	std::optional<Connection<BUFFER, NetProvider>> waitAny(int timeout = 0);
	void readyToDecode(const Connection<BUFFER, NetProvider> &conn);
	void readyToSend(const Connection<BUFFER, NetProvider> &conn);
	void finishSend(const Connection<BUFFER, NetProvider> &conn);

	std::set<Connection<BUFFER, NetProvider>> m_ReadyToSend;
	void close(Connection<BUFFER, NetProvider> &conn);
	void close(ConnectionImpl<BUFFER, NetProvider> &conn);
private:
	NetProvider m_NetProvider;
	std::set<Connection<BUFFER, NetProvider>> m_ReadyToDecode;
};

template<class BUFFER, class NetProvider>
Connector<BUFFER, NetProvider>::Connector() : m_NetProvider(*this)
{
}

template<class BUFFER, class NetProvider>
Connector<BUFFER, NetProvider>::~Connector()
{
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connect(Connection<BUFFER, NetProvider> &conn,
					const ConnectOptions &opts)
{
	//Make sure that connection is not yet established.
	assert(conn.get_strm().has_status(SS_DEAD));
	if (m_NetProvider.connect(conn, opts) != 0) {
		LOG_ERROR("Failed to connect to ",
			  opts.address, ':', opts.service);
		return -1;
	}
	conn.getImpl()->is_greeting_received = false;
	conn.getImpl()->is_auth_required = !opts.user.empty();
	if (conn.getImpl()->is_auth_required) {
		// Encode auth request to reserve space in buffer.
		conn.prepare_auth(opts.user, opts.passwd);
	}
	LOG_DEBUG("Connection to ", opts.address, ':', opts.service,
		  " has been established");
	return 0;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connect(Connection<BUFFER, NetProvider> &conn,
					const std::string& addr,
					unsigned port)
{
	std::string service = port == 0 ? std::string{} : std::to_string(port);
	return connect(conn, {
		.address = addr,
		.service = service,
	});
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(Connection<BUFFER, NetProvider> &conn)
{
	return close(*conn.getImpl());
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(ConnectionImpl<BUFFER, NetProvider> &conn)
{
	assert(!conn.strm.has_status(SS_DEAD));
	m_NetProvider.close(conn.strm);
}

template<class BUFFER, class NetProvider>
int
connectionDecodeResponses(Connection<BUFFER, NetProvider> &conn,
			  Response<BUFFER> *result)
{
	while (hasDataToDecode(conn)) {
		DecodeStatus rc = processResponse(conn,  result);
		if (rc == DECODE_ERR)
			return -1;
		//In case we've received only a part of response
		//we should wait until the rest arrives - otherwise
		//we can't properly decode response. */
		if (rc == DECODE_NEEDMORE)
			return 0;
		assert(rc == DECODE_SUCC);
	}
	return 0;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::wait(Connection<BUFFER, NetProvider> &conn,
				     rid_t future, int timeout,
				     Response<BUFFER> *result)
{
	LOG_DEBUG("Waiting for the future ", future, " with timeout ", timeout);
	Timer timer{timeout};
	timer.start();
	if (connectionDecodeResponses(conn, result) != 0)
		return -1;
	while (! conn.futureIsReady(future) && !timer.isExpired()) {
		if (m_NetProvider.wait(timeout - timer.elapsed()) != 0) {
			conn.setError("Failed to poll: " + std::to_string(errno));
			return -1;
		}
		if (hasDataToDecode(conn)) {
			assert(m_ReadyToDecode.find(conn) != m_ReadyToDecode.end());
			if (connectionDecodeResponses(conn, result) != 0)
				return -1;
			/*
			 * In case we've handled whole data in input buffer -
			 * mark connection as completed.
			 */
			if (!hasDataToDecode(conn))
				m_ReadyToDecode.erase(conn);
		}
	}
	if (! conn.futureIsReady(future)) {
		LOG_ERROR("Connection has been timed out: future ", future,
			  " is not ready");
		return -1;
	}
	LOG_DEBUG("Feature ", future, " is ready and decoded");
	return 0;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::waitAll(Connection<BUFFER, NetProvider> &conn,
					const std::vector<rid_t> &futures,
					int timeout)
{
	Timer timer{timeout};
	timer.start();
	size_t last_not_ready = 0;
	while (!timer.isExpired()) {
		if (m_NetProvider.wait(timeout - timer.elapsed()) != 0) {
			conn.setError("Failed to poll: " + std::to_string(errno));
			return -1;
		}
		if (hasDataToDecode(conn)) {
			assert(m_ReadyToDecode.find(conn) != m_ReadyToDecode.end());
			if (connectionDecodeResponses(conn, static_cast<Response<BUFFER>*>(nullptr)) != 0)
				return -1;
			if (!hasDataToDecode(conn))
				m_ReadyToDecode.erase(conn);
		}
		bool finish = true;
		for (size_t i = last_not_ready; i < futures.size(); ++i) {
			if (!conn.futureIsReady(futures[i])) {
				finish = false;
				last_not_ready = i;
				break;
			}
		}
		if (finish)
			return 0;
	}
	LOG_ERROR("Connection has been timed out: not all futures are ready");
	return -1;
}

template<class BUFFER, class NetProvider>
std::optional<Connection<BUFFER, NetProvider>>
Connector<BUFFER, NetProvider>::waitAny(int timeout)
{
	Timer timer{timeout};
	timer.start();
	while (m_ReadyToDecode.empty() && !timer.isExpired())
		m_NetProvider.wait(timeout - timer.elapsed());
	if (m_ReadyToDecode.empty()) {
		LOG_ERROR("wait() has been timed out! No responses are received");
		return std::nullopt;
	}
	Connection<BUFFER, NetProvider> conn = *m_ReadyToDecode.begin();
	assert(hasDataToDecode(conn));
	if (connectionDecodeResponses(conn, static_cast<Response<BUFFER>*>(nullptr)) != 0)
		return std::nullopt;
	if (!hasDataToDecode(conn))
		m_ReadyToDecode.erase(conn);
	return conn;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::waitCount(Connection<BUFFER, NetProvider> &conn,
					  size_t future_count, int timeout)
{
	Timer timer{timeout};
	timer.start();
	size_t ready_futures = conn.getFutureCount();
	while (!timer.isExpired()) {
		if (m_NetProvider.wait(timeout - timer.elapsed()) != 0) {
			conn.setError("Failed to poll: " + std::to_string(errno));
			return -1;
		}
		if (hasDataToDecode(conn)) {
			assert(m_ReadyToDecode.find(conn) != m_ReadyToDecode.end());
			if (connectionDecodeResponses(conn, static_cast<Response<BUFFER>*>(nullptr)) != 0)
				return -1;
			if (!hasDataToDecode(conn))
				m_ReadyToDecode.erase(conn);
		}
		if ((conn.getFutureCount() - ready_futures) >= future_count)
			return 0;
	}
	LOG_ERROR("Connection has been timed out: only ",
		   conn.getFutureCount() - ready_futures, " are ready");
	return -1;
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToSend(const Connection<BUFFER, NetProvider> &conn)
{
	if (conn.getImpl()->is_auth_required &&
	    !conn.getImpl()->is_greeting_received) {
		// Need to receive greeting first.
		return;
	}
	m_ReadyToSend.insert(conn);
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToDecode(const Connection<BUFFER, NetProvider> &conn)
{
	m_ReadyToDecode.insert(conn);
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::finishSend(const Connection<BUFFER, NetProvider> &conn)
{
	m_ReadyToSend.erase(conn);
}
