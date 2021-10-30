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
#include "NetworkEngine.hpp"
#include "../Utils/Timer.hpp"

#include <set>

/**
 * MacOS does not have epoll so let's use Libev as default network provider.
 */
#ifdef __linux__
#include "EpollNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = EpollNetProvider<BUFFER, NetworkEngine>;
#else
#include "LibevNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = LibevNetProvider<BUFFER, NetworkEngine>;
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
		    const std::string_view& addr, unsigned port,
		    size_t timeout = DEFAULT_CONNECT_TIMEOUT);

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
	void close(int socket);
	void close(Connection<BUFFER, NetProvider> &conn);
private:
	//Timeout of Connector::connect() method.
	constexpr static size_t DEFAULT_CONNECT_TIMEOUT = 2;
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
					const std::string_view& addr,
					unsigned port, size_t timeout)
{
	//Make sure that connection is not yet established.
	assert(conn.getSocket() < 0);
	if (m_NetProvider.connect(conn, addr, port, timeout) != 0) {
		LOG_ERROR("Failed to connect to ", addr, ':', port);
		return -1;
	}
	LOG_DEBUG("Connection to ", addr, ':', port, " has been established");
	return 0;
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(int socket)
{
	m_NetProvider.close(socket);
}

template<class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(Connection<BUFFER, NetProvider> &conn)
{
	assert(conn.getSocket() >= 0);
	m_NetProvider.close(conn.getSocket());
	conn.setSocket(-1);
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
