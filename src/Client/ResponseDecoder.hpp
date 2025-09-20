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
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>

#include "IprotoConstants.hpp"
#include "ResponseReader.hpp"
#include "../Utils/Logger.hpp"
#include "../Utils/Base64.hpp"

enum DecodeStatus {
	DECODE_SUCC = 0,
	DECODE_ERR = -1,
	DECODE_NEEDMORE = 1
};

/** Size in bytes of encoded into msgpack size of packet*/
static constexpr size_t MP_RESPONSE_SIZE = 5;

template<class BUFFER>
class ResponseDecoder {
public:
	ResponseDecoder(BUFFER &buf) : it(buf.begin()) {};
	ResponseDecoder(iterator_t<BUFFER> &itr) : it(itr) {};
	~ResponseDecoder() { };
	ResponseDecoder() = delete;
	ResponseDecoder(const ResponseDecoder& decoder) = delete;
	ResponseDecoder& operator = (const ResponseDecoder& decoder) = delete;

	int decodeResponse(Response<BUFFER> &response);
	int decodeResponseSize();
	void reset(iterator_t<BUFFER> &itr);

private:
	iterator_t<BUFFER> it;
};

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeResponseSize()
{
	int size = -1;
	bool ok = mpp::decode(it, size);
	//TODO: raise more detailed error
	if (!ok)
		return -1;
	return size;
}

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeResponse(Response<BUFFER> &response)
{
	/* Decode header and body separately to get more detailed error. */
	if (!mpp::decode(it, response.header)) {
		TNT_LOG_ERROR("Failed to decode header");
		return -1;
	}
	if (!mpp::decode(it, response.body)) {
		TNT_LOG_ERROR("Failed to decode body");
		return -1;
	}
	return 0;
}

template<class BUFFER>
void
ResponseDecoder<BUFFER>::reset(iterator_t<BUFFER> &itr)
{
	it = itr;
}

static inline uint32_t
versionId(unsigned major, unsigned minor, unsigned patch)
{
	return (((major << 8) | minor) << 8) | patch;
}

inline int
parseGreeting(std::string_view raw, Greeting &greeting) {
	assert(raw.size() == Iproto::GREETING_SIZE);
	std::string_view line1 = raw.substr(0, Iproto::GREETING_LINE1_SIZE);
	std::string_view line2 = raw.substr(Iproto::GREETING_LINE1_SIZE,
					    Iproto::GREETING_LINE2_SIZE);
	if (line1.back() != '\n' || line2.back() != '\n')
		return -1;

	// Parse 1st line.
	std::string_view name("Tarantool ");
	if (line1.substr(0, name.size()) != name)
		return -1;
	std::string_view version_etc = line1.substr(name.size());
	version_etc = version_etc.substr(0, version_etc.find_first_of(' '));
	char version[32];
	if (version_etc.size() + 1 > sizeof(version))
		return -1;
	version[version_etc.copy(version, sizeof(version))] = '\0';
	/* Parse a version string */
	unsigned major, minor, patch;
	if (sscanf(version, "%u.%u.%u", &major, &minor, &patch) != 3)
		return -1;
	greeting.version_id = versionId(major, minor, patch);

	// Parse 2nd line.
	std::string_view salt_encoded = line2.substr(0, Iproto::GREETING_MAX_SALT_SIZE);
	char *out = base64::decode(salt_encoded.begin(), salt_encoded.end(),
				   greeting.salt).second;
	greeting.salt_size = out - greeting.salt;
	assert(greeting.salt_size <= sizeof(greeting.salt));
	if (greeting.salt_size < Iproto::SCRAMBLE_SIZE)
		return -1;
	return 0;
}
