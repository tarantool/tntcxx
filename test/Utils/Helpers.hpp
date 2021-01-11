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

#include <cassert>
#include <cstdarg>
#include <iostream>

struct Announcer
{
	Announcer(const char *testName, ...) : m_testName(testName) {
		va_list args;
		va_start (args, testName);
		int arg_count = va_arg(args, int);
		if (arg_count > 0)
			m_testName.append("<");
		for (int i = 0; i < arg_count; ++i) {
			int arg = va_arg(args, int);
			m_testName.append(std::to_string(arg));
			if (i < arg_count - 1)
				m_testName.append(", ");
		}
		if (arg_count > 0)
			m_testName.append(">");
		va_end(args);
		std::cout << "*** TEST " << m_testName;
		std::cout << " started... ***" << std::endl;
	}
	~Announcer() {
		std::cout << "*** TEST " << m_testName <<
		": done" << std::endl;
	}
	std::string m_testName;

};

#define TEST_INIT(...) Announcer _Ann(__func__, ##__VA_ARGS__)

#define fail(expr, result) do {							\
	std::cerr << "Test failed: " << expr << " is " << result << " at " <<	\
	__FILE__ << ":" << __LINE__ << " in test " << __func__ << std::endl;	\
        assert(false);								\
	exit(-1);								\
} while (0)

#define fail_if(expr) if (expr) fail(#expr, "true")
#define fail_unless(expr) if (!(expr)) fail(#expr, "false")
