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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <sys/prctl.h>

#include <iostream>

#include "Helpers.hpp"

#include "../src/Client/Connector.hpp"
#include "../src/Buffer/Buffer.hpp"
#include "../src/mpp/Dec.hpp"

const char *localhost = "127.0.0.1";
//FIXME: in case of pre-installed tarantool path is not required.
const char *tarantool_path = "/home/nikita/tarantool/src/tarantool";

int WAIT_TIMEOUT = 1000; //milliseconds

int
launchTarantool()
{
	pid_t ppid_before_fork = getpid();
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Can't launch Tarantool: fork failed! %s",
			strerror(errno));
		return -1;
	}
	if (pid == 0) {
		//int status;
		//waitpid(pid, &status, 0);
		return 0;
	}
	/* Kill child (i.e. Tarantool process) when the test is finished. */
	if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
		fprintf(stderr, "Can't launch Tarantool: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (getppid() != ppid_before_fork) {
		fprintf(stderr, "Can't launch Tarantool: parent process exited "\
				"just before prctl call");
		exit(EXIT_FAILURE);
	}
	const char * argv[] = {"cfg.lua"};
	if (execv(tarantool_path, (char * const *)argv) == -1) {
		fprintf(stderr, "Can't launch Tarantool: exec failed! %s",
			strerror(errno));
	}
	exit(EXIT_FAILURE);
}

/** Corresponds to data stored in _space[512]. */
struct UserTuple {
	uint64_t field1;
	std::string field2;
	double field3;
};

std::ostream&
operator<<(std::ostream& strm, UserTuple &t)
{
	return strm << "Tuple: field1=" << t.field1 << " field2=" << t.field2 <<
		       " field3=" << t.field3;
}

using Buf_t = tnt::Buffer<16 * 1024>;
using BufIter_t = typename Buf_t::iterator;
using Net_t = DefaultNetProvider<Buf_t >;

struct UserTupleValueReader : mpp::DefaultErrorHandler {
	explicit UserTupleValueReader(UserTuple& t) : tuple(t) {}
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
		std::cout << "expected type is " << expected <<
			     " but got " << got << std::endl;
	}

	BufIter_t* StoreEndIterator() { return nullptr; }
	UserTuple& tuple;
};

template <class BUFFER>
struct UserTupleReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_ARR> {

	UserTupleReader(mpp::Dec<BUFFER>& d, UserTuple& t) : dec(d), tuple(t) {}

	void Value(const iterator_t<BUFFER>&, mpp::compact::Type, mpp::ArrValue u)
	{
		assert(u.size == 3);
		dec.SetReader(false, UserTupleValueReader{tuple});
	}
	mpp::Dec<BUFFER>& dec;
	UserTuple& tuple;
};

template <class BUFFER>
UserTuple
decodeUserTuple(BUFFER &buf, Data<BUFFER> &data)
{
	Tuple<BUFFER> t = data.tuple;
	assert(t.begin != std::nullopt);
	assert(t.end != std::nullopt);
	UserTuple tuple;
	mpp::Dec dec(buf);
	dec.SetPosition(*t.begin);
	dec.SetReader(false, UserTupleReader<BUFFER>{dec, tuple});
	mpp::ReadResult_t res = dec.Read();
	assert(res == mpp::READ_SUCCESS);
	return tuple;
}

template <class BUFFER>
void
printResponse(Connection<BUFFER, Net_t> &conn, Response<BUFFER> &response)
{
	if (response.body.error_stack != std::nullopt) {
		Error err = (*response.body.error_stack).error;
		std::cout << "RESPONSE ERROR: msg=" << err.msg <<
			  " line=" << err.file << " file=" << err.file <<
			  " errno=" << err.saved_errno <<
			  " type=" << err.type_name <<
			  " code=" << err.errcode << std::endl;
	}
	if (response.body.data != std::nullopt) {
		Data<BUFFER> data = *response.body.data;
		if (data.tuple.begin == std::nullopt) {
			std::cout << "Empty result" << std::endl;
			return;
		}
		UserTuple tuple = decodeUserTuple(conn.getInBuf(), data);
		std::cout << tuple << std::endl;
	}
}

