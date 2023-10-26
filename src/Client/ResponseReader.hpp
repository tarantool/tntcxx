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
#include <optional>
#include <tuple>
#include <vector>

#include "IprotoConstants.hpp"
#include "ResponseReader.hpp"
#include "../mpp/mpp.hpp"
#include "../Utils/Logger.hpp"

struct Header {
	int code;
	int sync;
	int schema_id;
};

template<class BUFFER>
using iterator_t = typename BUFFER::iterator;

struct Error {
	int line;
	char file[Iproto::DIAG_FILENAME_MAX];
	size_t file_len;
	char msg[Iproto::DIAG_ERRMSG_MAX];
	size_t msg_len;
	int saved_errno;
	char type_name[Iproto::DIAG_TYPENAME_MAX];
	size_t type_name_len;
	int errcode;
};

struct ErrorStack {
	size_t count;
	Error error;
};

template<class BUFFER>
struct Tuple {
	Tuple(iterator_t<BUFFER> &itr, size_t count) :
		begin(itr), field_count(count) {}
	iterator_t<BUFFER> begin;
	size_t field_count;
};

template<class BUFFER>
struct Data {
	Data(iterator_t<BUFFER> &itr) : end(itr) {}
	/**
	 * Data is returned in form of msgpack array (even in case of
	 * scalar value). This is size of data array.
	 */
	size_t dimension = 0;
	std::vector<Tuple<BUFFER>> tuples;
	iterator_t<BUFFER> end;
};

template<class BUFFER>
struct Body {
	std::optional<ErrorStack> error_stack;
	std::optional<Data<BUFFER>> data;
};

template<class BUFFER>
struct Response {
	Header header;
	Body<BUFFER> body;
	int size;
};

struct Greeting {
	uint32_t version_id;
	size_t salt_size;
	// Note that the salt is not null-terminated.
	char salt[Iproto::MAX_SALT_SIZE];
};

template <class BUFFER>
struct HeaderKeyReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_INT> {

	HeaderKeyReader(mpp::Dec<BUFFER>& d, Header& h) : dec(d), header(h) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, uint64_t key)
	{
		using Int_t = mpp::SimpleReader<BUFFER, mpp::MP_INT, int>;
		switch (key) {
			case Iproto::REQUEST_TYPE:
				dec.SetReader(true, Int_t{header.code});
				break;
			case Iproto::SYNC:
				dec.SetReader(true, Int_t{header.sync});
				break;
			case Iproto::SCHEMA_VERSION:
				dec.SetReader(true, Int_t{header.schema_id});
				break;
			default:
				LOG_ERROR("Invalid header key ", key);
				dec.AbortAndSkipRead();
		}
	}
	mpp::Dec<BUFFER>& dec;
	Header& header;
};

template <class BUFFER>
struct HeaderReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_MAP> {

	HeaderReader(mpp::Dec<BUFFER>& d, Header& h) : dec(d), header(h) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, HeaderKeyReader{dec, header});
	}

	mpp::Dec<BUFFER>& dec;
	Header& header;
};

template <class BUFFER>
struct TupleReader : mpp::ReaderTemplate<BUFFER> {

	TupleReader(mpp::Dec<BUFFER>& d, Data<BUFFER>& dt) : dec(d), data(dt) {}
	static constexpr mpp::Family VALID_TYPES = mpp::MP_ARR | mpp::MP_MAP |
		mpp::MP_INT | mpp::MP_BOOL | mpp::MP_FLT |
		mpp::MP_STR | mpp::MP_NIL;
	void Value(iterator_t<BUFFER>& arg, mpp::compact::Family, mpp::ArrValue u)
	{
		data.tuples.emplace_back(arg, u.size);
		dec.Skip();
	}
	void Value(iterator_t<BUFFER>& arg, mpp::compact::Family, mpp::MapValue u)
	{
		data.tuples.emplace_back(arg, u.size);
		dec.Skip();
	}
	/**
	 * Data does not necessarily contain array, it also can be scalar
	 * value. In this case store pointer right to its value.
	 */
	template <class T>
	void Value(iterator_t<BUFFER>& arg, mpp::compact::Family, T v)
	{
		(void) v;
		data.tuples.emplace_back(arg, 1);
		dec.Skip();
	}
	void WrongType(mpp::Family expected, mpp::Family got)
	{
		std::cout << "expected type is " << expected <<
			  " but got " << got << std::endl;
	}
	mpp::Dec<BUFFER>& dec;
	Data<BUFFER>& data;
};

