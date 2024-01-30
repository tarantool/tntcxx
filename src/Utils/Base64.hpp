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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

/**
 * RFC4648 base64 and base64url codecs.
 * 1. Encoders use alphabets that end with "+/" and "-_" respectively.
 * 2. Decoders decode both alphabets and even mixed.
 * 3. Padding (with "=") is added by encoder but required by decoder.
 * 4. Line feeds are not added by encoder and treated as error by decoder.
 * 5. Non-alphabet characters are treated as error by decoder.
 */

#if defined(__GNUC__)
#if __GNUC__ >= 3
#define BASE64_COMPLILER_HAS_BUILTIN_EXPECT 1
#endif
#endif

#if defined(__has_builtin)
#if __has_builtin(__builtin_expect)
#define BASE64_COMPLILER_HAS_BUILTIN_EXPECT 1
#endif
#endif

#ifdef BASE64_COMPLILER_HAS_BUILTIN_EXPECT
#define BASE64_LIKELY(x)   __builtin_expect(!!(x), 1)
#define BASE64_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define BASE64_LIKELY(x)   (x)
#define BASE64_UNLIKELY(x) (x)
#endif // #idfef BASE64_COMPLILER_HAS_BUILTIN_EXPECT

namespace base64 {

enum {
	/** Use "...-_" alphabet instead of "...+/". */
	URL = 1,
};

template <class INP, class OUT>
std::pair<INP, OUT> encode(INP first, INP last, OUT dest, int options = 0)
{
	const char *alphabets[2] = {
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
	};
	const char padding = '=';
	const char *alphabet = alphabets[(options & URL) ? 1 : 0];
	assert(alphabet[63] != 0 && alphabet[64] == 0);

	while (true) {
		if (BASE64_UNLIKELY(first == last))
			return {first, dest};

		uint32_t part = static_cast<uint8_t>(*first++); // Have 8 bits.
		*dest++ = alphabet[part >> 2]; // Use high 6 bits.
		part &= 0x3; // Save low 2 bits.

		if (BASE64_UNLIKELY(first == last)) {
			*dest++ = alphabet[part << 4]; // Use saved low 2 bits.
			*dest++ = padding;
			*dest++ = padding;
			return {first, dest};
		}

		part = (part << 8) | static_cast<uint8_t>(*first++); // Have 10.
		*dest++ = alphabet[part >> 4]; // Use high 6 bits.
		part &= 0xf; // Save low 4 bits.

		if (BASE64_UNLIKELY(first == last)) {
			*dest++ = alphabet[part << 2]; // Use saved low 4 bits.
			*dest++ = padding;
			return {first, dest};
		}

		part = (part << 8) | static_cast<uint8_t>(*first++); // Have 12.
		*dest++ = alphabet[part >> 6]; // Use high 6 bits.
		*dest++ = alphabet[part & 0x3f]; // Use low 6 bits.
	}
}

template <class INP, class OUT>
std::pair<INP, OUT> decode(INP first, INP last, OUT dest)
{
	// Map to decode back from alphabet to 6bit integer (0..63).
	// Has -1 for wrong characters and 64 for padding ('=') character.
	const char *decmap =
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\076\377\076\377\077"
		"\064\065\066\067\070\071\072\073\074\075\377\377\377\100\377\377"
		"\377\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016"
		"\017\020\021\022\023\024\025\026\027\030\031\377\377\377\377\077"
		"\377\032\033\034\035\036\037\040\041\042\043\044\045\046\047\050"
		"\051\052\053\054\055\056\057\060\061\062\063\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
		"\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377";
	
	while (true) {
		if (BASE64_UNLIKELY(first == last))
			return {first, dest};

		uint8_t c = static_cast<uint8_t>(
			decmap[static_cast<uint8_t>(*first)]);
		if (BASE64_UNLIKELY(c >= 64)) // Bad char or '=' here is error.
			return {first, dest};
		uint32_t part = c; // 6 bits.

		auto next = first;
		++next;
		if (BASE64_UNLIKELY(next == last)) // Unexpected end.
			return {first, dest};

		c = static_cast<uint8_t>(decmap[static_cast<uint8_t>(*next)]);
		if (BASE64_UNLIKELY(c >= 64)) // Bad char or '=' here is error.
			return {first, dest};

		part = (part << 6) | c; // 12 useful bits.
		*dest++ = static_cast<char>(part >> 4); // 1st char is accepted.
		// Now `part` has 4 lower useful bits and garbage in upper bits.
		first = next;
		++next;

		if (BASE64_UNLIKELY(next == last)) {
			// Now end of stream is allowed.
			if (BASE64_UNLIKELY((part & 0xf) != 0))
				// Incorrect (non-zero) remainder.
				return {first, dest};

			return {next, dest};
		}

		c = static_cast<uint8_t>(decmap[static_cast<uint8_t>(*next)]);
		if (BASE64_UNLIKELY(c >= 64)) {
			if (BASE64_UNLIKELY((part & 0xf) != 0))
				// Incorrect (non-zero) remainder.
				return {first, dest};

			if (BASE64_UNLIKELY(c > 64)) // Wrong character, leave.
				return {next, dest};

			++next; // '=' character, take it.
			// If the next character is '=' we should also take it.
			if (BASE64_LIKELY(next != last && *next == '='))
				++next;

			return {next, dest};
		}

		part = (part << 6) | c; // 10 usefult bits (and garbage above).
		*dest++ = static_cast<char>(part >> 2); // 2nd char is accepted.
		// Now `part` has 2 lower useful bits and garbage in upper bits.
		first = next;
		++next;

		if (BASE64_UNLIKELY(next == last)) {
			// Now end of stream is allowed.
			if (BASE64_UNLIKELY((part & 0x3) != 0))
				// Incorrect (non-zero) remainder.
				return {first, dest};

			return {next, dest};
		}

		c = static_cast<uint8_t>(decmap[static_cast<uint8_t>(*next)]);
		if (BASE64_UNLIKELY(c >= 64)) {
			if (BASE64_UNLIKELY((part & 0x3) != 0))
				// Incorrect (non-zero) remainder.
				return {first, dest};

			if (BASE64_UNLIKELY(c > 64)) // Wrong character, leave.
				return {next, dest};

			++next; // '=' character, take it.
			return {next, dest};
		}

		part = (part << 6) | c; // 8 usefult bits (and garbage above).
		*dest++ = static_cast<char>(part); // 3rd char is accepted.
		first = next;
		++first;
	}
}

/**
 * Calculate exact buffer size required for encoding @a src_size bytes of data.
 */
inline size_t enc_size(size_t src_size)
{
	return (src_size + 2) / 3 * 4;
}

/**
 * Calculate buffer size required for decoding @a src_size bytes of data.
 * Actual size can be less by 1 or 2 because of base64 padding.
 */
inline size_t dec_size(size_t src_size)
{
	return src_size * 3 / 4;
}

} // namespace base64 {