template <class BUFFER>
void
trivial(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn(client);
	/* Get nonexistent future. */
	std::optional<Response<Buf_t>> response = conn.getResponse(666);
	fail_unless(response == std::nullopt);
	/* Execute request without connecting to the host. */
	rid_t f = conn.ping();
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.status.is_failed);
	std::cout << conn.getError() << std::endl;
}

/** Single connection, separate/sequence pings, no errors */
template <class BUFFER>
void
single_conn_ping(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn(client);
	int rc = client.connect(conn, localhost, 3301);
	fail_unless(rc == 0);
	rid_t f = conn.ping();
	fail_unless(!conn.futureIsReady(f));
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));
	std::optional<Response<Buf_t>> response = conn.getResponse(f);
	fail_unless(response != std::nullopt);
	fail_unless(response->header.code == 0);
	f = conn.ping();
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));
	/* Second wait() should terminate immediately. */
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));
	response = conn.getResponse(f);
	fail_unless(response != std::nullopt);
	fail_unless(response->header.code == 0);
	/* Many requests at once. */
	rid_t features[3];
	features[0] = conn.ping();
	features[1] = conn.ping();
	features[2] = conn.ping();
	client.waitAll(conn, (rid_t *) &features, 3, WAIT_TIMEOUT);
	for (int i = 0; i < 3; ++i) {
		fail_unless(conn.futureIsReady(features[i]));
		response = conn.getResponse(features[i]);
		fail_unless(response != std::nullopt);
		fail_unless(response->header.code == 0);
		fail_unless(response->body.error_stack == std::nullopt);
	}
	client.close(conn);
}

/** Several connection, separate/sequence pings, no errors */
template <class BUFFER>
void
many_conn_ping(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn1(client);
	Connection<Buf_t, Net_t> conn2(client);
	Connection<Buf_t, Net_t> conn3(client);
	int rc = client.connect(conn1, localhost, 3301);
	fail_unless(rc == 0);
	/* Try to connect to the same port */
	rc = client.connect(conn2, localhost, 3301);
	fail_unless(rc == 0);
	/*
	 * Try to re-connect to another address whithout closing
	 * current connection.
	 */
	rc = client.connect(conn2, localhost, 3303);
	fail_unless(rc != 0);
	rc = client.connect(conn3, localhost, 3301);
	fail_unless(rc == 0);
	rid_t f1 = conn1.ping();
	rid_t f2 = conn2.ping();
	rid_t f3 = conn3.ping();
	Connection<Buf_t, Net_t> *conn = client.waitAny(WAIT_TIMEOUT);
	(void) conn;
	fail_unless(conn1.futureIsReady(f1) || conn2.futureIsReady(f2) ||
		    conn3.futureIsReady(f3));
	client.close(conn1);
	client.close(conn2);
	client.close(conn3);
}

/** Single connection, separate replaces */
template <class BUFFER>
void
single_conn_replace(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn(client);
	int rc = client.connect(conn, localhost, 3301);
	fail_unless(rc == 0);
	uint32_t space_id = 512;
	std::tuple data = std::make_tuple(666, "111", 1.01);
	rid_t f1 = conn.space[space_id].replace(data);
	data = std::make_tuple(777, "asd", 2.02);
	rid_t f2 = conn.space[space_id].replace(data);

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	printResponse<BUFFER>(conn, *response);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.close(conn);
}

/** Single connection, select single tuple */
template <class BUFFER>
void
single_conn_select(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn(client);
	int rc = client.connect(conn, localhost, 3301);
	fail_unless(rc == 0);
	uint32_t space_id = 512;
	uint32_t index_id = 0;
	uint32_t limit = 1;
	uint32_t offset = 0;
	IteratorType iter = IteratorType::EQ;

	auto s = conn.space[space_id];
	rid_t f1 = s.select(std::make_tuple(666));
	rid_t f2 = s.index[index_id].select(std::make_tuple(777));
	rid_t f3 = s.select(std::make_tuple(-1), index_id, limit, offset, iter);

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(conn, *response);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(conn, *response);

	client.wait(conn, f3, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f3));
	response = conn.getResponse(f3);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(conn, *response);

	client.close(conn);
}

int main()
{
//	if (launchTarantool() != 0)
//		return 1;
	Connector<Buf_t> client;
	trivial(client);
	single_conn_ping<Buf_t>(client);
	many_conn_ping<Buf_t>(client);
	single_conn_replace<Buf_t>(client);
	single_conn_select(client);
	return 0;
}
