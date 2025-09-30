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

#include "RequestEncoder.hpp"
#include "ResponseDecoder.hpp"
#include "Stream.hpp"
#include "../Utils/Logger.hpp"

#include <sys/uio.h> //iovec
#include <string>
#include <string_view>
#include <unordered_map> //futures

/** rid == request id */
typedef size_t rid_t;

static constexpr size_t CONN_READAHEAD = 64 * 1024;
static constexpr size_t IOVEC_MAX_SIZE = 32;

struct ConnectionError {
	ConnectionError(const std::string &msg, int errno_ = 0) :
		msg(msg), saved_errno(errno_)
	{
	}

	std::string msg;
	//Saved in case connection fails due to system error.
	int saved_errno = 0;
};

template <class BUFFER, class NetProvider>
class Connector;

template<class BUFFER, class NetProvider>
class Connection;

template<class BUFFER, class NetProvider>
struct ConnectionImpl
{
private:
	//Only Connection can create instances of this class
	friend class Connection<BUFFER, NetProvider>;
	using iterator = typename BUFFER::iterator;

	ConnectionImpl(Connector<BUFFER, NetProvider> &connector);
	ConnectionImpl(const ConnectionImpl& impl) = delete;
	ConnectionImpl& operator = (const ConnectionImpl& impl) = delete;
	~ConnectionImpl();

public:
	void ref();
	void unref();

	Connector<BUFFER, NetProvider> &connector;
	BUFFER inBuf;
	BUFFER outBuf;
	RequestEncoder<BUFFER> enc;
	ResponseDecoder<BUFFER> dec;
	/* Iterator separating decoded and raw data in input buffer. */
	iterator endDecoded;
	/* Network layer of the connection. */
	typename NetProvider::Stream_t strm;
	//Several connection wrappers may point to the same implementation.
	//It is useful to store connection objects in stl containers for example.
	ssize_t refs;
	//Members below can be default-initialized.
	std::optional<ConnectionError> error;
	Greeting greeting;
	bool is_greeting_received;
	bool is_auth_required;
	std::unordered_map<rid_t, Response<BUFFER>> futures;
};

template<class BUFFER, class NetProvider>
ConnectionImpl<BUFFER, NetProvider>::ConnectionImpl(Connector<BUFFER, NetProvider> &conn) :
	connector(conn), inBuf(), outBuf(), enc(outBuf), dec(inBuf),
	endDecoded(inBuf.begin()), refs(0), is_greeting_received(false),
	is_auth_required(false)
{
}

template<class BUFFER, class NetProvider>
ConnectionImpl<BUFFER, NetProvider>::~ConnectionImpl()
{
	assert(refs == 0);
	if (!strm.has_status(SS_DEAD)) {
		connector.close(*this);
	}
}

template<class BUFFER, class NetProvider>
void
ConnectionImpl<BUFFER, NetProvider>::ref()
{
	assert(refs >= 0);
	refs++;
}

template<class BUFFER, class NetProvider>
void
ConnectionImpl<BUFFER, NetProvider>::unref()
{
	assert(refs >= 1);
	if (--refs == 0)
		delete this;
}

/** Each connection is supposed to be bound to a single socket. */
template<class BUFFER, class NetProvider>
class Connection
{
public:
	class Space;
	Space space;
	using Impl_t = ConnectionImpl<BUFFER, NetProvider>;

	Connection(Connector<BUFFER, NetProvider> &connector);
	Connection(Impl_t *a);
	~Connection();
	Connection(const Connection& connection);
	Connection& operator = (const Connection& connection);

	Impl_t *getImpl() { return impl; }
	const Impl_t *getImpl() const { return impl; }

	typename NetProvider::Stream_t &get_strm() { return impl->strm; }
	const typename NetProvider::Stream_t &get_strm() const { return impl->strm; }

	//Required for storing Connections in hash tables (std::unordered_map)
	friend bool operator == (const Connection<BUFFER, NetProvider>& lhs,
				 const Connection<BUFFER, NetProvider>& rhs)
	{
		return lhs.impl == rhs.impl;
	}

