/*
 * Copyright 2010-2022 Tarantool AUTHORS: please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "Constants.hpp"

namespace mpp {

/**
 * Specificator is a wrapper around some object that additionally has some
 * settings that describe how the object must be handled by codec.
 * For example std::tuple is packed as array by default, and if you want it to
 * be packed as map you have to specify it mpp::as_map(<that tuple>).
 * Specificators usually hold reference to the object and thus the user
 * must think about original object's lifetime. The universal practice is to
 * use temporary specificators, like encoder.add(mpp::as_map(<that tuple>)).
 */

template <compact::Family FAMILY, class T>
constexpr auto as_family(T&& t);

template <class T> constexpr auto as_nil (T&& t) { return as_family<compact::MP_NIL >(std::forward<T>(t)); }
template <class T> constexpr auto as_ignr(T&& t) { return as_family<compact::MP_IGNR>(std::forward<T>(t)); }
template <class T> constexpr auto as_bool(T&& t) { return as_family<compact::MP_BOOL>(std::forward<T>(t)); }
template <class T> constexpr auto as_int (T&& t) { return as_family<compact::MP_INT >(std::forward<T>(t)); }
template <class T> constexpr auto as_flt (T&& t) { return as_family<compact::MP_FLT >(std::forward<T>(t)); }
template <class T> constexpr auto as_str (T&& t) { return as_family<compact::MP_STR >(std::forward<T>(t)); }
template <class T> constexpr auto as_bin (T&& t) { return as_family<compact::MP_BIN >(std::forward<T>(t)); }
template <class T> constexpr auto as_arr (T&& t) { return as_family<compact::MP_ARR >(std::forward<T>(t)); }
template <class T> constexpr auto as_map (T&& t) { return as_family<compact::MP_MAP >(std::forward<T>(t)); }

template <class EXT_TYPE, class T>
constexpr auto as_ext(EXT_TYPE&& e, T&& t);

#define TNT_DECLARE_TAG(name) 							\
struct name##_tag {};								\
										\
template <class T>								\
constexpr bool is_##name##_v =							\
	std::is_base_of_v<name##_tag, std::remove_reference_t<T>>

TNT_DECLARE_TAG(wrapped);
TNT_DECLARE_TAG(wrapped_family);
TNT_DECLARE_TAG(wrapped_fixed);
TNT_DECLARE_TAG(wrapped_raw);

#undef TNT_DECLARE_TAG

template <class T>
constexpr const auto& unwrap(const T& t)
{
	if constexpr (is_wrapped_v<T>)
		return t.object;
	else
		return t;
}

template <class T>
constexpr auto& unwrap(T& t)
{
	if constexpr (is_wrapped_v<T>)
		return t.object;
	else
		return t;
}

template <class T>
struct wrapped : wrapped_tag
{
	using object_t = std::remove_reference_t<T>;
	T&& object;
	constexpr explicit wrapped(T&& arg) : object(std::forward<T>(arg)) {}
};

template <compact::Family FAMILY, class BASE>
struct wrapped_family : wrapped_family_tag, BASE {
	static constexpr compact::Family family = FAMILY;
	constexpr explicit wrapped_family(BASE&& arg)
	: BASE(std::forward<BASE>(arg)) {}
};

template <compact::Family FAMILY, class T>
constexpr auto as_family(T&& t)
{
	static_assert(!is_wrapped_family_v<T>, "Family is already set");
	static_assert(!is_wrapped_raw_v<T>, "Incompatible with raw");
	static_assert(FAMILY < compact::MP_END, "Invalid family");
	static_assert(FAMILY != compact::MP_EXT, "Please use as_ext");

	if constexpr(is_wrapped_v<T>) {
		using WRAP_T = std::remove_reference_t<T>;
		return wrapped_family<FAMILY, WRAP_T>{std::forward<T>(t)};
	} else {
		using WRAP_T = wrapped<T&&>;
		return wrapped_family<FAMILY, WRAP_T>{WRAP_T{std::forward<T>(t)}};
	}
}