template <class BUFFER>
struct DataReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_ARR> {

	DataReader(mpp::Dec<BUFFER>& d, Data<BUFFER>& dt) : dec(d), data(dt) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::ArrValue u)
	{
		data.dimension = u.size;
		dec.SetReader(false, TupleReader<BUFFER>{dec, data});
	}

	mpp::Dec<BUFFER>& dec;
	Data<BUFFER>& data;
};

template <class BUFFER>
struct ErrorFieldsKeyReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_STR> {

	ErrorFieldsKeyReader(mpp::Dec<BUFFER>& d, Error& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, const mpp::StrValue& v)
	{
		//TODO: assert(strcmp(v, "custom_type", sizeof("custom_type");
		(void) v;
		using TypeNameReader_t = mpp::SimpleStrReader<BUFFER, sizeof(Error{}.type_name)>;
		dec.SetReader(true, TypeNameReader_t{error.type_name, error.type_name_len});
	}
	mpp::Dec<BUFFER>& dec;
	Error& error;
};

template <class BUFFER>
struct ErrorFieldsReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_MAP> {

	ErrorFieldsReader(mpp::Dec<BUFFER>& d, Error& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, ErrorFieldsKeyReader<BUFFER>{dec, error});
	}
	mpp::Dec<BUFFER>& dec;
	Error& error;
};

template <class BUFFER>
struct ErrorKeyReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_INT> {

	ErrorKeyReader(mpp::Dec<BUFFER>& d, Error& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, uint64_t key)
	{
		using TypeNameReader_t = mpp::SimpleStrReader<BUFFER, sizeof(Error{}.type_name)>;
		using Int_t = mpp::SimpleReader<BUFFER, mpp::MP_INT, int>;
		using FileReader_t = mpp::SimpleStrReader<BUFFER, sizeof(Error{}.file)>;
		using MsgReader_t = mpp::SimpleStrReader<BUFFER, sizeof(Error{}.msg)>;
		using FieldsReader_t = ErrorFieldsReader<BUFFER>;
		//TODO: handle "access denied" and custom errors
		switch (key) {
			case Iproto::ERROR_TYPE: {
				dec.SetReader(true, TypeNameReader_t{error.type_name, error.type_name_len});
				break;
			}
			case Iproto::ERROR_LINE: {
				dec.SetReader(true, Int_t{error.line});
				break;
			}
			case Iproto::ERROR_FILE: {
				dec.SetReader(true, FileReader_t{error.file, error.file_len});
				break;
			}
			case Iproto::ERROR_MESSAGE: {
				dec.SetReader(true, MsgReader_t{error.msg, error.msg_len});
				break;
			}
			case Iproto::ERROR_ERRNO: {
				dec.SetReader(true, Int_t{error.saved_errno});
				break;
			}
			case Iproto::ERROR_CODE: {
				dec.SetReader(true, Int_t{error.errcode});
				break;
			}
			case Iproto::ERROR_FIELDS: {
				dec.SetReader(true, FieldsReader_t{dec, error});
				break;
			}
			default:
				LOG_ERROR("Invalid error key: ", key);
				dec.AbortAndSkipRead();
		}
	}
	mpp::Dec<BUFFER>& dec;
	Error& error;
};

template <class BUFFER>
struct ErrorArrayValueReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_MAP> {

