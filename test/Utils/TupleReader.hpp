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
#include "../src/Buffer/Buffer.hpp"
#include "../src/mpp/mpp.hpp"
#include "../src/mpp/Dec.hpp"
#include "../src/Client/ResponseDecoder.hpp"

/** Corresponds to data stored in _space[512]. */
struct UserTuple {
	uint64_t field1 = 0;
	std::string field2;
	double field3 = 0.0;
	std::pair<std::string, uint64_t> field4;

	static constexpr auto mpp = std::make_tuple(
		&UserTuple::field1, &UserTuple::field2, &UserTuple::field3,
		&UserTuple::field4);
};

std::ostream&
operator<<(std::ostream& strm, const UserTuple &t)
{
	return strm << "Tuple: field1=" << t.field1 << " field2=" << t.field2 <<
		    " field3=" << t.field3 <<
		    " field4.first=" << t.field4.first <<
		    " field4.second=" << t.field4.second;
}

using Buf_t = tnt::Buffer<16 * 1024>;

template <class BUFFER>
std::vector<UserTuple>
decodeUserTuple(Data<BUFFER> &data)
{
	std::vector<UserTuple> result;
	bool ok = data.decode(result);
	if (!ok)
		return std::vector<UserTuple>();
	return result;
}
