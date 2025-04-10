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

#include "../src/Client/Connector.hpp"
#include "../src/Client/LibevNetProvider.hpp"

#include <cmath>
#include <thread>
#include <tuple>

const char *localhost = "127.0.0.1";
int port = 3301;
int dummy_server_port = 3302;
const char *unixsocket = "./tnt.sock";
int WAIT_TIMEOUT = 1000; // milliseconds

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
test_connect(Connector &client, Connection &conn, const std::string &addr, unsigned port, const std::string user = {},
	     const std::string passwd = {})
{
	std::string service = port == 0 ? std::string {} : std::to_string(port);
	return client.connect(conn,
			      {
				      .address = addr,
				      .service = service,
				      .transport = transport,
				      .user = user,
				      .passwd = passwd,
			      });
}

class PingRequestProcessor {
public:
	static rid_t sendRequest(Connection<Buf_t, NetProvider> &conn, size_t thread_id, size_t iter)
	{
		(void)thread_id;
		(void)iter;
		rid_t f = conn.ping();
		fail_unless(!conn.futureIsReady(f));
		return f;
	}

	static void processResponse(std::optional<Response<Buf_t>> &response, size_t thread_id, size_t iter)
	{
		(void)thread_id;
		(void)iter;
		fail_unless(response != std::nullopt);
		fail_unless(response->header.code == 0);
	}
};

class ReplaceRequestProcessor {
public:
	static rid_t sendRequest(Connection<Buf_t, NetProvider> &conn, size_t thread_id, size_t iter)
	{
		const size_t space_id = 512;
		std::tuple data = std::make_tuple(iter, "a", double(iter * thread_id));
		rid_t f = conn.space[space_id].replace(data);
		fail_unless(!conn.futureIsReady(f));
		return f;
	}

	static void processResponse(std::optional<Response<Buf_t>> &response, size_t thread_id, size_t iter)
	{
		fail_unless(response != std::nullopt);
		fail_unless(response->header.code == 0);

		fail_unless(response != std::nullopt);
		fail_unless(response->body.data != std::nullopt);
		fail_unless(response->body.error_stack == std::nullopt);

		std::vector<std::tuple<size_t, std::string, double>> response_data;
		fail_unless(response->body.data->decode(response_data));
		fail_unless(response_data.size() == 1);
		fail_unless(std::get<0>(response_data[0]) == iter);
		fail_unless(std::get<1>(response_data[0]) == std::string("a"));
		fail_unless(std::fabs(std::get<2>(response_data[0]) - iter * thread_id)
			    <= std::numeric_limits<double>::epsilon());
	}
};

template <typename RequestProcessor, size_t ConnPerThread = 1>
static void
multithread_test(void)
{
	TEST_INIT(0);
	static constexpr int ITER_NUM = 1000;
	static constexpr int THREAD_NUM = 24;
	std::vector<std::thread> threads;
	threads.reserve(THREAD_NUM);
	for (int t = 0; t < THREAD_NUM; t++) {
		threads.emplace_back([]() {
			Connector<Buf_t, NetProvider> client;
			std::vector<Connection<Buf_t, NetProvider>> conns;
			for (size_t i = 0; i < ConnPerThread; i++)
				conns.emplace_back(client);
			for (auto &conn : conns) {
				int rc = test_connect(client, conn, localhost, port);
				fail_unless(rc == 0);
			}

			for (int iter = 0; iter < ITER_NUM; iter++) {
				std::array<rid_t, ConnPerThread> fs;

				for (size_t t = 0; t < ConnPerThread; t++)
					fs[t] = RequestProcessor::sendRequest(conns[t], t, iter);

				for (size_t t = 0; t < ConnPerThread; t++) {
					client.wait(conns[t], fs[t], WAIT_TIMEOUT);
					fail_unless(conns[t].futureIsReady(fs[t]));
					std::optional<Response<Buf_t>> response = conns[t].getResponse(fs[t]);
					RequestProcessor::processResponse(response, t, iter);
				}
			}
		});
	}
	for (auto &thread : threads)
		thread.join();
}

int
main()
{
	/*
	 * Send STDOUT to /dev/null - otherwise, there will be a ton of logs
	 * and it will be impossible to inspect them on failure.
	 */
	if (freopen("/dev/null", "w", stdout) == NULL) {
		std::cerr << "Cannot send STDOUT to /dev/null" << std::endl;
		abort();
	}
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

	multithread_test<PingRequestProcessor>();
	multithread_test<PingRequestProcessor, 5>();
	multithread_test<ReplaceRequestProcessor>();
	multithread_test<ReplaceRequestProcessor, 5>();
	return 0;
}
