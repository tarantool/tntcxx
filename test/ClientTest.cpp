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
#include "Utils/System.hpp"
#include "Utils/UserTuple.hpp"

#include "../src/Client/LibevNetProvider.hpp"
#include "../src/Client/Connector.hpp"

const char *localhost = "127.0.0.1";
int port = 3301;
int dummy_server_port = 3302;
const char *unixsocket = "./tnt.sock";
int WAIT_TIMEOUT = 1000; //milliseconds

using Buf_t = tnt::Buffer<16 * 1024>;

#ifdef TNTCXX_ENABLE_SSL
constexpr bool enable_ssl = true;
constexpr StreamTransport transport = STREAM_SSL;
#else
constexpr bool enable_ssl = false;
constexpr StreamTransport transport = STREAM_PLAIN;
#endif

#ifdef __linux__
using NetProvider = EpollNetProvider<Buf_t, DefaultStream>;
#else
using NetProvider = LibevNetProvider<Buf_t, DefaultStream>;
#endif

template <class Connector, class Connection>
static int
test_connect(Connector &client, Connection &conn, const std::string &addr,
	     unsigned port,
	     const std::string user = {}, const std::string passwd = {})
{
	std::string service = port == 0 ? std::string{} : std::to_string(port);
	return client.connect(conn, {
		.address = addr,
		.service = service,
		.transport = transport,
		.user = user,
		.passwd = passwd,
	});
}

template <class Datum>
void
printDatum(const Datum &datum)
{
	if constexpr (tnt::is_pairish_v<Datum>) {
		std::cout << datum.first << ": " << datum.second << std::endl;
	} else if constexpr (tnt::is_tuplish_v<Datum>) {
		std::apply([&](auto&... datums){(
			...,
			[&](auto& child_datum) {
				printDatum(child_datum);
			}(datums));
		}, datum);
	} else if constexpr (tnt::is_optional_v<Datum>) {
		if (datum.has_value())
			std::cout << datum.value() << std::endl;
		else
			std::cout << "Nil" << std::endl;
	} else if constexpr (tnt::is_contiguous_v<Datum> &&
			     !tnt::is_contiguous_char_v<Datum>) {
		/* Consider as vector. */
		if (datum.empty()) {
			std::cout << "Empty result" << std::endl;
			return;
		}
		for (auto const& child_datum : datum) {
			printDatum(child_datum);
		}
	} else {
		std::cout << datum << std::endl;
	}

}

/**
 * Prints the response.
 * The last argument is a container to which received data will be decoded.
 * It is passed by value to assign the default argument.
 */
template <class BUFFER, class Data = std::vector<UserTuple>>
void
printResponse(Response<BUFFER> &response, Data data = std::vector<UserTuple>())
{
	if (response.body.error_stack != std::nullopt) {
		Error err = (*response.body.error_stack)[0];
		std::cout << "RESPONSE ERROR: msg=" << err.msg <<
			  " line=" << err.file << " file=" << err.file <<
			  " errno=" << err.saved_errno <<
			  " type=" << err.type_name <<
			  " code=" << err.errcode << std::endl;
		return;
	}
	if (response.body.metadata != std::nullopt) {
		std::cout << "RESPONSE SQL METADATA:" << std::endl;
		for (const auto &column : response.body.metadata->column_maps) {
			std::cout << "column=" << column.field_name <<
				" type=" << column.field_type <<
				" collation=" << column.collation <<
				" is_nullable=" << column.is_nullable <<
				" is_autoincrement" << column.is_autoincrement;
			if (column.span.has_value())
				std::cout << " span=" << column.span.value();
			std::cout << std::endl;
		}
	}
	if (response.body.data != std::nullopt) {
		if (!response.body.data->decode(data)) {
			std::cerr << "FAILED TO DECODE DATA" << std::endl;
			abort();
		}
		printDatum(data);
	} else {
		std::cout << "Request has no data" << std::endl;
	}
}

template<class BUFFER, class NetProvider>
bool
compareTupleResult(std::vector<UserTuple> &tuples,
		   std::vector<UserTuple> &expected);

template <class BUFFER, class NetProvider>
void
trivial(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	TEST_CASE("Nonexistent future");
	fail_unless(!conn.futureIsReady(666));
	/* Execute request without connecting to the host. */
	TEST_CASE("No established connection");
	rid_t f = conn.ping();
	int rc = client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(rc != 0);
	/* Connect to the wrong address. */
	TEST_CASE("Bad address");
	rc = test_connect(client, conn, "asdasd", port);
	fail_unless(rc != 0);
	TEST_CASE("Unreachable address");
	rc = test_connect(client, conn, "101.101.101", port);
	fail_unless(rc != 0);
	TEST_CASE("Wrong port");
	rc = test_connect(client, conn, localhost, -666);
	fail_unless(rc != 0);
	TEST_CASE("Connect timeout");
	rc = test_connect(client, conn, "8.8.8.8", port);
	fail_unless(rc != 0);
}

