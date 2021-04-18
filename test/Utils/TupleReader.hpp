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
	uint64_t field1;
	std::string field2;
	double field3;
};

std::ostream&
operator<<(std::ostream& strm, const UserTuple &t)
{
	return strm << "Tuple: field1=" << t.field1 << " field2=" << t.field2 <<
		    " field3=" << t.field3;
}

using Buf_t = tnt::Buffer<16 * 1024>;

template <class BUFFER>
struct TupleValueReader : mpp::DefaultErrorHandler {
	using BufIter_t = typename BUFFER::iterator;
	explicit TupleValueReader(mpp::Dec<BUFFER>& d, UserTuple& t) : dec(d), tuple(t) {}
	static constexpr mpp::Type VALID_TYPES = mpp::MP_UINT | mpp::MP_STR | mpp::MP_DBL;
	template <class T>
	void Value(const BufIter_t&, mpp::compact::Type, T v)
	{
		using A = UserTuple;
		static constexpr std::tuple map(&A::field1, &A::field3);
		auto ptr = std::get<std::decay_t<T> A::*>(map);
		tuple.*ptr = v;
	}
	void Value(const BufIter_t& itr, mpp::compact::Type, mpp::StrValue v)
	{
		BufIter_t tmp = itr;
		tmp += v.offset;
		std::string &dst = tuple.field2;
		while (v.size) {
			dst.push_back(*tmp);
			++tmp;
			--v.size;
		}
	}
	void WrongType(mpp::Type expected, mpp::Type got)
	{
		std::cout << "expected type is " << expected << " but got " <<
			got << std::endl;
	}
	BufIter_t* StoreEndIterator() { return nullptr; }
	mpp::Dec<BUFFER>& dec;
	UserTuple& tuple;
};

template <class BUFFER>
struct ArrayReader : mpp::DefaultErrorHandler {
	using BufIter_t = typename BUFFER::iterator;
	explicit ArrayReader(mpp::Dec<BUFFER>& d, UserTuple& t) : dec(d), tuple(t) {}
	static constexpr mpp::Type VALID_TYPES = mpp::MP_ARR;

	void Value(const BufIter_t&, mpp::compact::Type, mpp::ArrValue)
	{
		dec.SetReader(false, TupleValueReader{dec, tuple});
	}
	BufIter_t* StoreEndIterator() { return nullptr; }
	mpp::Dec<BUFFER>& dec;
	UserTuple& tuple;
};

/** Parse extra array and save iterators to the corresponding tuples. */
template <class BUFFER>
struct SelectArrayReader : mpp::DefaultErrorHandler {
	using BufIter_t = typename BUFFER::iterator;
	explicit SelectArrayReader(mpp::Dec<BUFFER>& d, std::vector<BufIter_t> &t,
				   size_t &fc) : dec(d), tuples(t),
				   field_count(fc) {}
	static constexpr mpp::Type VALID_TYPES =  mpp::MP_ANY;
	template <class T>
	void Value(const BufIter_t& arg, mpp::compact::Type, T)
	{
		BufIter_t itr = arg;
		tuples.push_back(itr);
		dec.Skip();
	}
	void Value(const BufIter_t&, mpp::compact::Type, mpp::ArrValue arr)
	{
		field_count = arr.size;
		dec.SetReader(false, SelectArrayReader{dec, tuples, field_count});
	}
	BufIter_t* StoreEndIterator() { return nullptr; }
	mpp::Dec<BUFFER>& dec;
	std::vector<BufIter_t> &tuples;
	size_t &field_count;
};

template <class BUFFER>
std::vector<UserTuple>
decodeUserTuple(BUFFER &buf, Data<BUFFER> &data)
{
	std::vector<UserTuple> results;
	for(auto const& t: data.tuples) {
		assert(t.begin != std::nullopt);
		assert(data.end != *t.begin);
		UserTuple tuple;
		mpp::Dec dec(buf);
		dec.SetPosition(*t.begin);
		dec.SetReader(false, ArrayReader<BUFFER>{dec, tuple});
		mpp::ReadResult_t res = dec.Read();
		assert(res == mpp::READ_SUCCESS);
		results.push_back(tuple);
	}
	return results;
}

template <class BUFFER>
std::vector<UserTuple>
decodeMultiReturn(BUFFER &buf, Data<BUFFER> &data)
{
	auto t = data.tuples[0];
	assert(t.begin != std::nullopt);
	assert(data.end != *t.begin);
	UserTuple tuple;
	mpp::Dec dec(buf);
	dec.SetPosition(*t.begin);
	dec.SetReader(false, TupleValueReader<BUFFER>{dec, tuple});
	for (size_t i = 0; i < data.dimension; ++i) {
		mpp::ReadResult_t res = dec.Read();
		assert(res == mpp::READ_SUCCESS);
	}
	return std::vector<UserTuple>({tuple});
}

template <class BUFFER>
std::vector<UserTuple>
decodeSelectReturn(BUFFER &buf, Data<BUFFER> &data)
{
	std::vector<UserTuple> results;
	auto t = data.tuples[0];
	mpp::Dec dec(buf);
	dec.SetPosition(*t.begin);
	std::vector<typename BUFFER::iterator> itrs;
	size_t tuple_sz = 0;
	dec.SetReader(false, SelectArrayReader{dec, itrs, tuple_sz});
	mpp::ReadResult_t res = dec.Read();
	assert(res == mpp::READ_SUCCESS);
	for (auto itr : itrs) {
		UserTuple tuple;
		dec.SetPosition(itr);
		dec.SetReader(false, TupleValueReader<BUFFER>{dec, tuple});
		for (size_t i = 0; i < tuple_sz; ++i) {
			mpp::ReadResult_t res = dec.Read();
			assert(res == mpp::READ_SUCCESS);
		}
		results.push_back(tuple);
	}
	return results;
}
