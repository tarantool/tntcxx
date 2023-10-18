/*
 * Copyright 2010-2021 Tarantool AUTHORS: please see AUTHORS file.
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

namespace mpp {

template <class T> struct mpp_rule;
template <class T> struct mpp_enc_rule;
template <class T> struct mpp_dec_rule;

template <class T> constexpr auto get_member_rule    (const T&) -> decltype(T::mpp_rule    )& { return T::mpp_rule    ; }
template <class T> constexpr auto get_member_enc_rule(const T&) -> decltype(T::mpp_enc_rule)& { return T::mpp_enc_rule; }
template <class T> constexpr auto get_member_dec_rule(const T&) -> decltype(T::mpp_dec_rule)& { return T::mpp_dec_rule; }

template <class T> constexpr auto get_specialization_rule    (const T&) -> decltype(mpp_rule<T>    ::value)& { return mpp_rule<T>    ::value; }
template <class T> constexpr auto get_specialization_enc_rule(const T&) -> decltype(mpp_enc_rule<T>::value)& { return mpp_enc_rule<T>::value; }
template <class T> constexpr auto get_specialization_dec_rule(const T&) -> decltype(mpp_dec_rule<T>::value)& { return mpp_dec_rule<T>::value; }

#define MAKE_RULE_CHECKER(name)							\
template <class T, typename _ = void>						\
struct has_##name : std::false_type {};						\
template <class T>								\
struct has_##name<T,								\
	std::void_t<decltype(get_##name(std::declval<T>()))>>			\
	: std::true_type {};							\
template <class T>								\
constexpr bool has_##name##_v = has_##name<T>::value

MAKE_RULE_CHECKER(member_rule);
MAKE_RULE_CHECKER(member_enc_rule);
MAKE_RULE_CHECKER(member_dec_rule);

MAKE_RULE_CHECKER(specialization_rule);
MAKE_RULE_CHECKER(specialization_enc_rule);
MAKE_RULE_CHECKER(specialization_dec_rule);

#undef MAKE_RULE_CHECKER

template <class T>
constexpr auto& get_enc_rule(const T& t)
{
	if constexpr (has_member_enc_rule_v<T>)
		return get_member_enc_rule<T>(t);
	else if constexpr (has_member_rule_v<T>)
		return get_member_rule<T>(t);
	else if constexpr (has_specialization_enc_rule_v<T>)
		return get_specialization_enc_rule<T>(t);
	else if constexpr (has_specialization_rule_v<T>)
		return get_specialization_rule<T>(t);
	else {
		static_assert(tnt::always_false_v<T>,
			      "Failed to find a rule for given type! "
			      "Check the type passed to encoder, "
			      "for general objects provide a rule");
	}
}