/** Single connection, separate/sequence pings, no errors */
template <class BUFFER, class NetProvider>
void
single_conn_ping(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
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
	std::vector<rid_t > features;
	features.push_back(conn.ping());
	features.push_back(conn.ping());
	features.push_back(conn.ping());
	client.waitAll(conn, features, WAIT_TIMEOUT);
	for (size_t i = 0; i < features.size(); ++i) {
		fail_unless(conn.futureIsReady(features[i]));
		response = conn.getResponse(features[i]);
		fail_unless(response != std::nullopt);
		fail_unless(response->header.code == 0);
		fail_unless(response->body.error_stack == std::nullopt);
	}
	features.clear();
	features.push_back(conn.ping());
	features.push_back(conn.ping());
	features.push_back(conn.ping());
	client.waitCount(conn, features.size(), WAIT_TIMEOUT);
	for (size_t i = 0; i < features.size(); ++i) {
		fail_unless(conn.futureIsReady(features[i]));
		response = conn.getResponse(features[i]);
		fail_unless(response != std::nullopt);
		fail_unless(response->header.code == 0);
		fail_unless(response->body.error_stack == std::nullopt);
	}
	client.close(conn);
}

template <class BUFFER, class NetProvider>
void
auto_close(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	{
		TEST_CASE("Without requests");
		Connection<Buf_t, NetProvider> conn(client);
		int rc = test_connect(client, conn, localhost, port);
		fail_unless(rc == 0);
	}
	{
		TEST_CASE("With requests");
		Connection<Buf_t, NetProvider> conn(client);
		int rc = test_connect(client, conn, localhost, port);
		fail_unless(rc == 0);

		rid_t f = conn.ping();
		fail_unless(!conn.futureIsReady(f));
		client.wait(conn, f, WAIT_TIMEOUT);
		fail_unless(conn.futureIsReady(f));
		std::optional<Response<Buf_t>> response = conn.getResponse(f);
		fail_unless(response != std::nullopt);
	}
}

/** Several connection, separate/sequence pings, no errors */
template <class BUFFER, class NetProvider>
void
many_conn_ping(void)
{
	TEST_INIT(0);
	/* FIXME(gh-123,gh-124): use own client not to leave hanging connection. */
	Connector<Buf_t, NetProvider> client;
	Connection<Buf_t, NetProvider> conn1(client);
	Connection<Buf_t, NetProvider> conn2(client);
	Connection<Buf_t, NetProvider> conn3(client);
	int rc = test_connect(client, conn1, localhost, port);
	fail_unless(rc == 0);
	/* Try to connect to the same port */
	rc = test_connect(client, conn2, localhost, port);
	fail_unless(rc == 0);
	/*
	 * Try to re-connect to another address whithout closing
	 * current connection.
	 */
	//rc = test_connect(client, conn2, localhost, port + 2);
	//fail_unless(rc != 0);
	rc = test_connect(client, conn3, localhost, port);
	fail_unless(rc == 0);
	rid_t f1 = conn1.ping();
	rid_t f2 = conn2.ping();
	rid_t f3 = conn3.ping();
	std::optional<Connection<Buf_t, NetProvider>> conn_opt = client.waitAny(WAIT_TIMEOUT);
	fail_unless(conn_opt.has_value());
	fail_unless(conn1.futureIsReady(f1) || conn2.futureIsReady(f2) ||
		    conn3.futureIsReady(f3));
	client.close(conn1);
	client.close(conn2);
	client.close(conn3);
}

/** Single connection, errors in response. */
template <class BUFFER, class NetProvider>
void
single_conn_error(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	/* Fake space id. */
	uint32_t space_id = -111;
	std::tuple data = std::make_tuple(666);
	rid_t f1 = conn.space[space_id].replace(data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	/* Wrong tuple format: missing fields. */
	space_id = 512;
	data = std::make_tuple(666);
	f1 = conn.space[space_id].replace(data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	/* Wrong tuple format: type mismatch. */
	space_id = 512;
	std::tuple another_data = std::make_tuple(666, "asd", "asd");
	f1 = conn.space[space_id].replace(another_data);
	client.wait(conn, f1, WAIT_TIMEOUT);
	response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);

	client.close(conn);
}

/** Single connection, separate replaces */
template <class BUFFER, class NetProvider>
void
single_conn_replace(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	uint32_t space_id = 512;
	std::tuple data = std::make_tuple(666, "111", 1.01);
	rid_t f1 = conn.space[space_id].replace(data);
	data = std::make_tuple(777, "asd", 2.02);
	rid_t f2 = conn.space[space_id].replace(data);

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	printResponse<BUFFER>(*response);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	printResponse<BUFFER>(*response);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.close(conn);
}

/** Single connection, separate inserts */
template <class BUFFER, class NetProvider>
void
single_conn_insert(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	TEST_CASE("Successful inserts");
	uint32_t space_id = 512;
	std::tuple data = std::make_tuple(123, "insert", 3.033);
	rid_t f1 = conn.space[space_id].insert(data);
	data = std::make_tuple(321, "another_insert", 2.022);
	rid_t f2 = conn.space[space_id].insert(data);

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	printResponse<BUFFER>(*response);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	TEST_CASE("Duplicate key during insertion");
	data = std::make_tuple(321, "another_insert", 2.022);
	rid_t f3 = conn.space[space_id].insert(data);
	client.wait(conn, f3, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f3));
	response = conn.getResponse(f3);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(*response);

	client.close(conn);
}

