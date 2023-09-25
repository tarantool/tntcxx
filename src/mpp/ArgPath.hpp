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

#include <cstdint>
#include <functional>
#include <utility>

#include "../Utils/Traits.hpp"

namespace mpp {

template <int64_t ...P>
struct Path {
	template <class ...T>
	constexpr auto&& operator()(T&& ...t) const
	{
		return (std::tuple<T&& ...>(std::forward<T>(t)...) >>
			... >> Get<P>{});
	}
private:
	template <int64_t Q>
	struct Get {
		template <class T>
		static constexpr T&& unwrap(T&& t)
		{
			return t;
		}
		template <class T>
		static constexpr auto&& unwrap(std::reference_wrapper<T> r)
		{
			return r.get();
		}
		template <class T>
		friend constexpr auto&& operator>>(T&& t, Get<Q>)
		{
			return unwrap(tnt::get<Q>(std::forward<T>(t)));
		}
	};
};

} // namespace mpp
