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

#include "UnixStream.hpp"

/**
 * Unix stream that does not support SSL encryption.
 */
class UnixPlainStream : public UnixStream {
public:
	UnixPlainStream() noexcept = default;
	~UnixPlainStream() noexcept = default;
	UnixPlainStream(const UnixPlainStream&) = delete;
	UnixPlainStream &operator=(const UnixPlainStream&) = delete;
	UnixPlainStream(UnixPlainStream &&a) noexcept = default;
	UnixPlainStream &operator=(UnixPlainStream &&a) noexcept = default;

	/**
	 * Connect to address. Return 0 on success, -1 on error.
	 * Pending (inprogress) connection has a successfull result.
	 */
	int connect(const ConnectOptions &opts);
	/**
	 * Receive data to connection.
	 * Return positive number - number of bytes was received.
	 * Return 0 if nothing was sent but there's no error.
	 * Return -1 on error.
	 * One must check the stream status to understand what happens.
	 */
	ssize_t send(struct iovec *iov, size_t iov_count);
	/**
	 * Receive data to connection.
	 * Return positive number - number of bytes was received.
	 * Return 0 if nothing was received but there's no error.
	 * Return -1 on error.
	 * One must check the stream status to understand what happens.
	 */
	ssize_t recv(struct iovec *iov, size_t iov_count);
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

inline int
UnixPlainStream::connect(const ConnectOptions &opts)
{
	if (opts.transport != STREAM_PLAIN)
		US_DIE("Non-plain socket are unsupported in this build."
		       "Consider enabling it with -DTNTCXX_ENABLE_TLS.");
	return UnixStream::connect(opts);
}

namespace internal {
inline struct msghdr
create_msghdr(struct iovec *iov, size_t iov_count)
{
	struct msghdr msg{};
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_count;
	return msg;
}
} // namespace internal

inline ssize_t
UnixPlainStream::send(struct iovec *iov, size_t iov_count)
{
	if (!(has_status(SS_ESTABLISHED))) {
		if (has_status(SS_DEAD))
			return US_DIE("Send to dead stream");
		if (check_pending() != 0)
			return -1;
		if (iov_count == 0)
			return 0;
	}

	remove_status(SS_NEED_EVENT_FOR_WRITE);
	struct msghdr msg = internal::create_msghdr(iov, iov_count);
	while (true) {
		int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
		ssize_t sent = sendmsg(get_fd(), &msg, flags);

		if (sent > 0)
			return sent;
		else if (errno == EINTR)
			continue;
		else if (errno == EAGAIN || errno == EWOULDBLOCK)
			return set_status(SS_NEED_WRITE_EVENT_FOR_WRITE);
		else
			return US_DIE("Send failed", strerror(errno));
	}
}

inline ssize_t
UnixPlainStream::recv(struct iovec *iov, size_t iov_count)
{
	if (!(has_status(SS_ESTABLISHED))) {
		if (has_status(SS_DEAD))
			return US_DIE("Recv from dead stream");
		else
			return US_DIE("Recv from pending stream");
	}

	remove_status(SS_NEED_EVENT_FOR_READ);
	struct msghdr msg = internal::create_msghdr(iov, iov_count);
	while (true) {
		int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
		ssize_t rcvd = recvmsg(get_fd(), &msg, flags);

		if (rcvd > 0)
			return rcvd;
		else if (rcvd == 0)
			return US_DIE("Peer shutdown");
		else if (errno == EINTR)
			continue;
		else if (errno == EAGAIN || errno == EWOULDBLOCK)
			return set_status(SS_NEED_READ_EVENT_FOR_READ);
		else
			return US_DIE("Recv failed", strerror(errno));
	}
}