/** Single connection, separate updates */
template <class BUFFER, class NetProvider>
void
single_conn_update(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	TEST_CASE("Successful update");
	uint32_t space_id = 512;
	std::tuple key = std::make_tuple(123);
	std::tuple op1 = std::make_tuple("=", 1, "update");
	std::tuple op2 = std::make_tuple("+", 2, 12);
	rid_t f1 = conn.space[space_id].update(key, std::make_tuple(op1, op2));
	key = std::make_tuple(321);
	std::tuple op3 = std::make_tuple(":", 1, 2, 1, "!!");
	std::tuple op4 = std::make_tuple("-", 2, 5.05);
	rid_t f2 = conn.space[space_id].update(key, std::make_tuple(op3, op4));

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);

	client.close(conn);
}

/** Single connection, separate deletes */
template <class BUFFER, class NetProvider>
void
single_conn_delete(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	TEST_CASE("Successful deletes");
	uint32_t space_id = 512;
	std::tuple key = std::make_tuple(123);
	rid_t f1 = conn.space[space_id].delete_(key);
	key = std::make_tuple(321);
	rid_t f2 = conn.space[space_id].index[0].delete_(key);

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	printResponse<BUFFER>(*response);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	TEST_CASE("Delete by wrong key (empty response)");
	key = std::make_tuple(10101);
	rid_t f3 = conn.space[space_id].delete_(key);
	client.wait(conn, f3, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f3));
	response = conn.getResponse(f3);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	printResponse<BUFFER>(*response);

	client.close(conn);
}

/** Single connection, separate upserts */
template <class BUFFER, class NetProvider>
void
single_conn_upsert(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);
	TEST_CASE("upsert-insert");
	uint32_t space_id = 512;
	std::tuple tuple = std::make_tuple(333, "upsert-insert", 0.0);
	std::tuple op1 = std::make_tuple("=", 1, "upsert");
	rid_t f1 = conn.space[space_id].upsert(tuple, std::make_tuple(op1));

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response->body.data != std::nullopt);

	TEST_CASE("upsert-update");
	tuple = std::make_tuple(666, "111", 1.01);
	std::tuple op2 =  std::make_tuple("=", 1, "upsert-update");
	rid_t f2 = conn.space[space_id].upsert(tuple, std::make_tuple(op2));
	client.wait(conn, f2, WAIT_TIMEOUT);
	response = conn.getResponse(f2);
	fail_unless(response->body.data != std::nullopt);

	client.close(conn);
}

/** Single connection, select single tuple */
template <class BUFFER, class NetProvider>
void
single_conn_select(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
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
	printResponse<BUFFER>(*response);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(*response);

	client.wait(conn, f3, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f3));
	response = conn.getResponse(f3);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(*response);

	client.wait(conn, f4, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f4));
	response = conn.getResponse(f4);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(*response);

	client.close(conn);
}

/** Single connection, call procedure with arguments */
template <class BUFFER, class NetProvider>
void
single_conn_call(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	const static char *return_replace = "remote_replace";
	const static char *return_select  = "remote_select";
	const static char *return_uint    = "remote_uint";
	const static char *return_multi   = "remote_multi";
	const static char *return_nil     = "remote_nil";
	const static char *return_map     = "remote_map";
	const static char *echo		  = "remote_echo";

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);

	TEST_CASE("call remote_replace");
	rid_t f1 = conn.call(return_replace, std::make_tuple(5, "value_from_test", 5.55));
	rid_t f2 = conn.call(return_replace, std::make_tuple(6, "value_from_test2", 3.33));

	client.wait(conn, f1, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f1));
	std::optional<Response<Buf_t>> response = conn.getResponse(f1);
	fail_unless(response != std::nullopt);
	printResponse<BUFFER>(*response);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	client.wait(conn, f2, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f2));
	response = conn.getResponse(f2);
	fail_unless(response != std::nullopt);
	printResponse<BUFFER>(*response);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	TEST_CASE("call remote_uint");
	rid_t f4 = conn.call(return_uint, std::make_tuple());
	client.wait(conn, f4, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f4));
	response = conn.getResponse(f4);
	printResponse<BUFFER>(*response, std::make_tuple(0));

	TEST_CASE("call remote_multi");
	rid_t f5 = conn.call(return_multi, std::make_tuple());
	client.wait(conn, f5, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f5));
	response = conn.getResponse(f5);
	printResponse<BUFFER>(*response, std::make_tuple(std::string(), 0, 0.0));

	TEST_CASE("call remote_select");
	rid_t f6 = conn.call(return_select, std::make_tuple());
	client.wait(conn, f6, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f6));
	response = conn.getResponse(f6);
	printResponse<BUFFER>(*response, std::make_tuple(std::vector<UserTuple>()));

	/*
	 * Also test that errors during call are handled properly:
	 * call non-existent function and pass wrong number of arguments.
	 */
	TEST_CASE("call wrong function");
	rid_t f7 = conn.call("wrong_name", std::make_tuple(7, "aaa", 0.0));
	client.wait(conn, f7, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f7));
	response = conn.getResponse(f7);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(*response);

	TEST_CASE("call function with wrong number of arguments");
	rid_t f8 = conn.call(return_replace, std::make_tuple(7));
	client.wait(conn, f8, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f8));
	response = conn.getResponse(f8);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack != std::nullopt);
	printResponse<BUFFER>(*response);

	TEST_CASE("call remote_nil");
	rid_t f9 = conn.call(return_nil, std::make_tuple());
	client.wait(conn, f9, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f9));
	response = conn.getResponse(f9);
	printResponse<BUFFER>(*response, std::make_tuple(std::optional<int>()));

	TEST_CASE("call remote_map");
	rid_t f10 = conn.call(return_map, std::make_tuple());
	client.wait(conn, f10, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f10));
	response = conn.getResponse(f10);
	printResponse<BUFFER>(*response,
		std::make_tuple(std::make_tuple(std::make_pair("key", int()))));

	TEST_CASE("call remote_echo with raw arguments");
	/* [1, 2, 3] as a raw MsgPack. */
	const char raw_data[4] = {static_cast<char>(0x93), 1, 2, 3};
	rid_t f11 = conn.call(echo, mpp::as_raw(raw_data));
	client.wait(conn, f11, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f11));
	response = conn.getResponse(f11);
	printResponse<BUFFER>(*response, std::make_tuple(std::make_tuple(0, 0, 0)));

	client.close(conn);
}

