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
#include "Utils/PerfTimer.hpp"

#include "../src/Client/Connector.hpp"
#include "../src/Client/LibevNetProvider.hpp"

static const char *localhost = "127.0.0.1";
static constexpr size_t port = 3301;
static constexpr size_t space_id = 512;
static size_t suite_numb = 0;

static constexpr int WAIT_TIMEOUT = 10000; //milliseconds

static constexpr size_t SMALL_BUFFER_SIZE   = 128;
static constexpr size_t AVERAGE_BUFFER_SIZE = 4 * 1024;
static constexpr size_t BIG_BUFFER_SIZE     = 16 * 1024;
static constexpr size_t GIANT_BUFFER_SIZE   = 128 * 1024;

/** In total 1 million requests per test. */
constexpr size_t NUM_REQ = 2000;
constexpr size_t NUM_TEST = 500;
constexpr size_t TOTAL_REQ = NUM_REQ * NUM_TEST;
constexpr size_t NUM_CONN = 1;

struct RequestResult {
	double rps;
	size_t server_rps;
};

struct BenchResults {
	RequestResult ping;
	RequestResult replace;
	RequestResult select;
	RequestResult call;
};

void printResults(BenchResults &r)
{
	std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
	std::cout << "+                 RESULTS                          " << std::endl;
	std::cout << "+          " << TOTAL_REQ << " REQUESTS ARE DONE   " << std::endl;
	std::cout << "+  PING " << std::endl;
	std::cout << "+          MRPS        " << r.ping.rps / TOTAL_REQ << std::endl;
	std::cout << "+          SERVER RPS  " << r.ping.server_rps    << std::endl;
	std::cout << "+  REPLACE " << std::endl;
	std::cout << "+          MRPS        " << r.replace.rps / TOTAL_REQ << std::endl;
	std::cout << "+          SERVER RPS  " << r.replace.server_rps    << std::endl;
	std::cout << "+  SELECT " << std::endl;
	std::cout << "+          MRPS        " << r.select.rps / TOTAL_REQ << std::endl;
	std::cout << "+          SERVER RPS  " << r.select.server_rps    << std::endl;
	std::cout << "+  CALL " << std::endl;
	std::cout << "+          MRPS        " << r.call.rps / TOTAL_REQ << std::endl;
	std::cout << "+          SERVER RPS  " << r.call.server_rps    << std::endl;
	std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
}

template<class BUFFER, class NetProvider>
rid_t
executeRequest(Connection<BUFFER, NetProvider> &conn, int request_type, int key)
{
	switch (request_type) {
		case Iproto::REPLACE:
			return conn.space[space_id].replace(std::make_tuple(key, "str", 1.01));
		case Iproto::PING:
			return conn.ping();
		case Iproto::SELECT:
			return conn.space[space_id].select(std::make_tuple(key));
		case Iproto::CALL:
			return conn.call("bench_func", std::make_tuple(1, 2, 3, 4, 5));
		default:
			abort();
	}
}

template<class BUFFER, class NetProvider>
RequestResult
testBatchRequestsSingleConn(int request_type)
{
	Connector<BUFFER, NetProvider> client;
	Connection<BUFFER, NetProvider> conn(client);
	int rc = client.connect(conn, localhost, port);
	if (rc != 0) {
		std::cerr << "Failed to connect to localhost:" << port << std::endl;
		abort();
	}
	PerfTimer timer;
	timer.start();
	for (size_t k = 0; k < NUM_TEST; k++) {
		rid_t ids[NUM_REQ];
		for (size_t i = 0; i < NUM_REQ; i++)
			ids[i] = executeRequest(conn, request_type, i);
		client.wait(conn, ids[NUM_REQ-1], WAIT_TIMEOUT);
		for (size_t i = 0; i < NUM_REQ; i++) {
			if (!conn.futureIsReady(ids[i])) {
				std::cerr << "Test failed: response is not ready!" << std::endl;
				abort();
			}
			auto resp = conn.getResponse(ids[i]);
			if (resp.header.code != 0) {
				std::cerr << "Test failed: server responded with an error!" << std::endl;
				abort();
			}
		}
	}
	timer.stop();
	RequestResult r;
	r.rps = NUM_REQ * NUM_TEST / timer.result();
	r.server_rps = getServerRps(client, conn);
	client.close(conn);
	return r;
}

