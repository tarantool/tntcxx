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

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstring>

#include "Stream.hpp"
#include "../Utils/AddrInfo.hpp"
#include "../Utils/Logger.hpp"
#include "../Utils/Resource.hpp"

/**
 * Unix-like stream base representing socket.
 * 'Unix' in this case means that it operates with int file descriptor.
 * It can be either internet domain or unix domain socket.
 */
class UnixStream : public Stream {
public:
	/* Disabled copy, enabled move. */
	UnixStream() = default;
	~UnixStream() noexcept;
	UnixStream(const UnixStream&) = delete;
	UnixStream &operator=(const UnixStream&) = delete;
	UnixStream(UnixStream &&a) noexcept = default;
	UnixStream &operator=(UnixStream &&a) noexcept = default;

	/** Connect implementation.return 0 on success, -1 on error. */
	int connect(const ConnectOptions &opts);
	/** Close the socket. */
	void close();

	/** Get internal file descriptor of the socket. */
	int get_fd() const { return fd; }

protected:
	/** Log helpers. */
	template <class ...MSG>
	void log_wise(LogLevel level, const char *file, int line,
		      const char *msg, MSG &&...more);
	template <class ...MSG>
	int die(const char *file, int line,
		const char *msg, MSG &&...more);
	template <class ...MSG>
	int tell(StreamStatus st, const char *file, int line,
		 const char *msg, MSG&& ...more);

	/** Pending connection helper. */
	int check_pending();

private:
	Resource<int, -1> fd;
};

/**
 * Call `failed` method with current `file` and `line` and all other given
 * arguments.
 */
#ifdef US_DIE
#error "Macro name collision!"
#endif
#define US_DIE(...) die(__FILE__, __LINE__,  __VA_ARGS__)

/**
 * Call `failed` method with current `file` and `line` and all other given
 * arguments.
 */
#ifdef US_TELL
#error "Macro name collision!"
#endif
#define US_TELL(STATUS, ...) tell(STATUS, __FILE__, __LINE__,  __VA_ARGS__)

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

template <class... MSG>
void
UnixStream::log_wise(LogLevel level, const char *file, int line,
		     const char *msg, MSG&& ...more)
{
	if (sizeof...(MSG) == 0 && fd < 0)
		log(level, file, line, msg);
	else if (sizeof...(MSG) == 0)
		log(level, file, line, msg, " (", fd, ")");
	else if (fd < 0)
		log(level, file, line, msg, ": ", std::forward<MSG>(more)...);
	else
		log(level, file, line, msg, ": ", std::forward<MSG>(more)...,
		    " (", fd, ")");
}

template <class ...MSG>
int
UnixStream::die(const char *file, int line, const char *msg, MSG&& ...more)
{
	log_wise(ERROR, file, line, msg, std::forward<MSG>(more)...);
	set_status(SS_DEAD);
	return -1;
}

template <class ...MSG>
int
UnixStream::tell(StreamStatus st, const char *file, int line,
		 const char *msg, MSG&& ...more)
{
	log_wise(INFO, file, line, msg, std::forward<MSG>(more)...);
	set_status(st);
	return 0;
}

inline int
UnixStream::check_pending()
{
	assert(has_status(SS_CONNECT_PENDING));
	int err = 0;
	socklen_t len = sizeof(err);
	int rc = getsockopt(get_fd(), SOL_SOCKET, SO_ERROR, &err, &len);
	if (rc == 0 && err == 0)
		return US_TELL(SS_ESTABLISHED, "Pending connected");
	return US_DIE("Failed to connect", strerror(rc == 0 ? err : errno));
}

inline int
UnixStream::connect(const ConnectOptions &opts_arg)
{
	if (!has_status(SS_DEAD))
		return US_DIE("Double connect");

	opts = opts_arg;

	AddrInfo addr_info(opts.address, opts.service);
	if (addr_info.last_rc() != 0)
		return US_DIE("Network address resolve failed",
			      addr_info.last_error());
	int socket_errno = 0, connect_errno = 0;
	for (auto &inf: addr_info) {
		fd = ::socket(inf.ai_family, inf.ai_socktype, inf.ai_protocol);
		if (fd < 0) {
			socket_errno = errno;
			continue;
		}
		if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
			::close(fd);
			socket_errno = errno;
			continue;
		}
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
			::close(fd);
			socket_errno = errno;
			continue;
		}
		do {
			int rc = ::connect(fd, inf.ai_addr, inf.ai_addrlen);
			if (rc == 0) {
				return US_TELL(SS_ESTABLISHED,
					       "Connected", opts);
			} else if (errno == EINPROGRESS || errno == EAGAIN) {
				// TODO remove timeout and #include <poll.h>
				//return US_TELL(SS_CONNECT_PENDING,
				//	       "Connect pending", opts);

				set_status(SS_CONNECT_PENDING);
				struct pollfd fds;
				fds.fd = fd;
				fds.events = POLLOUT;
				if (poll(&fds, 1, opts.connect_timeout) == 0) {
					connect_errno = ETIMEDOUT;
					break;
				}
				if (check_pending() == 0)
					return 0;
			}
		} while (errno == EINTR);
		close();
		connect_errno = errno;
	}
	if (connect_errno != 0)
		return US_DIE("Failed to connect", strerror(connect_errno));
	else if (socket_errno != 0)
		return US_DIE("Failed to create socket",
			      strerror(socket_errno));
	else
		return US_DIE("Failed to connect");
}

inline
UnixStream::~UnixStream() noexcept
{
	close();
}

inline void
UnixStream::close()
{
	if (fd >= 0) {
		if (::close(fd) == 0)
			US_TELL(SS_DEAD, "Socket closed", fd);
		else
			US_DIE("Socket close error", strerror(errno));
		fd = -1;
	}
}