/** Statement processor that returns the string statement as-is. */
class StmtProcessorNoop {
public:
	template<class BUFFER, class NetProvider>
	static std::string&
	process(Connector<BUFFER, NetProvider> &client,
		Connection<Buf_t, NetProvider> &conn,
		std::string &stmt)
	{
		(void)client;
		(void)conn;
		return stmt;
	}
};

/** Statement processor that prepares statement and returns its id. */
class StmtProcessorPrepare {
public:
	template<class BUFFER, class NetProvider>
	static unsigned int
	process(Connector<BUFFER, NetProvider> &client,
		Connection<Buf_t, NetProvider> &conn,
		std::string &stmt)
	{
		rid_t future = conn.prepare(stmt);

		client.wait(conn, future, WAIT_TIMEOUT);
		fail_unless(conn.futureIsReady(future));
		std::optional<Response<Buf_t>> response = conn.getResponse(future);
		fail_unless(response != std::nullopt);
		fail_if(response->body.error_stack != std::nullopt);
		fail_unless(response->body.stmt_id != std::nullopt);
		fail_unless(response->body.bind_count != std::nullopt);
		return response->body.stmt_id.value();
	}
};

/**
 * Compares sql data of two given Body objects.
 */
template<class BUFFER>
void
check_sql_data(const Body<BUFFER> &got, const Body<BUFFER> &expected)
{
	/* Metadata. */
	fail_unless(got.metadata.has_value() == expected.metadata.has_value());
	if (got.metadata.has_value()) {
		fail_unless(got.metadata->column_maps.size() == expected.metadata->column_maps.size());
		for (size_t i = 0; i < got.metadata->column_maps.size(); i++) {
			const ColumnMap &got_cm = got.metadata->column_maps[i];
			const ColumnMap &expected_cm = expected.metadata->column_maps[i];
			fail_unless(got_cm.field_name == expected_cm.field_name);
			fail_unless(got_cm.field_type == expected_cm.field_type);
			fail_unless(got_cm.collation == expected_cm.collation);
			fail_unless(got_cm.span == expected_cm.span);
			fail_unless(got_cm.is_nullable == expected_cm.is_nullable);
			fail_unless(got_cm.is_autoincrement == expected_cm.is_autoincrement);
		}
	}

	/* Statement id. */
	fail_unless(got.stmt_id == expected.stmt_id);

	/* Bind count. */
	fail_unless(got.bind_count == expected.bind_count);

	/* Sql info. */
	fail_unless(got.sql_info.has_value() == expected.sql_info.has_value());
	if (got.sql_info.has_value()) {
		fail_unless(got.sql_info->row_count == expected.sql_info->row_count);
		fail_unless(got.sql_info->autoincrement_ids ==
			    expected.sql_info->autoincrement_ids);
	}
}

