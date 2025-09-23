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
		 int timeout = -1, Response<BUFFER> *result = nullptr);
	int waitAll(Connection<BUFFER, NetProvider> &conn,
		    const std::vector<rid_t > &futures, int timeout = -1);
	int waitCount(Connection<BUFFER, NetProvider> &conn,
		      size_t feature_count, int timeout = -1);
	std::optional<Connection<BUFFER, NetProvider>> waitAny(int timeout = -1);
	////////////////////////////Service interfaces//////////////////////////
	void readyToDecode(ConnectionImpl<BUFFER, NetProvider> *conn);
	void readyToSend(ConnectionImpl<BUFFER, NetProvider> *conn);
	void readyToSend(Connection<BUFFER, NetProvider> &conn);
	void finishSend(ConnectionImpl<BUFFER, NetProvider> *conn);

	std::set<ConnectionImpl<BUFFER, NetProvider> *> m_ReadyToSend;
	void close(Connection<BUFFER, NetProvider> &conn);
	void close(ConnectionImpl<BUFFER, NetProvider> *conn);

private:
	/**
	 * A helper to decode responses of a connection.
	 * Can be called when the connection is not ready to decode - it's just no-op.
	 * If `result` is not `nullptr`, it is used to return response for a request with
	 * `req_sync` sync. If `result` is `nullptr` - `req_sync` is ignored.
	 * Returns -1 in the case of any error, 0 on success.
	 */
	int connectionDecodeResponses(ConnectionImpl<BUFFER, NetProvider> *conn, int req_sync = -1,
				      Response<BUFFER> *result = nullptr);
	int connectionDecodeResponses(Connection<BUFFER, NetProvider> &conn, int req_sync = -1,
				      Response<BUFFER> *result = nullptr);
	/**
	 * A helper to check the readiness of requested responses.
	 * Decodes new responses from the connection and checks the readiness of the requested responses. `finish`
	 * indicates that all the requested responses are ready, and `last_not_ready` is reused between consecutive
	 * calls to this function.
	 * Returns -1 in case of any error, 0 on success.
	 */
	int connectionCheckResponsesReadiness(Connection<BUFFER, NetProvider> &conn, const std::vector<rid_t> &futures,
					      size_t *last_not_ready, bool *finish);
	/**
	 * A helper to check the readiness of at least `future_count` responses. Decodes new responses from the
	 * connection and checks that at least `future_count` responses are ready. `finish` indicates that at least
	 * `future_count` responses are ready.
	 * Returns -1 in case of any error, 0 on success.
	 */
	int connectionCheckCountResponsesReadiness(Connection<BUFFER, NetProvider> &conn, size_t future_count,
						   bool *finish);

private:
	NetProvider m_NetProvider;
	/**
	 * Set of connections that are ready to decode.
	 * Shouldn't be modified directly - is managed by methods `readyToDecode`
	 * and `connectionDecodeResponses`.
	 */
	std::set<ConnectionImpl<BUFFER, NetProvider> *> m_ReadyToDecode;
	/**
	 * Set of active connections owned by connector.
	 */
	std::set<ConnectionImpl<BUFFER, NetProvider> *> m_Connections;
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
	if (m_NetProvider.connect(conn.getImpl(), opts) != 0) {
		TNT_LOG_ERROR("Failed to connect to ", opts.address, ':', opts.service);
		return -1;
	}
	conn.getImpl()->is_greeting_received = false;
	conn.getImpl()->is_auth_required = !opts.user.empty();
	if (conn.getImpl()->is_auth_required) {
		// Encode auth request to reserve space in buffer.
		conn.getImpl()->prepare_auth(opts.user, opts.passwd);
	}
	TNT_LOG_DEBUG("Connection to ", opts.address, ':', opts.service, " has been established");
	m_Connections.insert(conn.getImpl());
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
	return close(conn.getImpl());
}

template <class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::close(ConnectionImpl<BUFFER, NetProvider> *conn)
{
	if (conn->get_strm().is_open()) {
		m_NetProvider.close(conn->get_strm());
		m_ReadyToSend.erase(conn);
		m_ReadyToDecode.erase(conn);
		m_Connections.erase(conn);
	}
}

