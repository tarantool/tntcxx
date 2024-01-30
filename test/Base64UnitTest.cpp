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

#include "../src/Utils/Base64.hpp"

#include <cassert>
#include <cstring>

#include "Utils/Helpers.hpp"

// Proof that the codec's constants are correct.
static void
gen_src_strings()
{
	char alphabets[2][65];
	for (size_t i = 0; i < 2; i++) {
		char *alphabet = alphabets[i];
		for (char c = 'A'; c <= 'Z'; c++)
			*alphabet++ = c;
		for (char c = 'a'; c <= 'z'; c++)
			*alphabet++ = c;
		for (char c = '0'; c <= '9'; c++)
			*alphabet++ = c;
		if (i == 0) {
			*alphabet++ = '+';
			*alphabet++ = '/';
		} else {
			*alphabet++ = '-';
			*alphabet++ = '_';
		}
		*alphabet = 0;
		fail_if(alphabet - alphabets[i] != 64);
	}

	char decmap[256];
	for (size_t i = 0; i < 256; i++)
		decmap[i] = -1;
	for (size_t i = 0; i < 2; i++)
		for (size_t j = 0; j < 64; j++)
			decmap[static_cast<uint8_t>(alphabets[i][j])] =
				static_cast<char>(j);
	decmap[size_t('=')] = 64;

	std::cout << "\tconst char *alphabets[2] = {\n";
	for (size_t i = 0; i < 2; i++)
		std::cout << "\t\t\"" << alphabets[i] << (i ? "\"\n" : "\",\n");
	std::cout << "\t};\n";

	std::cout << "\tconst char *decmap =\n";
	for (size_t i = 0; i < 16; i++) {
		std::cout << "\t\t\"";
		for (size_t j = 0; j < 16; j++) {
			uint8_t c = static_cast<uint8_t>(decmap[i * 16 + j]);
			std::cout << "\\"
				  << static_cast<char>(('0' + (c >> 6)))
				  << static_cast<char>(('0' + ((c >> 3) & 0x7)))
				  << static_cast<char>(('0' + (c & 0x7)));
		}
		std::cout << (i < 15 ? "\"\n" : "\";\n");
	}
}

// Encode `orig` an compare with `encoded`. Decode back and compare with `orig`.
static void
simple_test(const char *orig, size_t orig_size,
	    const char *encoded, size_t encoded_size)
{
	char buf[256];
	fail_if(encoded_size > sizeof(buf));

	{
		auto [inp, out] = base64::encode(orig, orig + orig_size, buf);
		size_t processed_src = static_cast<size_t>(inp - orig);
		size_t processed_dst = static_cast<size_t>(out - buf);
		fail_unless(processed_src == orig_size);
		fail_unless(processed_dst == base64::enc_size(orig_size));
		fail_unless(processed_dst == encoded_size);
		fail_unless(memcmp(buf, encoded, processed_dst) == 0);
	}

	{
		auto [inp, out] = base64::decode(encoded, encoded + encoded_size, buf);
		size_t processed_src = static_cast<size_t>(inp - encoded);
		size_t processed_dst = static_cast<size_t>(out - buf);
		fail_unless(processed_src == encoded_size);
		fail_unless(processed_dst == base64::dec_size(encoded_size) ||
			    processed_dst + 1 == base64::dec_size(encoded_size) ||
			    processed_dst + 2 == base64::dec_size(encoded_size));
		fail_unless(processed_dst == orig_size);
		fail_unless(memcmp(buf, orig, processed_dst) == 0);
	}

}

// Several tests that was generated with standard base64 utility.
static void
simple_tests()
{
	simple_test("", 0, "", 0);
	simple_test("a", 1, "YQ==", 4);
	simple_test("aa", 2, "YWE=", 4);
	simple_test("aaa", 3, "YWFh", 4);
	simple_test("aaaaa", 4, "YWFhYQ==", 8);
}