/** Single connection, several executes. */
template <class BUFFER, class NetProvider, class StmtProcessor>
void
single_conn_sql(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);

	using Data_t = std::vector<std::tuple<int, std::string, double>>;
	Data_t data;
	using Body_t = Body<BUFFER>;

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);

	TEST_CASE("CREATE TABLE");
	std::string stmt_str = "CREATE TABLE IF NOT EXISTS TSQL (COLUMN1 UNSIGNED PRIMARY KEY, "
			       "COLUMN2 VARCHAR(50), COLUMN3 DOUBLE);";
	auto stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t create_table = conn.execute(stmt, std::make_tuple());

	client.wait(conn, create_table, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(create_table));
	std::optional<Response<Buf_t>> response = conn.getResponse(create_table);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	Body_t sql_data_create_table;
	sql_data_create_table.sql_info = SqlInfo{1, {}};
	check_sql_data(response->body, sql_data_create_table);

	TEST_CASE("Simple INSERT");
	stmt_str = "INSERT INTO TSQL VALUES (20, 'first', 3.2), (21, 'second', 5.4)";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t insert = conn.execute(stmt, std::make_tuple());

	client.wait(conn, insert, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(insert));
	response = conn.getResponse(insert);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	/* Check metadata. */
	Body_t sql_data_insert;
	sql_data_insert.sql_info = SqlInfo{2, {}};
	check_sql_data(response->body, sql_data_insert);

	TEST_CASE("INSERT with binding arguments");
	std::tuple args = std::make_tuple(1, "Timur",   12.8,
	                                  2, "Nikita",  -8.0,
					  3, "Anastas", 345.298);
	stmt_str = "INSERT INTO TSQL VALUES (?, ?, ?), (?, ?, ?), (?, ?, ?);";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t insert_args = conn.execute(stmt, args);

	client.wait(conn, insert_args, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(insert_args));
	response = conn.getResponse(insert_args);
	fail_unless(response != std::nullopt);

	printResponse<BUFFER>(*response, Data_t());
	Body_t sql_data_insert_bind;
	sql_data_insert_bind.sql_info = SqlInfo{3, {}};
	check_sql_data(response->body, sql_data_insert_bind);

	TEST_CASE("SELECT");
	stmt_str = "SELECT * FROM SEQSCAN TSQL;";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t select = conn.execute(stmt, std::make_tuple());

	client.wait(conn, select, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(select));
	response = conn.getResponse(select);
	fail_unless(response != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.data->decode(data));
	fail_unless(data.size() == 5);
	printResponse<BUFFER>(*response, Data_t());
	Body_t sql_data_select;
	std::vector<ColumnMap> sql_data_select_columns = {
		{"COLUMN1", "unsigned", "", std::nullopt, false, false},
		{"COLUMN2", "string", "", std::nullopt, false, false},
		{"COLUMN3", "double", "", std::nullopt, false, false},
	};
	sql_data_select.metadata = Metadata{sql_data_select_columns};
	check_sql_data(response->body, sql_data_select);

	TEST_CASE("DROP TABLE");
	stmt_str = "DROP TABLE IF EXISTS TSQL;";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t drop_table = conn.execute(stmt, std::make_tuple());

	client.wait(conn, drop_table, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(drop_table));
	response = conn.getResponse(drop_table);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.sql_info != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);

	TEST_CASE("ENABLE METADATA");
	stmt_str = "UPDATE \"_session_settings\" SET \"value\" = true WHERE \"name\" = 'sql_full_metadata';";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t enable_metadata = conn.execute(stmt, std::make_tuple());

	client.wait(conn, enable_metadata, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(enable_metadata));
	response = conn.getResponse(enable_metadata);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.sql_info != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);

	TEST_CASE("CREATE TABLE with autoincrement and collation");
	stmt_str = "CREATE TABLE IF NOT EXISTS TSQL "
		   "(COLUMN1 UNSIGNED PRIMARY KEY AUTOINCREMENT, "
		   "COLUMN2 STRING COLLATE \"unicode\", COLUMN3 DOUBLE);";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	create_table = conn.execute(stmt, std::make_tuple());
	client.wait(conn, create_table, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(create_table));
	response = conn.getResponse(create_table);
	fail_unless(response != std::nullopt);
	printResponse<BUFFER>(*response, Data_t());
	fail_unless(response->body.error_stack == std::nullopt);

	Body_t sql_data_create_table_autoinc;
	sql_data_create_table_autoinc.sql_info = SqlInfo{1, {}};
	check_sql_data(response->body, sql_data_create_table_autoinc);

	TEST_CASE("INSERT with autoincrement");
	std::tuple args2 = std::make_tuple(
		nullptr, "Timur", 12.8,
	        nullptr, "Nikita", -8.0,
		/* Null for the 1st field is in statement. */
		"Anastas", 345.298);
	stmt_str = "INSERT INTO TSQL VALUES (?, ?, ?), (?, ?, ?), (NULL, ?, ?);";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	insert = conn.execute(stmt, args2);
	client.wait(conn, insert, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(insert));
	response = conn.getResponse(insert);
	fail_unless(response != std::nullopt);
	printResponse<BUFFER>(*response, Data_t());
	fail_unless(response->body.error_stack == std::nullopt);

	Body_t sql_data_insert_autoinc;
	sql_data_insert_autoinc.sql_info = SqlInfo{3, {1, 2, 3}};
	check_sql_data(response->body, sql_data_insert_autoinc);

	TEST_CASE("SELECT from space with autoinc and collation");
	stmt_str = "SELECT * FROM SEQSCAN TSQL;";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	select = conn.execute(stmt, std::make_tuple());

	client.wait(conn, select, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(select));
	response = conn.getResponse(select);
	fail_unless(response != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.data->decode(data));
	fail_unless(data.size() == 3);
	printResponse<BUFFER>(*response, Data_t());
	Body_t sql_data_select_autoinc;
	sql_data_select_columns = {
		{"COLUMN1", "unsigned", "", std::nullopt, false, true},
		{"COLUMN2", "string", "unicode", std::nullopt, true, false},
		{"COLUMN3", "double", "", std::nullopt, true, false},
	};
	sql_data_select_autoinc.metadata = Metadata{sql_data_select_columns};
	check_sql_data(response->body, sql_data_select_autoinc);

	TEST_CASE("SELECT with span");
	stmt_str = "SELECT 1 AS X;";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	select = conn.execute(stmt, std::make_tuple());

	client.wait(conn, select, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(select));
	response = conn.getResponse(select);
	fail_unless(response != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.data->decode(data));
	fail_unless(data.size() == 1);
	printResponse<BUFFER>(*response, Data_t());
	Body_t sql_data_select_span;
	sql_data_select_columns = {{"X", "integer", "", "1"}};
	sql_data_select_span.metadata = Metadata{sql_data_select_columns};
	check_sql_data(response->body, sql_data_select_span);

	/* Finally, drop the table. */
	stmt_str = "DROP TABLE IF EXISTS TSQL;";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	drop_table = conn.execute(stmt, std::make_tuple());
	client.wait(conn, drop_table, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(drop_table));
	response = conn.getResponse(drop_table);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.sql_info != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);

	TEST_CASE("DISABLE METADATA");
	stmt_str = "UPDATE \"_session_settings\" SET \"value\" = false WHERE \"name\" = 'sql_full_metadata';";
	stmt = StmtProcessor::process(client, conn, stmt_str);
	rid_t disable_metadata = conn.execute(stmt, std::make_tuple());

	client.wait(conn, disable_metadata, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(disable_metadata));
	response = conn.getResponse(disable_metadata);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.sql_info != std::nullopt);
	fail_if(response->body.error_stack != std::nullopt);

	client.close(conn);
}