	ErrorArrayValueReader(mpp::Dec<BUFFER>& d, Error& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, ErrorKeyReader<BUFFER>{dec, error});

	}
	mpp::Dec<BUFFER>& dec;
	Error& error;
};

template <class BUFFER>
struct ErrorArrayReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_ARR> {

	ErrorArrayReader(mpp::Dec<BUFFER>& d, ErrorStack& s) : dec(d), stack(s) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::ArrValue v)
	{
		stack.count = v.size;
		assert(stack.count == 1);
		dec.SetReader(false, ErrorArrayValueReader<BUFFER>{dec, stack.error});
	}
	mpp::Dec<BUFFER>& dec;
	ErrorStack& stack;
};

template <class BUFFER>
struct ErrorStackReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_INT> {

	ErrorStackReader(mpp::Dec<BUFFER>& d, ErrorStack& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, uint64_t key)
	{
		if (key != Iproto::ERROR_STACK) {
			dec.AbortAndSkipRead();
			return;
		}
		dec.SetReader(true, ErrorArrayReader<BUFFER>{dec, error});
	}
	mpp::Dec<BUFFER>& dec;
	ErrorStack& error;
};

/**
 * MP_ERROR format:
 *
 * MP_ERROR: <MP_MAP> {
 *     MP_ERROR_STACK: <MP_ARRAY> [
 *         <MP_MAP> {
 *             MP_ERROR_TYPE:  <MP_STR>,
 *             MP_ERROR_FILE: <MP_STR>,
 *             MP_ERROR_LINE: <MP_UINT>,
 *             MP_ERROR_MESSAGE: <MP_STR>,
 *             MP_ERROR_ERRNO: <MP_UINT>,
 *             MP_ERROR_CODE: <MP_UINT>,
 *             MP_ERROR_FIELDS: <MP_MAP> {
 *                 <MP_STR>: ...,
 *                 <MP_STR>: ...,
 *                 ...
 *             },
 *             ...
 *         },
 *         ...
 *     ]
 * }
 */
template <class BUFFER>
struct ErrorReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_MAP> {

	ErrorReader(mpp::Dec<BUFFER>& d, ErrorStack& er) : dec(d), error(er) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, ErrorStackReader<BUFFER>{dec, error});

	}
	mpp::Dec<BUFFER>& dec;
	ErrorStack& error;
};


template <class BUFFER>
struct BodyKeyReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_INT> {

	BodyKeyReader(mpp::Dec<BUFFER>& d, Body<BUFFER>& b) : dec(d), body(b) {}

	void Value(iterator_t<BUFFER>& itr, mpp::compact::Family, uint64_t key)
	{
		using Str_t = mpp::SimpleStrReader<BUFFER, sizeof(Error{}.msg)>;
		using Err_t = ErrorReader<BUFFER>;
		using Data_t = DataReader<BUFFER>;
		switch (key) {
			case Iproto::DATA: {
				body.data = Data<BUFFER>(itr);
				dec.SetReader(true, Data_t{dec, *body.data});
				break;
			}
			case Iproto::ERROR_24: {
				body.error_stack = ErrorStack();
				dec.SetReader(true, Str_t{body.error_stack->error.msg,
							  body.error_stack->error.msg_len});
				break;
			}
			case Iproto::ERROR: {
				/* ERROR_24 key must be parsed first. */
				assert(body.error_stack != std::nullopt);
				ErrorStack &error_stack = *body.error_stack;
				dec.SetReader(true, Err_t{dec, error_stack});
				break;
			}
			default:
				LOG_ERROR("Invalid body key: ", key);
				dec.AbortAndSkipRead();
		}
	}
	mpp::Dec<BUFFER>& dec;
	Body<BUFFER>& body;
};

template <class BUFFER>
struct BodyReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_MAP> {

	BodyReader(mpp::Dec<BUFFER>& d, Body<BUFFER>& b) : dec(d), body(b) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, BodyKeyReader{dec, body});
	}

	mpp::Dec<BUFFER>& dec;
	Body<BUFFER>& body;
};