	//Required for storing Connections in trees (std::map)
	friend bool operator < (const Connection<BUFFER, NetProvider>& lhs,
				const Connection<BUFFER, NetProvider>& rhs)
	{
		// TODO: remove dependency on socket.
		return lhs.get_strm().get_fd() < rhs.get_strm().get_fd();
	}

	Response<BUFFER> getResponse(rid_t future);
	bool futureIsReady(rid_t future);
	void flush();
	size_t getFutureCount() const;

	template <class T>
	rid_t call(std::string_view func, const T &args);
	rid_t ping();

	/**
	 * Execute the SQL statement contained in the 'statement' parameter.
	 * @param statement statement, which should conform to the rules for SQL grammar
	 * @param parameters tuple for placeholders in the statement
	 * @retval request id
	 */
	template <class T>
	rid_t execute(std::string_view statement, const T& parameters);

	/**
	 * Execute the SQL statement contained in the 'statement' parameter.
	 * @param stmt_id the statement id obtained with prepare()
	 * @param parameters tuple for placeholders in the statement
	 * @retval request id
	 */
	template <class T>
	rid_t execute(unsigned int stmt_id, const T& parameters);

	/**
	 * Prepare the SQL statement contained in the 'statement' parameter.
	 * The syntax and requirements for Connection::prepare() are the same
	 * as for Connection::execute().
	 * @param statement statement, which should conform to the rules for SQL grammar
	 * @retval request id
	 */
	rid_t prepare(std::string_view statement);

	void setError(const std::string &msg, int errno_ = 0);
	bool hasError() const;
	ConnectionError& getError();
	void reset();
	BUFFER& getInBuf();
	BUFFER& getOutBuf();