/** Single connection, call procedure with arguments */
template <class BUFFER, class NetProvider>
void
replace_unix_socket(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, unixsocket, 0);
	fail_unless(rc == 0);

	TEST_CASE("select from unix socket");

	auto s = conn.space[512];

	rid_t f = s.replace(std::make_tuple(666, "111", 1.01));
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));
	std::optional<Response<Buf_t>> response = conn.getResponse(f);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(*response);

	client.close(conn);
}

/** Single connection, call procedure with arguments */
template <class BUFFER, class NetProvider>
void
test_auth(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	const char *user = "megauser";
	const char *passwd  = "megapassword";

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port, user, passwd);
	fail_unless(rc == 0);

	uint32_t space_id = 513;

	auto s = conn.space[space_id];
	rid_t f = s.select(std::make_tuple(0));
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));

	std::optional<Response<Buf_t>> response = conn.getResponse(f);
	fail_unless(response != std::nullopt);
	fail_unless(response->body.data != std::nullopt);
	fail_unless(response->body.error_stack == std::nullopt);
	printResponse<BUFFER>(*response);
}

/** Single connection, write to closed connection. */
void
test_sigpipe(void)
{
	TEST_INIT(0);

	int rc = ::launchDummyServer(localhost, dummy_server_port);
	fail_unless(rc == 0);

	/* FIXME(gh-122): use own client not to leave hanging dead connection. */
	Connector<Buf_t, NetProvider> client;
	Connection<Buf_t, NetProvider> conn(client);
	rc = ::test_connect(client, conn, localhost, dummy_server_port);
	fail_unless(rc == 0);

	/*
	 * Create a large payload so that request needs at least 2 `send`s, the
	 * latter being written to a closed socket.
	 */
	rid_t f = conn.space[0].replace(std::vector<uint64_t>(500000, 777));
	fail_if(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	int saved_errno = conn.getError().saved_errno;
#ifdef __APPLE__
	fail_unless(saved_errno == EPIPE || saved_errno == ECONNRESET ||
		    saved_errno == EPROTOTYPE);
#else
	fail_unless(saved_errno == EPIPE);
#endif
	fail_if(conn.futureIsReady(f));
}

/** Single connection, wait response from closed connection. */
void
test_dead_connection_wait(void)
{
	TEST_INIT(0);

	int rc = ::launchDummyServer(localhost, dummy_server_port);
	fail_unless(rc == 0);

	/* FIXME(gh-122): use own client not to leave hanging dead connection. */
	Connector<Buf_t, NetProvider> client;
	Connection<Buf_t, NetProvider> conn(client);
	rc = ::test_connect(client, conn, localhost, dummy_server_port);
	fail_unless(rc == 0);

	rid_t f = conn.ping();
	fail_if(client.wait(conn, f) == 0);
	fail_if(conn.futureIsReady(f));

	fail_if(client.waitAll(conn, std::vector<rid_t>(f)) == 0);
	fail_if(conn.futureIsReady(f));

	fail_if(client.waitCount(conn, 1) == 0);
	fail_if(conn.futureIsReady(f));

	/* FIXME(gh-51) */
#if 0
	fail_if(client.waitAny() != std::nullopt);
	fail_if(conn.futureIsReady(f));
#endif
}

/**
 * Test for miscellaneous issues related to response decoding.
 */
template <class BUFFER, class NetProvider>
void
response_decoding(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);

	TEST_CASE("decode data with non-matching format");
	rid_t f = conn.call("remote_uint", std::make_tuple());
	client.wait(conn, f, WAIT_TIMEOUT);
	fail_unless(conn.futureIsReady(f));
	std::optional<Response<Buf_t>> response = conn.getResponse(f);

	fail_unless(response.has_value());
	fail_unless(response->body.data.has_value());

	std::string str;
	unsigned num;
	std::tuple<std::string> arr_of_str;
	std::tuple<unsigned> arr_of_num;
	/* Try to decode data with non-matching format. */
	fail_if(response->body.data->decode(str));
	fail_if(response->body.data->decode(num));
	fail_if(response->body.data->decode(arr_of_str));
	/* We should successfully decode data after all. */
	fail_unless(response->body.data->decode(arr_of_num));
	fail_unless(std::get<0>(arr_of_num) == 666);

	TEST_CASE("decode data to rvalue object");
	num = 0;
	fail_unless(response->body.data->decode(std::forward_as_tuple(num)));
	fail_unless(num == 666);

	TEST_CASE("decode data to object with an mpp tag");
	std::get<0>(arr_of_num) = 0;
	fail_unless(response->body.data->decode(mpp::as_arr(arr_of_num)));
	fail_unless(std::get<0>(arr_of_num) == 666);

	client.close(conn);
}

