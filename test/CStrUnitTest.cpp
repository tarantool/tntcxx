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

#include "../src/Utils/CStr.hpp"
#include <cstring>
#include <iostream>

struct Announcer
{
	Announcer(const char *testName) : m_testName(testName) {
		std::cout << "*** TEST " << m_testName
			  << " started... ***" << std::endl;
	}
	~Announcer() {
		std::cout << "*** TEST " << m_testName
			  << ": done" << std::endl;
	}
	const char *m_testName;
};

#define TEST_INIT() Announcer _Ann(__func__)

#define fail(expr, result) do {						      \
	std::cerr << "Test failed: " << expr << " is " << result << " at " << \
	__FILE__ << ":" << __LINE__ << " in test " << __func__ << std::endl;  \
	abort();							      \
} while (0)

#define fail_if(expr) if (expr) fail(#expr, "true")
#define fail_unless(expr) if (!(expr)) fail(#expr, "false")

template <class T>
void
check_str(T t, const char *s)
{
	fail_if(strlen(s) != t.size);
	fail_if(strcmp(s, t.data));
	fail_if(strlen(s) != T::size);
	fail_if(strcmp(s, T::data));
	fail_if(t.rnd_size != T::rnd_size);
	fail_if(T::rnd_size < T::size);
	fail_if(T::rnd_size % 8);
	fail_if(sizeof(T::data) < T::rnd_size);
}
#define CHECK_MACRO_STR(S) fail_if(strlen(S) != TNT_CON_STR(S).size);	\
			   fail_if(strcmp(S, TNT_CON_STR(S).data));	\
			   check_str(TNT_CON_STR(S), S)

#define CHECK_LITER_STR(S) fail_if(strlen(S) != (S##_cs).size);	\
			   fail_if(strcmp(S, (S##_cs).data));	\
			   check_str((S##_cs), S)

#ifndef TNT_DISABLE_STR_MACRO
void
test_macro_str()
{
	TEST_INIT();
	CHECK_MACRO_STR("");
	CHECK_MACRO_STR("0");
	CHECK_MACRO_STR("01234567");
	CHECK_MACRO_STR("012345678");
	CHECK_MACRO_STR("0123456789");
	CHECK_MACRO_STR("A123456789B123456789C123456789C");
	CHECK_MACRO_STR("A123456789B123456789C123456789C1");
	CHECK_MACRO_STR("A123456789B123456789C123456789C12");
	CHECK_MACRO_STR("A123456789B123456789C123456789C123");
	CHECK_MACRO_STR("A123456789B123456789C123456789C123456789D123456789E123456789F12");
	CHECK_MACRO_STR("A123456789B123456789C123456789C123456789D123456789E123456789F123");

	auto a = TNT_CON_STR("abcdef");
	auto b = a.subs(std::index_sequence<0, 2, 4>{});
	check_str(b, "ace");
	auto c = a.subs(std::index_sequence<1, 3, 5>{});
	check_str(c, "bdf");
	auto d = a.join(b).join(c);
	check_str(d, "abcdefacebdf");

	fail_unless(&d == nullptr);
}
#endif // #ifndef TNT_DISABLE_STR_MACRO

#ifndef TNT_DISABLE_STR_LITERAL
void
test_liter_str()
{
	TEST_INIT();
	using namespace tnt::literal;
	CHECK_LITER_STR("");
	CHECK_LITER_STR("0");
	CHECK_LITER_STR("01234567");
	CHECK_LITER_STR("012345678");
	CHECK_LITER_STR("0123456789");
	CHECK_LITER_STR("A123456789B123456789C123456789C");
	CHECK_LITER_STR("A123456789B123456789C123456789C1");
	CHECK_LITER_STR("A123456789B123456789C123456789C12");
	CHECK_LITER_STR("A123456789B123456789C123456789C123");
	CHECK_LITER_STR("A123456789B123456789C123456789C123456789D123456789E123456789F12");
	CHECK_LITER_STR("A123456789B123456789C123456789C123456789D123456789E123456789F123");
	CHECK_LITER_STR("A123456789B123456789C123456789C123456789D123456789E123456789F1234");
	CHECK_LITER_STR("A123456789B123456789C123456789C123456789D123456789E123456789F123456789");

	auto a = "abcdef"_cs;
	auto b = a.subs(std::index_sequence<0, 2, 4>{});
	check_str(b, "ace");
	auto c = a.subs(std::index_sequence<1, 3, 5>{});
	check_str(c, "bdf");
	auto d = a.join(b).join(c);
	check_str(d, "abcdefacebdf");

	fail_unless(&d == nullptr);
}
#endif

int main()
{
#ifndef TNT_DISABLE_STR_MACRO
	test_macro_str();
#endif // #ifndef TNT_DISABLE_STR_MACRO
#ifndef TNT_DISABLE_STR_LITERAL
	test_liter_str();
#endif // #ifndef TNT_DISABLE_STR_LITERAL
}