template <class EXT_TYPE, class BASE>
struct wrapped_ext : wrapped_family_tag, BASE {
	static constexpr compact::Family family = compact::MP_EXT;
	EXT_TYPE ext_type;
	constexpr wrapped_ext(EXT_TYPE e, BASE&& arg)
	: BASE(std::forward<BASE>(arg)), ext_type(e) {}
};

template <class EXT_TYPE, class T>
constexpr auto as_ext(EXT_TYPE&& e, T&& t)
{
	static_assert(!is_wrapped_family_v<T>, "Family is already set");
	static_assert(!is_wrapped_raw_v<T>, "Incompatible with raw");

	if constexpr(is_wrapped_v<T>) {
		using WRAP_T = std::remove_reference_t<T>;
		wrapped_ext<EXT_TYPE&&, WRAP_T>{std::forward<EXT_TYPE>(e),
						std::forward<T>(t)};
	} else {
		using WRAP_T = wrapped<T&&>;
		wrapped_ext<EXT_TYPE&&, WRAP_T>{std::forward<EXT_TYPE>(e),
						WRAP_T{std::forward<T>(t)}};
	}
}

template <class TYPE, class BASE>
struct wrapped_fixed : wrapped_fixed_tag, BASE {
	using fixed_t = TYPE;
	constexpr explicit wrapped_fixed(BASE&& arg)
	: BASE(std::forward<BASE>(arg)) {}
};

template <class TYPE, class T>
constexpr auto as_fixed(T&& t)
{
	static_assert(!is_wrapped_fixed_v<T>, "Fixed type is already set");
	static_assert(!is_wrapped_raw_v<T>, "Incompatible with raw");
	if constexpr(is_wrapped_v<T>) {
		using WRAP_T = std::remove_reference_t<T>;
		return wrapped_fixed<TYPE, WRAP_T>{std::forward<T>(t)};
	} else {
		using WRAP_T = wrapped<T&&>;
		return wrapped_fixed<TYPE, WRAP_T>{WRAP_T{std::forward<T>(t)}};
	}
}

namespace details {
template <bool is_fixed, class T>
struct get_fixed_h { using type = void; };

template <class T>
struct get_fixed_h<true, T> { using type = typename T::fixed_t; };
} // namespace details

template <class T>
using get_fixed_t = typename details::get_fixed_h<is_wrapped_fixed_v<T>, T>::type;

template <class BASE>
struct wrapped_raw : wrapped_raw_tag, BASE {
	constexpr explicit wrapped_raw(BASE&& arg)
		: BASE(std::forward<BASE>(arg)) {}
};

template <class T>
constexpr auto as_raw(T&& t)
{
	static_assert(!is_wrapped_v<T>, "Raw is incompatible with multiwrap");
	using WRAP_T = wrapped<T&&>;
	return wrapped_raw<WRAP_T>{WRAP_T{std::forward<T>(t)}};
}

/**
 * Constants are types that have a constant value enclosed in type itself,
 * as some constexpr static member of the class.
 * For the most of constants std::integral_constant works perfectly.
 * MPP_AS_CONST is just a short form of creating an integral_constant.
 * There' some complexity in creating string constants. There's a special class
 * for them - CStr, that could be instantiated in a pair of ways.
 * MPP_AS_CONSTR is just a macro that instantiates on one of those forms.
 * There are also 'as_const' and 'as_constr' macros that are disabled by
 * default.
 */
#ifndef MPP_DISABLE_AS_CONST_MACRO
#define MPP_AS_CONST(x) std::integral_constant<decltype(x), x>{}
#endif
#ifndef TNT_DISABLE_STR_MACRO
#define MPP_AS_CONSTR(x) TNT_CON_STR(x)
#else
#ifndef TNT_DISABLE_STR_LITERAL
#define MPP_AS_CONSTR(x) x##_cs
#endif
#endif

#ifdef MPP_USE_SHORT_CONST_MACROS
#define as_const(x) std::integral_constant<decltype(x), x>{}
#define as_constr(x) MPP_AS_CONSTR(x)
#endif // #ifdef MPP_USE_SHORT_CONST_MACROS

} // namespace mpp {