	template<class B, class N>
	friend
	void hasSentBytes(Connection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	void hasNotRecvBytes(Connection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	bool hasDataToSend(Connection<B, N> &conn);

	template<class B, class N>
	friend
	bool hasDataToDecode(Connection<B, N> &conn);

	template<class B, class N>
	friend
	enum DecodeStatus processResponse(Connection<B, N> &conn, int req_sync, Response<B> *result);

	template<class B, class N>
	friend
	void inputBufGC(Connection<B, N> &conn);

	template<class B, class N>
	friend
	int decodeGreeting(Connection<B, N> &conn);

	rid_t prepare_auth(std::string_view user,
			   std::string_view passwd);

	rid_t commit_auth(std::string_view user,
			  std::string_view passwd);

private:
	ConnectionImpl<BUFFER, NetProvider> *impl;
	static constexpr size_t GC_STEP_CNT = 100;
	size_t gc_step = 0;

	template <class T>
	rid_t insert(const T &tuple, uint32_t space_id);
	template <class T>
	rid_t replace(const T &tuple, uint32_t space_id);
	template <class T>
	rid_t delete_(const T &key, uint32_t space_id, uint32_t index_id);
	template <class K, class T>
	rid_t update(const K &key, const T &tuple, uint32_t space_id,
		     uint32_t index_id);
	template <class T, class O>
	rid_t upsert(const T &tuple, const O &ops, uint32_t space_id,
		     uint32_t index_base);
	template <class T>
	rid_t select(const T &key,
		     uint32_t space_id, uint32_t index_id = 0,
		     uint32_t limit = UINT32_MAX,
		     uint32_t offset = 0, IteratorType iterator = EQ);
};

/**
 * Public wrappers to access request methods in Tarantool way:
 * like box.space[space_id].replace() and
 * box.space[sid].index[iid].select()
 */
template<class BUFFER, class NetProvider>
class Connection<BUFFER, NetProvider>::Space
{
public:
	Space(Connection<BUFFER, NetProvider> &conn) :
		index(conn, *this), m_Conn(conn) {};
	Space& operator[] (uint32_t id)
	{
		space_id = id;
		return *this;
	}
	template <class T>
	rid_t insert(const T &tuple)
	{
		return m_Conn.insert(tuple, space_id);
	}
	template <class T>
	rid_t replace(const T &tuple)
	{
		return m_Conn.replace(tuple, space_id);
	}
	template <class T>
	rid_t delete_(const T &key, uint32_t index_id = 0)
	{
		return m_Conn.delete_(key, space_id, index_id);
	}
	template <class K, class T>
	rid_t update(const K &key, const T &tuple, uint32_t index_id = 0)
	{
		return m_Conn.update(key, tuple, space_id, index_id);
	}
	template <class T, class O>
	rid_t upsert(const T &tuple, const O &ops, uint32_t index_base = 0)
	{
		return m_Conn.upsert(tuple, ops, space_id, index_base);
	}
	template <class T>
	rid_t select(const T& key, uint32_t index_id = 0,
		     uint32_t limit = UINT32_MAX,
		     uint32_t offset = 0, IteratorType iterator = EQ)
	{
		return m_Conn.select(key, space_id, index_id, limit,
				     offset, iterator);
	}
	class Index {
	public:
		Index(Connection<BUFFER, NetProvider> &conn, Space &space) :
			m_Conn(conn), m_Space(space) {};
		Index& operator[] (uint32_t id)
		{
			index_id = id;
			return *this;
		}
		template <class T>
		rid_t delete_(const T &key)
		{
			return m_Conn.delete_(key, m_Space.space_id,
					      index_id);
		}
		template <class K, class T>
		rid_t update(const K &key, const T &tuple)
		{
			return m_Conn.update(key, tuple,
					     m_Space.space_id, index_id);
		}
		template <class T>
		rid_t select(const T &key,
			     uint32_t limit = UINT32_MAX,
			     uint32_t offset = 0,
			     IteratorType iterator = EQ)
		{
			return m_Conn.select(key, m_Space.space_id,
					     index_id, limit,
					     offset, iterator);
		}
	private:
		Connection<BUFFER, NetProvider> &m_Conn;
		Space &m_Space;
		uint32_t index_id;
	} index;
private:
	Connection<BUFFER, NetProvider> &m_Conn;
	uint32_t space_id;

};

template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider>::Connection(Connector<BUFFER, NetProvider> &connector) :
				   space(*this), impl(new ConnectionImpl(connector))
{
	impl->ref();
}

template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider>::Connection(ConnectionImpl<BUFFER, NetProvider> *a) :
	space(*this), impl(a)
{
	impl->ref();
}

template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider>::Connection(const Connection& connection) :
	space(*this), impl(connection.impl)
{
	impl->ref();
}

template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider>&
Connection<BUFFER, NetProvider>::operator = (const Connection& other)
{
	if (this == &other)
		return *this;
	impl->unref();
	impl = other.impl;
	impl->ref();
	return *this;
}

template<class BUFFER, class NetProvider>
Connection<BUFFER, NetProvider>::~Connection()
{
	impl->unref();
}

template<class BUFFER, class NetProvider>
Response<BUFFER>
Connection<BUFFER, NetProvider>::getResponse(rid_t future)
{
	//This method does not tolerate extracting wrong future.
	//Firstly user should invoke futureIsReady() to make sure future
	//is present.
	auto entry = impl->futures.find(future);
#ifndef NDEBUG
	if (entry == impl->futures.end())
		std::abort();
#endif
	Response<BUFFER> response = std::move(entry->second);
	impl->futures.erase(future);
	return response;
}

template<class BUFFER, class NetProvider>
bool
Connection<BUFFER, NetProvider>::futureIsReady(rid_t future)
{
	return impl->futures.find(future) != impl->futures.end();
}

template<class BUFFER, class NetProvider>
void
Connection<BUFFER, NetProvider>::flush()
{
	impl->futures.clear();
}

template<class BUFFER, class NetProvider>
size_t
Connection<BUFFER, NetProvider>::getFutureCount() const
{
	return impl->futures.size();
}

template<class BUFFER, class NetProvider>
void
Connection<BUFFER, NetProvider>::setError(const std::string &msg, int errno_)
{
	impl->error.emplace(msg, errno_);
}

template<class BUFFER, class NetProvider>
bool
Connection<BUFFER, NetProvider>::hasError() const
{
	return impl->error.has_value();
}

template<class BUFFER, class NetProvider>
ConnectionError&
Connection<BUFFER, NetProvider>::getError()
{
	assert(hasError());
	return impl->error.value();
}

template<class BUFFER, class NetProvider>
void
Connection<BUFFER, NetProvider>::reset()
{
	impl->error.reset();
}

template<class BUFFER, class NetProvider>
BUFFER&
Connection<BUFFER, NetProvider>::getInBuf()
{
	return impl->inBuf;
}

template<class BUFFER, class NetProvider>
BUFFER&
Connection<BUFFER, NetProvider>::getOutBuf()
{
	return impl->outBuf;
}

template<class BUFFER, class NetProvider>
void
hasSentBytes(Connection<BUFFER, NetProvider> &conn, size_t bytes)
{
	//dropBack()/dropFront() interfaces require number of bytes be greater
	//than zero so let's check it first.
	if (bytes > 0)
		conn.impl->outBuf.dropFront(bytes);
}

template<class BUFFER, class NetProvider>
void
hasNotRecvBytes(Connection<BUFFER, NetProvider> &conn, size_t bytes)
{
	if (bytes > 0)
		conn.impl->inBuf.dropBack(bytes);
}

template<class BUFFER, class NetProvider>
bool
hasDataToSend(Connection<BUFFER, NetProvider> &conn)
{
	//We drop content of input buffer once it has been sent. So to detect
	//if there's any data to send it's enough to check buffer's emptiness.
	return !conn.impl->outBuf.empty();
}

template<class BUFFER, class NetProvider>
bool
hasDataToDecode(Connection<BUFFER, NetProvider> &conn)
{
	assert(conn.impl->endDecoded < conn.impl->inBuf.end() ||
	       conn.impl->endDecoded == conn.impl->inBuf.end());
	return conn.impl->endDecoded != conn.impl->inBuf.end();
}

template<class BUFFER, class NetProvider>
static void
inputBufGC(Connection<BUFFER, NetProvider> &conn)
{
	if ((conn.gc_step++ % Connection<BUFFER, NetProvider>::GC_STEP_CNT) == 0) {
		LOG_DEBUG("Flushed input buffer of the connection %p", &conn);
		conn.impl->inBuf.flush();
	}
}

template<class BUFFER, class NetProvider>
DecodeStatus
processResponse(Connection<BUFFER, NetProvider> &conn, int req_sync, Response<BUFFER> *result)
{
	//Decode response. In case of success - fill in feature map
	//and adjust end-of-decoded data pointer. Call GC if needed.
	if (! conn.impl->inBuf.has(conn.impl->endDecoded, MP_RESPONSE_SIZE))
		return DECODE_NEEDMORE;

	Response<BUFFER> response;
	response.size = conn.impl->dec.decodeResponseSize();
	if (response.size < 0) {
		LOG_ERROR("Failed to decode response size");
		//In case of corrupted response size all other data in the buffer
		//is likely to be decoded in the wrong way (since we don't
		// know how much bytes should be skipped). So let's simply
		//terminate here.
		std::abort();

	}
	response.size += MP_RESPONSE_SIZE;
	if (! conn.impl->inBuf.has(conn.impl->endDecoded, response.size)) {
		//Response was received only partially. Reset decoder position
		//to the start of response to make this function re-entered.
		conn.impl->dec.reset(conn.impl->endDecoded);
		return DECODE_NEEDMORE;
	}
	if (conn.impl->dec.decodeResponse(response) != 0) {
		conn.setError("Failed to decode response, skipping bytes..");
		conn.impl->endDecoded += response.size;
		return DECODE_ERR;
	}
	LOG_DEBUG("Header: sync=", response.header.sync, ", code=",
		  response.header.code, ", schema=", response.header.schema_id);
	if (result != nullptr && response.header.sync == req_sync) {
		*result = std::move(response);
	} else {
		conn.impl->futures.insert({response.header.sync,
					   std::move(response)});
	}
	conn.impl->endDecoded += response.size;
	inputBufGC(conn);
	return DECODE_SUCC;
}

template<class BUFFER, class NetProvider>
int
decodeGreeting(Connection<BUFFER, NetProvider> &conn)
{
	//TODO: that's not zero-copy, should be rewritten in that pattern.
	assert(conn.getInBuf().has(conn.impl->endDecoded, Iproto::GREETING_SIZE));
	char greeting_buf[Iproto::GREETING_SIZE];
	conn.impl->endDecoded.read({greeting_buf, sizeof(greeting_buf)});
	conn.impl->dec.reset(conn.impl->endDecoded);
	if (parseGreeting(std::string_view{greeting_buf, Iproto::GREETING_SIZE},
			  conn.impl->greeting) != 0)
		return -1;
	conn.impl->is_greeting_received = true;
	LOG_DEBUG("Version: ", conn.impl->greeting.version_id);

#ifndef NDEBUG
	//print salt in hex format.
	char hex_salt[Iproto::MAX_SALT_SIZE * 2 + 1];
	const char *hex = "0123456789abcdef";
	for (size_t i = 0; i < conn.impl->greeting.salt_size; i++) {
		uint8_t u = conn.impl->greeting.salt[i];
		hex_salt[i * 2] = hex[u / 16];
		hex_salt[i * 2 + 1] = hex[u % 16];
	}
	hex_salt[conn.impl->greeting.salt_size * 2] = 0;
	LOG_DEBUG("Salt: ", hex_salt);
#endif
	return 0;
}

////////////////////////////BOX-like interface functions////////////////////////
template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::execute(std::string_view statement, const T& parameters)
{
    impl->enc.encodeExecute(statement, parameters);
    impl->connector.readyToSend(*this);
    return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::execute(unsigned int stmt_id, const T& parameters)
{
    impl->enc.encodeExecute(stmt_id, parameters);
    impl->connector.readyToSend(*this);
    return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
rid_t
Connection<BUFFER, NetProvider>::prepare(std::string_view statement)
{
    impl->enc.encodePrepare(statement);
    impl->connector.readyToSend(*this);
    return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::call(std::string_view func, const T &args)
{
	impl->enc.encodeCall(func, args);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
rid_t
Connection<BUFFER, NetProvider>::ping()
{
	impl->enc.encodePing();
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::insert(const T &tuple, uint32_t space_id)
{
	impl->enc.encodeInsert(tuple, space_id);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::replace(const T &tuple, uint32_t space_id)
{
	impl->enc.encodeReplace(tuple, space_id);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::delete_(const T &key, uint32_t space_id,
					 uint32_t index_id)
{
	impl->enc.encodeDelete(key, space_id, index_id);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class K, class T>
rid_t
Connection<BUFFER, NetProvider>::update(const K &key, const T &tuple,
					uint32_t space_id, uint32_t index_id)
{
	impl->enc.encodeUpdate(key, tuple, space_id, index_id);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T, class O>
rid_t
Connection<BUFFER, NetProvider>::upsert(const T &tuple, const O &ops,
					uint32_t space_id, uint32_t index_base)
{
	impl->enc.encodeUpsert(tuple, ops, space_id, index_base);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
template <class T>
rid_t
Connection<BUFFER, NetProvider>::select(const T &key, uint32_t space_id,
					uint32_t index_id, uint32_t limit,
					uint32_t offset, IteratorType iterator)
{
	impl->enc.encodeSelect(key, space_id, index_id, limit,
					       offset, iterator);
	impl->connector.readyToSend(*this);
	return impl->enc.getSync();
}

template<class BUFFER, class NetProvider>
rid_t
Connection<BUFFER, NetProvider>::prepare_auth(std::string_view user,
					      std::string_view passwd)
{
	impl->enc.encodeAuth(user, passwd, impl->greeting);
	return 0;
}

template<class BUFFER, class NetProvider>
rid_t
Connection<BUFFER, NetProvider>::commit_auth(std::string_view user,
					      std::string_view passwd)
{
	impl->enc.reencodeAuth(user, passwd, impl->greeting);;
	impl->connector.readyToSend(*this);
	return 0;
}
