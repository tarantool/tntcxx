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
	ResponseDecoder(BUFFER &buf) : m_Dec(buf) {};
	~ResponseDecoder() { };
	ResponseDecoder() = delete;
	ResponseDecoder(const ResponseDecoder& decoder) = delete;
	ResponseDecoder& operator = (const ResponseDecoder& decoder) = delete;

	int decodeResponse(Response<BUFFER> &response);
	int decodeResponseSize();
	void reset(const iterator_t<BUFFER> &itr);

private:
	int decodeHeader(Header &header);
	int decodeBody(Body<BUFFER> &body);
	mpp::Dec<BUFFER> m_Dec;
};

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeResponseSize()
{
	int size = -1;
	m_Dec.SetReader(false, mpp::SimpleReader<BUFFER, mpp::MP_UINT, int>{size});
	mpp::ReadResult_t res = m_Dec.Read();
	//TODO: raise more detailed error
	if (res != mpp::READ_SUCCESS)
		return -1;
	return size;
}

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeHeader(Header &header)
{
	m_Dec.SetReader(false, HeaderReader{m_Dec, header});
	mpp::ReadResult_t res = m_Dec.Read();
	if (res != mpp::READ_SUCCESS)
		return -1;
	return 0;
}

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeBody(Body<BUFFER> &body)
{
	m_Dec.SetReader(false, BodyReader{m_Dec, body});
	mpp::ReadResult_t res = m_Dec.Read();
	if (res != mpp::READ_SUCCESS)
		return -1;
	return 0;
}

template<class BUFFER>
int
ResponseDecoder<BUFFER>::decodeResponse(Response<BUFFER> &response)
{
	if (decodeHeader(response.header) != 0) {
		LOG_ERROR("Failed to decode header");
		return -1;
	}
	if (decodeBody(response.body) != 0) {
		LOG_ERROR("Failed to decode body");
		return -1;
	}
	return 0;
}

template<class BUFFER>
void
ResponseDecoder<BUFFER>::reset(const iterator_t<BUFFER> &itr)
{
	m_Dec.SetPosition(itr);
}

static inline uint32_t
versionId(unsigned major, unsigned minor, unsigned patch)
{
	return (((major << 8) | minor) << 8) | patch;
}

inline int
parseGreeting(const char *greetingbuf, Greeting &greeting) {
	if (memcmp(greetingbuf, "Tarantool ", strlen("Tarantool ")) != 0 ||
	    greetingbuf[Iproto::GREETING_SIZE / 2 - 1] != '\n' ||
	    greetingbuf[Iproto::GREETING_SIZE - 1] != '\n')
		return -1;
	int h = Iproto::GREETING_SIZE / 2;
	const char *pos = greetingbuf + strlen("Tarantool ");
	const char *end = greetingbuf + h;
	/* skip spaces */
	for (; pos < end && *pos == ' '; ++pos);
	/* Extract a version string - a string until ' ' */
	char version[20];
	const char *vend = (const char *) memchr(pos, ' ', end - pos);
	if (vend == NULL || (size_t)(vend - pos) >= sizeof(version))
		return -1;
	memcpy(version, pos, vend - pos);
	version[vend - pos] = '\0';
	pos = vend + 1;
	/* skip spaces */
	for (; pos < end && *pos == ' '; ++pos);

	/* Parse a version string */
	unsigned major, minor, patch;
	if (sscanf(version, "%u.%u.%u", &major, &minor, &patch) != 3)
		return -1;
	greeting.version_id = versionId(major, minor, patch);
	/* Decode salt */
	std::string_view salt_encoded(std::string_view(greetingbuf + h,
				      Iproto::GREETING_SALT_LEN_MAX));
	try {
		greeting.salt = base64_decode(salt_encoded);
	} catch (const std::runtime_error &e) {
		LOG_ERROR("Failed to decode salt: %s", e.what());
		return -1;
	}
	if (greeting.salt.length() < Iproto::SCRAMBLE_SIZE ||
	    greeting.salt.length() >= (uint32_t) h)
		return -1;
	return 0;
}