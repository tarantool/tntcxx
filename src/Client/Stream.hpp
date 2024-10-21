/*
 Copyright 2010-2022 Tarantool AUTHORS: please see AUTHORS file.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 1. Redistributions of source code must retain the above
    copyright notice, this list of conditions and the
    following disclaimer.

 2. Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.
*/
#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <ostream>

enum StreamStatus {
	/* The stream was not open or was closed. */
	SS_DEAD = 1 << 0,
	/* Non-blocking connect was initiated.. */
	SS_CONNECT_PENDING = 1 << 1,
	/* Connection.. was established. */
	SS_ESTABLISHED = 1 << 2,
	/* Mask of statuses above. Exactly on of bits must be set. */
	SS_READINESS_STATUS = SS_DEAD | SS_CONNECT_PENDING | SS_ESTABLISHED,

	/* Non-blocking read requires 'read' event in socket. */
	SS_NEED_READ_EVENT_FOR_READ = 1 << 5,
	/* Non-blocking write requires 'read' event in socket. */
	SS_NEED_READ_EVENT_FOR_WRITE = 1 << 6,
	/* Non-blocking i/o requires 'read' event in socket. */
	SS_NEED_READ_EVENT = SS_NEED_READ_EVENT_FOR_READ |
			     SS_NEED_READ_EVENT_FOR_WRITE,

	/* Non-blocking read requires 'write' event in socket. */
	SS_NEED_WRITE_EVENT_FOR_READ = 1 << 8,
	/* Non-blocking write requires 'write' event in socket. */
	SS_NEED_WRITE_EVENT_FOR_WRITE = 1 << 9,
	/* Non-blocking i/o requires 'write' event in socket. */
	SS_NEED_WRITE_EVENT = SS_NEED_WRITE_EVENT_FOR_READ |
			      SS_NEED_WRITE_EVENT_FOR_WRITE |
			      SS_CONNECT_PENDING,

	/* Non-blocking read requires some event in socket. */
	SS_NEED_EVENT_FOR_READ = SS_NEED_READ_EVENT_FOR_READ |
				 SS_NEED_WRITE_EVENT_FOR_READ,
	/* Non-blocking write requires some event in socket. */
	SS_NEED_EVENT_FOR_WRITE = SS_NEED_READ_EVENT_FOR_WRITE |
				  SS_NEED_WRITE_EVENT_FOR_WRITE,
};

enum StreamTransport {
	/* Simple non-encrypted stream. */
	STREAM_PLAIN,
	/* SSL encrypted  stream. */
	STREAM_SSL,
};

/**
 * Standard output of enum StreamTransport.
 */
inline std::ostream &
operator<<(std::ostream &strm, enum StreamTransport transport);

/**
 * Common connection options.
 */
struct ConnectOptions {
	static constexpr size_t DEFAULT_CONNECT_TIMEOUT = 2;

	/** Server address or unix socket name. */
	std::string address;
	/** Internet service or port; must be empty for unix domain socket. */
	std::string service{};
	/** Desired transport. Actual stream can reject some values. */
	StreamTransport transport = STREAM_PLAIN;
	/** Time span limit for connection establishment. */
	size_t connect_timeout = DEFAULT_CONNECT_TIMEOUT;

	/** Optional login and password. */
	std::string user{};
	std::string passwd{};

	/** SSL settings. */
	std::string ssl_cert_file{};
	std::string ssl_key_file{};
	std::string ssl_ca_file{};
	std::string ssl_ciphers{};
	std::string ssl_passwd{};
	std::string ssl_passwd_file{};

	/** Standard output. */
	friend inline std::ostream &
	operator<<(std::ostream &strm, const ConnectOptions &opts);
};

/**
 * Stream base. Hold stream status and a copy of options.
 */
class Stream {
public:
	/* Disabled copy, enabled move. */
	Stream() noexcept = default;
	~Stream() noexcept = default;
	Stream(const Stream &) = delete;
	Stream &operator=(const Stream &) = delete;
	Stream(Stream &&a) noexcept = default;
	Stream &operator=(Stream &&a) noexcept = default;

	/**
	 * Check the status bits.
	 */
	bool has_status(uint32_t st) const { return (status & st) != 0; }

	/**
	 * Get connect options.
	 */
	const ConnectOptions& get_opts() const { return opts; }

	/*
	 * The final stream class must implement the following methods:
	 * // Connect to address. Return 0 on success, -1 on error.
	 * // Pending (inprogress) connection has a successfull result.
	 * int connect(const ConnectOptions &opts);
	 * // Close connection. Reentrant.
	 * void close();
	 * // Send data to connection.
	 * // Return positive number - number of bytes sent.
	 * // Return 0 if nothing was sent but there's no error.
	 * // Return -1 on error.
	 * // One must check the stream status to understand what happens.
	 * ssize_t send(struct iovec *iov, size_t iov_count);
	 * // Receive data to connection.
	 * // Return positive number - number of bytes was received.
	 * // Return 0 if nothing was received but there's no error.
	 * // Return -1 on error.
	 * // One must check the stream status to understand what happens.
	 * ssize_t recv(struct iovec *iov, size_t iov_count);
	 */

protected:
	/**
	 * Wisely set new status. Set given bits and remove incompatible bits.
	 * Always return 0.
	 */
	inline int set_status(uint32_t st);
	/**
	 * Wisely remove status bit(s). Not intended to use with readiness bits.
	 * Always return .
	 */
	inline int remove_status(uint32_t st);

	ConnectOptions opts;

private:
	uint32_t status = SS_DEAD;
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

inline std::ostream &
operator<<(std::ostream &strm, enum StreamTransport transport)
{
	if (transport == STREAM_PLAIN)
		return strm << "plain";
	else if (transport == STREAM_SSL)
		return strm << "SSL";
	else
		return strm << "unknown transport";
}

inline std::ostream &
operator<<(std::ostream &strm, const ConnectOptions &opts)
{
	strm << opts.address;
	if (!opts.service.empty())
		strm << ':' << opts.service;
	if (opts.transport != STREAM_PLAIN)
		strm << '(' << opts.transport << ')';
	return strm;
}

inline int
Stream::set_status(uint32_t st)
{
	if (st & SS_READINESS_STATUS)
		status = st;
	else
		status |= st;
	assert(has_status(SS_ESTABLISHED) ||
	       (status & ~SS_READINESS_STATUS) == 0);
	assert(!(has_status(SS_NEED_READ_EVENT_FOR_READ) &&
		 has_status(SS_NEED_WRITE_EVENT_FOR_READ)));
	assert(!(has_status(SS_NEED_READ_EVENT_FOR_WRITE) &&
		 has_status(SS_NEED_WRITE_EVENT_FOR_WRITE)));
	return 0;
}

inline int
Stream::remove_status(uint32_t st)
{
	assert(!(st & SS_READINESS_STATUS));
	status &= ~st;
	return 0;
}