template<class BUFFER, class NetProvider>
RequestResult
testBatchRequestsSeveralConns(int request_type)
{
	Connector<BUFFER, NetProvider> client;
	std::vector<Connection<BUFFER, NetProvider>> conns;
	for (size_t i = 0; i < NUM_CONN; ++i) {
		Connection<BUFFER, NetProvider> conn(client);
		int rc = client.connect(conn, localhost, port);
		if (rc != 0) {
			std::cerr << "Failed to connect to localhost:" << port << std::endl;
			abort();
		}
		conns.push_back(conn);
	}
	PerfTimer timer;
	timer.start();
	for (size_t k = 0; k < NUM_TEST; k++) {
		rid_t ids[NUM_REQ];
		for (size_t j = 0; j < NUM_CONN; j++) {
			for (size_t i = 0; i < NUM_REQ / NUM_CONN; i++) {
				rid_t future = j * (NUM_REQ / NUM_CONN) + i;
				ids[future] = executeRequest(conns[j], request_type, i);
			}
		}
		for (size_t j = 0; j < NUM_CONN; j++) {
			rid_t last_feature = (j+1)*(NUM_REQ / NUM_CONN) - 1;
			if (client.wait(conns[j], ids[last_feature], WAIT_TIMEOUT) != 0) {
				std::cerr << "Wait failed: " << conns[j].getError().msg << std::endl;
				abort();
			}
		}
		for (size_t j = 0; j < NUM_CONN; j++) {
			for (size_t i = 0; i < NUM_REQ / NUM_CONN; i++) {
				rid_t future = j*(NUM_REQ / NUM_CONN) + i;
				if (!conns[j].futureIsReady(ids[future])) {
					std::cerr << "Test failed: response is not ready!" << std::endl;
					abort();
				}
				auto resp = conns[j].getResponse(ids[future]);
				if (resp.header.code != 0) {
					abort();
				}
			}
		}
	}
	timer.stop();
	RequestResult r;
	r.rps = NUM_REQ * NUM_TEST / timer.result();
	r.server_rps = getServerRps(client, conns[0]);
	return r;
}


template<class BUFFER, class NetProvider>
void
testRequestTypes()
{
	BenchResults r;
	r.ping = testBatchRequestsSingleConn<BUFFER, NetProvider>(Iproto::PING);
	r.replace = testBatchRequestsSingleConn<BUFFER, NetProvider>(Iproto::REPLACE);
	r.select = testBatchRequestsSingleConn<BUFFER, NetProvider>(Iproto::SELECT);
	r.call = testBatchRequestsSingleConn<BUFFER, NetProvider>(Iproto::CALL);
	printResults(r);
}

template<class BUFFER>
void
testEngines()
{
	std::cout << "///////////////////////////////////////////////////" << std::endl;
	std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
	std::cout << "              TEST SUITE #" << suite_numb++ << std::endl;
	std::cout << "              BUFFER SIZE=" << BUFFER::blockSize() << std::endl;
#ifdef __linux__
	using DefaultNet_t = EpollNetProvider<BUFFER, DefaultStream >;
	std::cout << "===================================================" << std::endl;
	std::cout << "        STARTING TEST EPOLL" << std::endl;
	std::cout << "===================================================" << std::endl;
	testRequestTypes<BUFFER, DefaultNet_t >();
#endif
	using LibEvNet_t = LibevNetProvider<BUFFER, DefaultStream >;
	std::cout << "===================================================" << std::endl;
	std::cout << "        STARTING TEST LibEV" << std::endl;
	std::cout << "===================================================" << std::endl;
	testRequestTypes<BUFFER, LibEvNet_t >();
}

template<class BUFFER, class NetProvider>
size_t
getServerRps(Connector<BUFFER, NetProvider> &client,
	     Connection<BUFFER, NetProvider> &conn)
{
	rid_t f = conn.call("get_rps", std::make_tuple());
	client.wait(conn, f, WAIT_TIMEOUT);
	if (! conn.futureIsReady(f)) {
		std::cerr << "Failed to retrieve rps from server!" << std::endl;
		abort();
	}
	Response<BUFFER> response = conn.getResponse(f);
	if (response.body.data == std::nullopt) {
		std::cerr << "Failed to retrieve rps from server: error is returned" << std::endl;
		abort();
	}
	Data<BUFFER>& data = *response.body.data;
	std::vector<UserTuple> tuples = decodeMultiReturn(conn.getInBuf(), data);
	return tuples[0].field1;
}

void
greetings()
{
	std::cout << "===================================================" << std::endl;
	std::cout << "              CLIENT PERFORMANCE TEST              " << std::endl;
	std::cout << "===================================================" << std::endl;
	std::cout << "              GLOBAL CONFIGS                       " << std::endl;
	std::cout << "          TIMEOUT " << WAIT_TIMEOUT << " MILLISECONDS   " << std::endl;
	std::cout << "          SERVER ADDRESS " << localhost << ":" << port << std::endl;
	std::cout << "          NUMBER OF CONNECTIONS " << NUM_CONN << std::endl;
}

template<std::size_t... I>
void
testBuffer(std::index_sequence<I...>)
{
	(testEngines< tnt::Buffer<I>>(),...);
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

	greetings();
	if (cleanDir() != 0) {
		std::cerr << "Failed to clean-up current directory" << std::endl;
		return -1;
	}
	if (launchTarantool() != 0) {
		std::cerr << "Failed to launch server" << std::endl;
		return -1;
	}
	sleep(1);

	testBuffer(std::index_sequence<SMALL_BUFFER_SIZE, AVERAGE_BUFFER_SIZE,
				       BIG_BUFFER_SIZE, GIANT_BUFFER_SIZE>{});

	return 0;
}
