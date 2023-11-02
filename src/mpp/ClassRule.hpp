/*
 * Copyright 2010-2023 Tarantool AUTHORS: please see AUTHORS file.
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

#include "../Utils/Common.hpp"

template <class T> struct mpp_rule;
template <class T> struct mpp_enc_rule;
template <class T> struct mpp_dec_rule;

namespace mpp {

namespace details {
template <class T>
struct has_member_base {
	static constexpr bool value = !std::is_member_pointer_v<T>;
};

template <class T, typename = void>
struct has_member_rule_h : std::false_type {};
template <class T>
struct has_member_rule_h<T, std::void_t<decltype(T::mpp)>>
	: has_member_base<decltype(&T::mpp)> {};

template <class T, typename = void>
struct has_member_enc_rule_h : std::false_type {};
template <class T>
struct has_member_enc_rule_h<T, std::void_t<decltype(T::mpp_enc)>>
	: has_member_base<decltype(&T::mpp_enc)> {};

template <class T, typename = void>
struct has_member_dec_rule_h : std::false_type {};
template <class T>
struct has_member_dec_rule_h<T, std::void_t<decltype(T::mpp_dec)>>
	: has_member_base<decltype(&T::mpp_dec)> {};

template <class T, typename = void>
struct has_spec_rule_h : std::false_type {};
template <class T>
struct has_spec_rule_h<T, std::void_t<decltype(mpp_rule<T>::value)>>
	: has_member_base<decltype(&mpp_rule<T>::value)> {};

template <class T, typename = void>
struct has_spec_enc_rule_h : std::false_type {};
template <class T>
struct has_spec_enc_rule_h<T, std::void_t<decltype(mpp_enc_rule<T>::value)>>
	: has_member_base<decltype(&mpp_enc_rule<T>::value)> {};

template <class T, typename = void>
struct has_spec_dec_rule_h : std::false_type {};
template <class T>
struct has_spec_dec_rule_h<T, std::void_t<decltype(mpp_dec_rule<T>::value)>>
	: has_member_base<decltype(&mpp_dec_rule<T>::value)> {};
} // namespace details

template <class T>
constexpr bool has_enc_rule_v =
	details::has_member_rule_h<T>::value ||
	details::has_member_enc_rule_h<T>::value ||
	details::has_spec_rule_h<T>::value ||
	details::has_spec_enc_rule_h<T>::value;

template <class T>
constexpr bool has_dec_rule_v =
	details::has_member_rule_h<T>::value ||
	details::has_member_dec_rule_h<T>::value ||
	details::has_spec_rule_h<T>::value ||
	details::has_spec_dec_rule_h<T>::value;

template <class T>
constexpr auto& get_enc_rule()
{
	if constexpr (details::has_member_enc_rule_h<T>::value)
		return T::mpp_enc;
	else if constexpr (details::has_member_rule_h<T>::value)
		return T::mpp;
	else if constexpr (details::has_spec_enc_rule_h<T>::value)
		return mpp_enc_rule<T>::value;
	else if constexpr (details::has_spec_rule_h<T>::value)
		return mpp_rule<T>::value;
	else
		static_assert(tnt::always_false_v<T>,
			      "Failed to find a rule for given type");
}

template <class T>
constexpr auto& get_dec_rule()
{
	if constexpr (details::has_member_dec_rule_h<T>::value)
		return T::mpp_dec;
	else if constexpr (details::has_member_rule_h<T>::value)
		return T::mpp;
	else if constexpr (details::has_spec_dec_rule_h<T>::value)
		return mpp_dec_rule<T>::value;
	else if constexpr (details::has_spec_rule_h<T>::value)
		return mpp_rule<T>::value;
	else
		static_assert(tnt::always_false_v<T>,
			      "Failed to find a rule for given type");
}

} // namespace mpp