/** Checks all available `wait` methods of connector. */
template <class BUFFER, class NetProvider>
void
test_wait(Connector<BUFFER, NetProvider> &client)
{
	TEST_INIT(0);
	static constexpr double SLEEP_TIME = 0.1;

	Connection<Buf_t, NetProvider> conn(client);
	int rc = test_connect(client, conn, localhost, port);
	fail_unless(rc == 0);

	TEST_CASE("wait(0) and wait(-1)");
	rid_t f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!conn.futureIsReady(f));
	client.wait(conn, f, 0);
	fail_unless(!conn.futureIsReady(f));
	client.wait(conn, f, -1);
	fail_unless(conn.futureIsReady(f));
	std::optional<Response<Buf_t>> response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("wait(0) polls connections");
	f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!conn.futureIsReady(f));
	while (!conn.futureIsReady(f)) {
		client.wait(conn, f, 0);
		usleep(10 * 1000); /* 10ms */
	}
	fail_unless(conn.futureIsReady(f));
	response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("waitAny(0) and waitAny(-1)");
	f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!client.waitAny(0).has_value());
	fail_unless(client.waitAny(-1).has_value());
	response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("waitAny(0) polls connections");
	f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!conn.futureIsReady(f));
	while (!conn.futureIsReady(f)) {
		client.waitAny(0);
		usleep(10 * 1000); /* 10ms */
	}
	fail_unless(conn.futureIsReady(f));
	response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("waitAll(0) and waitAll(-1)");
	std::vector<rid_t> fs;
	fs.push_back(conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME)));
	fs.push_back(conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME)));
	fail_unless(client.waitAll(conn, fs, 0) == -1);
	fail_unless(client.waitAll(conn, fs, -1) == 0);
	response = conn.getResponse(fs[0]);
	fail_unless(response.has_value());
	response = conn.getResponse(fs[1]);
	fail_unless(response.has_value());

	TEST_CASE("waitAll(0) polls connections");
	f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!conn.futureIsReady(f));
	while (!conn.futureIsReady(f)) {
		client.waitAll(conn, std::vector<rid_t>{f}, 0);
		usleep(10 * 1000); /* 10ms */
	}
	fail_unless(conn.futureIsReady(f));
	response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("waitCount(0) and waitCount(-1)");
	fs.clear();
	fs.push_back(conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME)));
	fs.push_back(conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME)));
	fail_unless(client.waitCount(conn, 2, 0) == -1);
	fail_unless(client.waitCount(conn, 2, -1) == 0);
	response = conn.getResponse(fs[0]);
	fail_unless(response.has_value());
	response = conn.getResponse(fs[1]);
	fail_unless(response.has_value());

	TEST_CASE("waitCount(0) polls connections");
	f = conn.call("remote_sleep", std::forward_as_tuple(SLEEP_TIME));
	fail_unless(!conn.futureIsReady(f));
	while (!conn.futureIsReady(f)) {
		client.waitCount(conn, 1, 0);
		usleep(10 * 1000); /* 10ms */
	}
	fail_unless(conn.futureIsReady(f));
	response = conn.getResponse(f);
	fail_unless(response.has_value());

	TEST_CASE("waitAny after several waits (gh-124)");
	Connection<Buf_t, NetProvider> conn1(client);
	Connection<Buf_t, NetProvider> conn2(client);
	Connection<Buf_t, NetProvider> conn3(client);
	rc = test_connect(client, conn1, localhost, port);
	fail_unless(rc == 0);
	rc = test_connect(client, conn2, localhost, port);
	fail_unless(rc == 0);
	rc = test_connect(client, conn3, localhost, port);
	fail_unless(rc == 0);
	rid_t f1 = conn1.ping();
	rid_t f2 = conn2.ping();
	rid_t f3 = conn3.ping();

	/* Wait for all connections. */
	fail_unless(client.wait(conn1, f1, WAIT_TIMEOUT) == 0);
	fail_unless(conn1.futureIsReady(f1));
	fail_unless(conn1.getResponse(f1).header.code == 0);

	fail_unless(client.wait(conn2, f2, WAIT_TIMEOUT) == 0);
	fail_unless(conn2.futureIsReady(f2));
	fail_unless(conn2.getResponse(f2).header.code == 0);

	fail_unless(client.wait(conn3, f3, WAIT_TIMEOUT) == 0);
	fail_unless(conn3.futureIsReady(f3));
	fail_unless(conn3.getResponse(f3).header.code == 0);

	/*
	 * Wait any - we shouldn't get any of the connections here since we've
	 * received all the responses.
	 * Note that the connector used to crash here (gh-124) because some of the
	 * connnections still could appear in `m_ReadyToDecode` set.
	 */
	std::optional<Connection<Buf_t, NetProvider>> conn_opt = client.waitAny(WAIT_TIMEOUT);
	fail_if(conn_opt.has_value());

	/* Close all connections used only by the case. */
	client.close(conn1);
	client.close(conn2);
	client.close(conn3);

	TEST_CASE("wait with argument result");
	f = conn.ping();
	fail_unless(!conn.futureIsReady(f));
	Response<BUFFER> result;
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT, &result) == 0);
	/* The result was consumed, so the future is not ready. */
	fail_unless(!conn.futureIsReady(f));
	/* The future is actually request sync - check if the result is valid. */
	fail_unless(result.header.sync == static_cast<int>(f));
	fail_unless(result.header.code == 0);

	TEST_CASE("wait with argument result for decoded future");
	f = conn.ping();
	fail_unless(!conn.futureIsReady(f));
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	fail_unless(conn.futureIsReady(f));
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT, &result) == 0);
	/* The result was consumed, so the future is not ready. */
	fail_unless(!conn.futureIsReady(f));
	/* The future is actually request sync - check if the result is valid. */
	fail_unless(result.header.sync == static_cast<int>(f));
	fail_unless(result.header.code == 0);

	TEST_CASE("wait with argument result - several requests");
	/* Obtain in direct order. */
	f1 = conn.ping();
	f2 = conn.ping();
	fail_unless(client.wait(conn, f1, WAIT_TIMEOUT, &result) == 0);
	fail_unless(result.header.sync == static_cast<int>(f1));
	fail_unless(result.header.code == 0);
	fail_unless(client.wait(conn, f2, WAIT_TIMEOUT, &result) == 0);
	fail_unless(result.header.sync == static_cast<int>(f2));
	fail_unless(result.header.code == 0);

	/* Obtain in reversed order. */
	f1 = conn.ping();
	f2 = conn.ping();
	fail_unless(client.wait(conn, f2, WAIT_TIMEOUT, &result) == 0);
	fail_unless(result.header.sync == static_cast<int>(f2));
	fail_unless(result.header.code == 0);
	fail_unless(client.wait(conn, f1, WAIT_TIMEOUT, &result) == 0);
	fail_unless(result.header.sync == static_cast<int>(f1));
	fail_unless(result.header.code == 0);

	TEST_CASE("wait method check future readiness before waiting (gh-133");
	f = conn.ping();
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	fail_unless(client.wait(conn, f) == 0);
	conn.getResponse(f);
	f = conn.ping();
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	fail_unless(client.waitAll(conn, {f}) == 0);
	conn.getResponse(f);
	f = conn.ping();
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	/* FIXME(gh-143): test solely that we check future readiness before waiting. */
	fail_unless(client.waitCount(conn, 0) == 0);
	conn.getResponse(f);
	/* FIXME(gh-132): waitAny does not check connections for ready futures. */
