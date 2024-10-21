/*
 * Copyright 2010-2022 Tarantool AUTHORS: please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#pragma once

#include <iterator>
#include <cstring>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

class AddrInfo
{
public:
	class iterator;

	AddrInfo() = default;
	AddrInfo(const std::string &addr, const std::string &service);
	inline ~AddrInfo() noexcept;
	inline int resolve(const std::string &addr, const std::string &service);
	int last_rc() const { return rc; }
	const char *last_error() const { return gai_strerror(rc); }
	inline iterator begin() const;
	static inline iterator end();

private:
	struct addrinfo* infos = nullptr;
	struct addrinfo unix_info{};
	struct sockaddr_un unix_addr{};
	int rc = 0;
};

class AddrInfo::iterator
{
public:
	using iterator_category = std::input_iterator_tag;
	using value_type = const struct addrinfo;
	using difference_type = std::ptrdiff_t;
	using pointer = const struct addrinfo *;
	using reference = const struct addrinfo&;

	value_type &operator*() const { return *info; }
	value_type *operator->() const { return info; }
	bool operator==(const iterator &a) const { return info == a.info; }
	bool operator!=(const iterator &a) const { return info != a.info; }
	inline iterator &operator++();
	inline iterator operator++(int);
private:
	friend class AddrInfo;
	explicit iterator(const struct addrinfo *ainfo) : info(ainfo) {}
	value_type *info;
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

AddrInfo::~AddrInfo() noexcept
{
	if (infos != nullptr && infos != &unix_info)
		freeaddrinfo(infos);
}

inline
AddrInfo::AddrInfo(const std::string &addr, const std::string &service)
{
	resolve(addr, service);
}

int
AddrInfo::resolve(const std::string& addr, const std::string& service)
{
	if (infos != nullptr && infos != &unix_info)
		freeaddrinfo(infos);
	infos = nullptr;

	if (service.empty() || service == "unix") {
		unix_addr.sun_family = AF_UNIX;
		constexpr size_t bufsize = sizeof(unix_addr.sun_path);
		size_t size = addr.size() < bufsize ? addr.size() : bufsize - 1;
		memcpy(unix_addr.sun_path, addr.data(), size);
		unix_addr.sun_path[size] = 0;

		unix_info.ai_family = AF_UNIX;
		unix_info.ai_socktype = SOCK_STREAM;
		unix_info.ai_addr = (struct sockaddr *) &unix_addr;
		unix_info.ai_addrlen = sizeof(unix_addr);

		infos = &unix_info;
		return rc = 0;
	}

	struct addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	return rc = getaddrinfo(addr.c_str(), service.c_str(), &hints, &infos);
}

AddrInfo::iterator
AddrInfo::begin() const
{
	return iterator(infos);
}

AddrInfo::iterator
AddrInfo::end()
{
	return iterator(nullptr);
}

AddrInfo::iterator&
AddrInfo::iterator::operator++()
{
	info = info->ai_next;
	return *this;
}

AddrInfo::iterator
AddrInfo::iterator::operator++(int)
{
	iterator tmp = *this;
	info = info->ai_next;
	return tmp;
}
