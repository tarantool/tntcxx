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
#include "Utils/Helpers.hpp"
#include "Utils/TupleReader.hpp"
#include "Utils/System.hpp"

#include "../src/Client/Connector.hpp"

const char *localhost = "127.0.0.1";
int WAIT_TIMEOUT = 1000; //milliseconds

using Net_t = DefaultNetProvider<Buf_t >;

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
		if (data.tuples.empty()) {
			std::cout << "Empty result" << std::endl;
			return;
		}
		std::vector<UserTuple> tuples =
			decodeUserTuple(conn.getInBuf(), data);
		for (auto const& t : tuples) {
			std::cout << t << std::endl;
		}
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

/** Single connection, errors in response. */
template <class BUFFER>
void
single_conn_error(Connector<BUFFER> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, Net_t> conn(client);
	int rc = client.connect(conn, localhost, 3301);
	fail_unless(rc == 0);
	/* Fake space id. */
	uint32_t space_id = -111;
	std::tuple data = std::make_tuple(666);
	rid_t f1 = conn.space[space_id].replace(data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(conn, *response);
	/* Wrong tuple format: missing fields. */
	space_id = 512;
	data = std::make_tuple(666);
	f1 = conn.space[space_id].replace(data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(conn, *response);
	/* Wrong tuple format: type mismatch. */
	space_id = 512;
	std::tuple another_data = std::make_tuple(666, "asd", "asd");
	f1 = conn.space[space_id].replace(another_data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(conn, *response);

	client.close(conn);
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
	rid_t f4 = s.select(std::make_tuple(), index_id, limit + 3, offset, IteratorType::ALL);

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

	client.wait(conn, f4, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f4));
	response = conn.getResponse(f4);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(conn, *response);

	client.close(conn);
}

int main()
{
	if (cleanDir() != 0)
		return -1;
	if (launchTarantool() != 0)
		return -1;
	sleep(1);
	Connector<Buf_t> client;
	trivial(client);
	single_conn_ping<Buf_t>(client);
	many_conn_ping<Buf_t>(client);
	single_conn_error<Buf_t>(client);
	single_conn_replace<Buf_t>(client);
	single_conn_select(client);
	return 0;
}