template <class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connectionDecodeResponses(ConnectionImpl<BUFFER, NetProvider> *conn, int req_sync,
							  Response<BUFFER> *result)
{
	if (!hasDataToDecode(conn))
		return 0;

	/* Ready to decode connection must be in the corresponding set. */
	assert(m_ReadyToDecode.find(conn) != m_ReadyToDecode.end());

	int rc = 0;
	while (hasDataToDecode(conn)) {
		DecodeStatus status = processResponse(conn, req_sync, result);
		if (status == DECODE_ERR) {
			rc = -1;
			break;
		}
		//In case we've received only a part of response
		//we should wait until the rest arrives - otherwise
		//we can't properly decode response. */
		if (status == DECODE_NEEDMORE) {
			rc = 0;
			break;
		}
		assert(status == DECODE_SUCC);
	}
	/* A connection that has no data to decode must not be left in the set. */
	if (!hasDataToDecode(conn))
		m_ReadyToDecode.erase(conn);
	return rc;
}

template <class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connectionDecodeResponses(Connection<BUFFER, NetProvider> &conn, int req_sync,
							  Response<BUFFER> *result)
{
	return connectionDecodeResponses(conn.getImpl(), req_sync, result);
}

template <class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connectionCheckResponsesReadiness(Connection<BUFFER, NetProvider> &conn,
								  const std::vector<rid_t> &futures,
								  size_t *last_not_ready, bool *finish)
{
	if (conn.hasError() || connectionDecodeResponses(conn) != 0)
		return -1;
	*finish = true;
	for (size_t i = *last_not_ready; i < futures.size(); ++i) {
		if (!conn.futureIsReady(futures[i])) {
			*finish = false;
			*last_not_ready = i;
			break;
		}
	}
	return 0;
}

