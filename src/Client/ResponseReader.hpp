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

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::REQUEST_TYPE, &Header::code),
		std::make_pair(Iproto::SYNC, &Header::sync),
		std::make_pair(Iproto::SCHEMA_VERSION, &Header::schema_id)
	);
};

template<class BUFFER>
using iterator_t = typename BUFFER::iterator;

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
struct Error {
	int line;
	std::string file;
	std::string msg;
	int saved_errno;
	std::string type_name;
	int errcode;

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::ERROR_TYPE, &Error::type_name),
		std::make_pair(Iproto::ERROR_FILE, &Error::file),
		std::make_pair(Iproto::ERROR_LINE, &Error::line),
		std::make_pair(Iproto::ERROR_MESSAGE, &Error::msg),
		std::make_pair(Iproto::ERROR_ERRNO, &Error::saved_errno),
		std::make_pair(Iproto::ERROR_CODE, &Error::errcode)
	);
};

template<class BUFFER>
struct Data {
	using it_t = iterator_t<BUFFER>;
	std::pair<it_t, it_t> iters;

	/** Unpacks tuples to passed container. */
	template<class T>
	bool decode(T& tuples)
	{
		it_t itr = iters.first;
		bool ok = mpp::decode(itr, tuples);
		assert(!ok || itr == iters.second);
		return ok;
	}

	static constexpr auto mpp = &Data<BUFFER>::iters;
};

struct SqlInfo
{
	uint32_t row_count = 0;
	std::vector<uint32_t> autoincrement_ids;

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::SQL_INFO_ROW_COUNT, &SqlInfo::row_count),
		std::make_pair(Iproto::SQL_INFO_AUTOINCREMENT_IDS, &SqlInfo::autoincrement_ids)
	);
};

struct ColumnMap
{
	std::string field_name;
	std::string field_type;
	std::string collation;
	std::optional<std::string> span;
	bool is_nullable = false;
	bool is_autoincrement = false;

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::FIELD_NAME, &ColumnMap::field_name),
		std::make_pair(Iproto::FIELD_TYPE, &ColumnMap::field_type),
		std::make_pair(Iproto::FIELD_COLL, &ColumnMap::collation),
		std::make_pair(Iproto::FIELD_SPAN, &ColumnMap::span),
		std::make_pair(Iproto::FIELD_IS_NULLABLE, &ColumnMap::is_nullable),
		std::make_pair(Iproto::FIELD_IS_AUTOINCREMENT, &ColumnMap::is_autoincrement)
	);
};

struct Metadata
{
	std::vector<ColumnMap> column_maps;

	static constexpr auto mpp = &Metadata::column_maps;
};

template<class BUFFER>
struct Body {
	std::optional<std::vector<Error>> error_stack;
	std::optional<Data<BUFFER>> data;
	std::optional<SqlInfo> sql_info;
	std::optional<Metadata> metadata;
	std::optional<uint32_t> stmt_id;
	std::optional<uint32_t> bind_count;

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::DATA, &Body<BUFFER>::data),
		std::make_pair(Iproto::ERROR, std::make_tuple(std::make_pair(
			Iproto::ERROR_STACK, &Body<BUFFER>::error_stack))),
		std::make_pair(Iproto::SQL_INFO, &Body<BUFFER>::sql_info),
		std::make_pair(Iproto::METADATA, &Body<BUFFER>::metadata),
		std::make_pair(Iproto::STMT_ID, &Body<BUFFER>::stmt_id),
		std::make_pair(Iproto::BIND_COUNT, &Body<BUFFER>::bind_count)
	);
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