#if 0
	f = conn.ping();
	fail_unless(client.wait(conn, f, WAIT_TIMEOUT) == 0);
	fail_unless(client.waitAny(conn).has_value());
	conn.getResponse(f);
#endif

	client.close(conn);
}

int main()
{
#ifdef TNTCXX_ENABLE_SSL
#ifndef __FreeBSD__
	// There's no way to disable SIGPIPE for SSL on non-FreeBSD platforms,
	// so it is needed to disable signal handling.
	signal(SIGPIPE, SIG_IGN);
#endif
#endif

	if (cleanDir() != 0)
		return -1;

#ifdef TNTCXX_ENABLE_SSL
	if (genSSLCert() != 0)
		return -1;
#endif

	if (launchTarantool(enable_ssl) != 0)
		return -1;

	sleep(1);

	Connector<Buf_t, NetProvider> client;
	trivial<Buf_t, NetProvider>(client);
	single_conn_ping<Buf_t, NetProvider>(client);
	auto_close<Buf_t, NetProvider>(client);
	many_conn_ping<Buf_t, NetProvider>();
	single_conn_error<Buf_t, NetProvider>(client);
	single_conn_replace<Buf_t, NetProvider>(client);
	single_conn_insert<Buf_t, NetProvider>(client);
	single_conn_update<Buf_t, NetProvider>(client);
	single_conn_delete<Buf_t, NetProvider>(client);
	single_conn_upsert<Buf_t, NetProvider>(client);
	single_conn_select<Buf_t, NetProvider>(client);
	single_conn_call<Buf_t, NetProvider>(client);
	single_conn_sql<Buf_t, NetProvider, StmtProcessorNoop>(client);
	single_conn_sql<Buf_t, NetProvider, StmtProcessorPrepare>(client);
	replace_unix_socket(client);
	test_auth(client);
	/*
	 * Testing this for SSL is hard, since the connection starts to involve
	 * an a lot more complex state machine.
	 */
#ifndef TNTCXX_ENABLE_SSL
	::test_sigpipe();
#endif
	::test_dead_connection_wait();
	response_decoding(client);
	test_wait(client);
	return 0;
}