template <class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::connectionCheckCountResponsesReadiness(Connection<BUFFER, NetProvider> &conn,
								       size_t future_count, bool *finish)
{
	if (conn.hasError() || connectionDecodeResponses(conn) != 0)
		return -1;
	*finish = conn.getFutureCount() >= future_count;
	return 0;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::wait(Connection<BUFFER, NetProvider> &conn,
				     rid_t future, int timeout,
				     Response<BUFFER> *result)
{
	TNT_LOG_DEBUG("Waiting for the future ", future, " with timeout ", timeout);
	Timer timer{timeout};
	timer.start();
	static constexpr int INVALID_SYNC = -1;
	int req_sync = static_cast<int>(future);
	if (result != NULL)
		result->header.sync = INVALID_SYNC;
	if (connectionDecodeResponses(conn, req_sync, result) != 0)
		return -1;
	if (result != NULL && result->header.sync != INVALID_SYNC) {
		assert(result->header.sync == req_sync);
		TNT_LOG_DEBUG("Future ", future, " is ready and decoded");
		return 0;
	}
	while (!conn.hasError() && !conn.futureIsReady(future)) {
		if (m_NetProvider.wait(timer.timeLeft()) != 0) {
			conn.setError(std::string("Failed to poll: ") +
				      strerror(errno), errno);
			return -1;
		}
		if (connectionDecodeResponses(conn, req_sync, result) != 0)
			return -1;
		if (result != NULL && result->header.sync != INVALID_SYNC) {
			assert(result->header.sync == req_sync);
			TNT_LOG_DEBUG("Future ", future, " is ready and decoded");
			return 0;
		}
		if (timer.isExpired())
			break;
	}
	if (conn.hasError()) {
		TNT_LOG_ERROR("Connection got an error: ", conn.getError().msg);
		return -1;
	}
	if (! conn.futureIsReady(future)) {
		TNT_LOG_DEBUG("Connection has been timed out: future ", future, " is not ready");
		return -1;
	} else if (result != NULL) {
		*result = std::move(conn.getResponse(future));
	}
	TNT_LOG_DEBUG("Feature ", future, " is ready and decoded");
	return 0;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::waitAll(Connection<BUFFER, NetProvider> &conn,
					const std::vector<rid_t> &futures,
					int timeout)
{
	size_t last_not_ready = 0;
	bool finish = false;
	if (connectionCheckResponsesReadiness(conn, futures, &last_not_ready, &finish) != 0)
		return -1;
	if (finish)
		return 0;
	Timer timer{timeout};
	timer.start();
	while (!conn.hasError()) {
		if (m_NetProvider.wait(timer.timeLeft()) != 0) {
			conn.setError(std::string("Failed to poll: ") +
				      strerror(errno), errno);
			return -1;
		}
		if (connectionCheckResponsesReadiness(conn, futures, &last_not_ready, &finish) != 0)
			return -1;
		if (finish)
			return 0;
		if (timer.isExpired())
			break;
	}
	if (conn.hasError()) {
		TNT_LOG_ERROR("Connection got an error: ", conn.getError().msg);
		return -1;
	}
	TNT_LOG_DEBUG("Connection has been timed out: not all futures are ready");
	return -1;
}

template<class BUFFER, class NetProvider>
std::optional<Connection<BUFFER, NetProvider>>
Connector<BUFFER, NetProvider>::waitAny(int timeout)
{
	if (m_Connections.empty()) {
		TNT_LOG_DEBUG("waitAny() called on connector without connections");
		return std::nullopt;
	}
	Timer timer{timeout};
	timer.start();
	while (m_ReadyToDecode.empty()) {
		bool has_alive_conn = false;
		for (auto *conn : m_Connections) {
			if (!conn->hasError()) {
				has_alive_conn = true;
				break;
			}
		}
		if (!has_alive_conn) {
			TNT_LOG_ERROR("All connections have an error");
			return std::nullopt;
		}
		if (m_NetProvider.wait(timer.timeLeft()) != 0) {
			TNT_LOG_ERROR("Failed to poll connections: ", strerror(errno));
			return std::nullopt;
		}
		if (timer.isExpired())
			break;
	}
	if (m_ReadyToDecode.empty()) {
		TNT_LOG_DEBUG("wait() has been timed out! No responses are received");
		return std::nullopt;
	}
	auto *conn = *m_ReadyToDecode.begin();
	assert(hasDataToDecode(conn));
	if (connectionDecodeResponses(conn) != 0)
		return std::nullopt;
	return conn;
}

template<class BUFFER, class NetProvider>
int
Connector<BUFFER, NetProvider>::waitCount(Connection<BUFFER, NetProvider> &conn,
					  size_t future_count, int timeout)
{
	size_t ready_futures = conn.getFutureCount();
	size_t expected_future_count = ready_futures + future_count;
	bool finish = false;
	if (connectionCheckCountResponsesReadiness(conn, expected_future_count, &finish) != 0)
		return -1;
	if (finish)
		return 0;
	Timer timer{timeout};
	timer.start();
	while (!conn.hasError()) {
		if (m_NetProvider.wait(timer.timeLeft()) != 0) {
			conn.setError(std::string("Failed to poll: ") +
				      strerror(errno), errno);
			return -1;
		}
		if (connectionCheckCountResponsesReadiness(conn, expected_future_count, &finish) != 0)
			return -1;
		if (finish)
			return 0;
		if (timer.isExpired())
			break;
	}
	if (conn.hasError()) {
		TNT_LOG_ERROR("Connection got an error: ", conn.getError().msg);
		return -1;
	}
	TNT_LOG_DEBUG("Connection has been timed out: only ", conn.getFutureCount() - ready_futures, " are ready");
	return -1;
}

template <class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToSend(ConnectionImpl<BUFFER, NetProvider> *conn)
{
	if (conn->is_auth_required && !conn->is_greeting_received) {
		// Need to receive greeting first.
		return;
	}
	m_ReadyToSend.insert(conn);
}

template <class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToSend(Connection<BUFFER, NetProvider> &conn)
{
	readyToSend(conn.getImpl());
}

template <class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::readyToDecode(ConnectionImpl<BUFFER, NetProvider> *conn)
{
	m_ReadyToDecode.insert(conn);
}

template <class BUFFER, class NetProvider>
void
Connector<BUFFER, NetProvider>::finishSend(ConnectionImpl<BUFFER, NetProvider> *conn)
{
	m_ReadyToSend.erase(conn);
}
