#pragma once

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