static void
forth_and_back_test(const char *orig, size_t orig_size)
{
	char buf[256];
	fail_if(base64::enc_size(orig_size) > sizeof(buf));

	size_t encoded_size;
	{
		auto [inp, out] = base64::encode(orig, orig + orig_size, buf);
		size_t processed_src = static_cast<size_t>(inp - orig);
		encoded_size = static_cast<size_t>(out - buf);
		fail_unless(processed_src == orig_size);
		fail_unless(encoded_size == base64::enc_size(orig_size));
	}

	char buf2[256];
	size_t decoded_size;
	{
		auto [inp, out] = base64::decode(buf, buf + encoded_size, buf2);
		size_t processed_src = static_cast<size_t>(inp - buf);
		decoded_size = static_cast<size_t>(out - buf2);
		fail_unless(processed_src == encoded_size);
		fail_unless(decoded_size == base64::dec_size(encoded_size) ||
			    decoded_size + 1 == base64::dec_size(encoded_size) ||
			    decoded_size + 2 == base64::dec_size(encoded_size));
		fail_unless(decoded_size == orig_size);
		fail_unless(memcmp(buf2, orig, orig_size) == 0);
	}
}

static void
forth_and_back_tests()
{
	constexpr size_t K = 2;
	constexpr size_t N[K] = {1024 * 1024, 128 * 1024}; // Number of runs.
	constexpr size_t M[K] = {4, 128}; // Limit of string's length.
	char buf[M[K - 1]];

	for (size_t k = 0; k < K; k++) {
		for (size_t i = 0; i < N[k]; i++) {
			size_t s = 1 + static_cast<size_t>(rand()) % (M[k] - 1);
			for (size_t j = 0; j < s; j++)
				buf[j] = static_cast<char>(rand());
			forth_and_back_test(buf, s);
		}
	}
}

static void
check_bad_ending(const char *enc,
		 size_t expected_consumed, size_t expected_produced,
		 const char *expected_dec)
{
	size_t enc_size = strlen(enc);
	char res[256];
	auto [cons_end,prod_end] = base64::decode(enc, enc + enc_size, res);
	size_t real_cons = static_cast<size_t>(cons_end - enc);
	size_t real_prod = static_cast<size_t>(prod_end - res);
	fail_unless(expected_consumed == real_cons);
	fail_unless(expected_produced == real_prod);
	fail_unless(memcmp(res, expected_dec, expected_produced) == 0);
}

static void
check_bad_endings()
{
	check_bad_ending("", 0, 0, "");

	for (size_t len = 1; len <= 4; len++) {
		char data[8];
		data[len] = 0;
		for (uint32_t mask = 0; mask < 1u << len; mask++) {
			for (size_t i = 0; i < len; i++)
				data[i] = (mask & (1u << i)) ? '*' : '=';
			check_bad_ending(data, 0, 0, "");
		}
	}

	check_bad_ending("/", 0, 0, "");
	check_bad_ending("/*", 0, 0, "");
	check_bad_ending("/=", 0, 0, "");

	check_bad_ending("/w==", 4, 1, "\xff");
	check_bad_ending("/w==*", 4, 1, "\xff");
	check_bad_ending("/w=", 3, 1, "\xff");
	check_bad_ending("/w=*", 3, 1, "\xff");
	check_bad_ending("/w", 2, 1, "\xff");
	check_bad_ending("/w*", 2, 1, "\xff");

	check_bad_ending("/x", 1, 1, "\xff");
	check_bad_ending("/x==", 1, 1, "\xff");
	check_bad_ending("/x*", 1, 1, "\xff");

	check_bad_ending("//8=", 4, 2, "\xff\xff");
	check_bad_ending("//8=*", 4, 2, "\xff\xff");
	check_bad_ending("//8", 3, 2, "\xff\xff");
	check_bad_ending("//8*", 3, 2, "\xff\xff");

	check_bad_ending("//9", 2, 2, "\xff\xff");
	check_bad_ending("//9=", 2, 2, "\xff\xff");
	check_bad_ending("//9*", 2, 2, "\xff\xff");

}

int main()
{
	gen_src_strings();
	simple_tests();
	forth_and_back_tests();
	check_bad_endings();
}
