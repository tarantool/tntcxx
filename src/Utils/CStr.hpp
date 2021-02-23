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

#include <cstddef> // size_t
#include <type_traits> // is_same_v
#include <utility> // index_sequence

namespace tnt {
/**
 * A type that holds constexpr string. Note that note an object but the type.
 * It's like std::intergral_constant, that works fine with arithmetic constants
 * but it not suitable for strings.
 */
template <char... C>
struct CStr {
	/** The size of the string. */
	static constexpr size_t size = sizeof...(C);
	/** Size that rounded up to be a multiple of 8. */
	static constexpr size_t rnd_size = size ? (size - 1) / 8 * 8 + 8 : 0;
	/** The string itself, null-terminated,
	 *  extended with zeros to have at least rnd_size bytes. */
	static constexpr char data[rnd_size + 1] = {C...};

	/** Concatenate the string with another. */
	template <char... D>
	static constexpr CStr<C..., D...> join(CStr<D...>) { return {}; }
	/** Get substring that consists of charactars with give indexes. */
	template <size_t... I>
	static constexpr CStr<data[I]...> subs(std::index_sequence<I...>) { return {}; }
	/** There's no sense in referencing the object. */
	constexpr std::nullptr_t operator&() const { return nullptr; }
};

/**
 * Macro for CStr definition.
 * If you don't like it, disable it by defining TNT_DISABLE_STR_MACRO.
 * Now does not accept string more that 64 symbols (static assert).
 *
 * Usage:
 * auto str = TNT_CON_STR("test");
 * std::cout << str.data << std::endl;
 */
#ifndef TNT_DISABLE_STR_MACRO
template <size_t S>
struct MppMacroIndexes {
	static_assert(S <= 64);
	using IndSeq_t = std::make_index_sequence<S>;
};
#define TNT_CON_STR_HLP0(S, N) S[(N) % sizeof(S)]
#define TNT_CON_STR_HLP1(S, N) TNT_CON_STR_HLP0(S, N), TNT_CON_STR_HLP0(S, N+ 1)
#define TNT_CON_STR_HLP2(S, N) TNT_CON_STR_HLP1(S, N), TNT_CON_STR_HLP1(S, N+ 2)
#define TNT_CON_STR_HLP3(S, N) TNT_CON_STR_HLP2(S, N), TNT_CON_STR_HLP2(S, N+ 4)
#define TNT_CON_STR_HLP4(S, N) TNT_CON_STR_HLP3(S, N), TNT_CON_STR_HLP3(S, N+ 8)
#define TNT_CON_STR_HLP5(S, N) TNT_CON_STR_HLP4(S, N), TNT_CON_STR_HLP4(S, N+16)
#define TNT_CON_STR_HLP6(S, N) TNT_CON_STR_HLP5(S, N), TNT_CON_STR_HLP5(S, N+32)
#define TNT_CON_STR_IS(S) tnt::MppMacroIndexes<std::size(S) - 1>::IndSeq_t{}
#define TNT_CON_STR(S) tnt::CStr<TNT_CON_STR_HLP6(S, 0)>::subs(TNT_CON_STR_IS(S))
#endif // #ifndef TNT_DISABLE_STR_MACRO

/**
 * C++ literal for CStr definition.
 * If it doesn't compile, disable it by defining TNT_DISABLE_STR_LITERAL.
 *
 * Usage:
 * using namespace tnt::literal;
 * auto str = "test"_cs;
 * std::cout << str.data << std::endl;
 */
#ifndef TNT_DISABLE_STR_LITERAL
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#elif __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
namespace literal {
template <class T, T... C>
constexpr CStr<C...> operator""_cs() {
	static_assert(std::is_same_v<char, T>, "Char type only");
	return { };
}
} // namespace literal {
#ifdef __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif
#endif // #ifndef TNT_DISABLE_STR_LITERAL

} // namespace tnt {
