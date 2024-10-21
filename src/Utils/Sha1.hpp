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

#include <array>
#include <utility>

#include "../third_party/sha1.hpp"

namespace tnt {

constexpr size_t SHA1_SIZE = 20;
using Sha1_type = std::array<unsigned char, SHA1_SIZE>;

class Sha1Calc {
public:
	/** Prepare for hashing. */
	Sha1Calc();

	/** Add data to hash. */
	template <class T, class ...More>
	void add(T &&t, More &&...more);

	/** Finalize and get hash result. */
	inline Sha1_type get();

	/** Combination of add + get. */
	template <class ...T>
	Sha1_type get(T &&...t);

private:
	void add() {}

	SHA1_CTX ctx;
};

template <class ...T>
Sha1_type sha1(T &&...t);

inline void sha1_xor(Sha1_type &a, const Sha1_type &b);

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

inline
Sha1Calc::Sha1Calc()
{
	SHA1Init(&ctx);
}

template <class T, class...More>
void Sha1Calc::add(T &&t, More &&...more)
{
	SHA1Update(&ctx, std::begin(t), std::size(t));
	add(std::forward<More>(more)...);
}

Sha1_type
Sha1Calc::get()
{
	Sha1_type res;
	SHA1Final(res.data(), &ctx);
	return res;
}

template <class ...T>
Sha1_type
Sha1Calc::get(T &&...t)
{
	add(std::forward<T>(t)...);
	return get();
}

template <class ...T>
Sha1_type sha1(T &&...t)
{
	Sha1Calc calc;
	return calc.get(std::forward<T>(t)...);
}

void sha1_xor(Sha1_type &a, const Sha1_type &b)
{
	for (size_t i = 0; i < SHA1_SIZE; i++)
		a[i] ^= b[i];
}

} // namespace tnt
