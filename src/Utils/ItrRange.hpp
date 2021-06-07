#pragma once
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

#include <cstddef>
#include <type_traits>
#include <utility>

#include "Traits.hpp"

namespace tnt {

/**
 * ItrRange is a range described by a pair of iterators - range's begin and end.
 * Similar to boost::iterator_range.
 * By default ItrRange tries to store both iterators by lvalue references;
 * if not possible - moves iterators to itself.
 * It is highly recommended to construct range using special creator
 * function - make_itr_range.
 * Additional feature is that this creator can create a temporary itr range
 * with pointers to iterator members of some class. Such a temporary
 * can be simply converted to regular itr range using subst function.
 */
namespace details {
template <class ITR1, class _ = void>
struct ItrBeginner {
	ITR1 itr1;
	ItrBeginner(ITR1 aitr) : itr1(aitr) {}
	ITR1 begin() { return itr1; };
};
template <class ITR1>
struct ItrBeginner<ITR1, std::enable_if_t<tnt::is_member_ptr_v<ITR1>, void>> {
	ITR1 itr1;
	ItrBeginner(ITR1 aitr) : itr1(aitr) {}
};

template <class ITR2, class _ = void>
struct ItrEnder {
	ITR2 itr2;
	ItrEnder(ITR2 aitr) : itr2(aitr) {}
	ITR2 end() { return itr2; };
};
template <class ITR2>
struct ItrEnder<ITR2, std::enable_if_t<tnt::is_member_ptr_v<ITR2>, void>> {
	ITR2 itr2;
	ItrEnder(ITR2 aitr) : itr2(aitr) {}
};
} // namespace details {

template <class ITR1, class ITR2>
struct ItrRange : details::ItrBeginner<ITR1>, details::ItrEnder<ITR2> {
	using itr1_t = ITR1;
	using itr2_t = ITR2;

	ItrRange(ITR1 aitr1, ITR2 aitr2)
	: details::ItrBeginner<ITR1>(aitr1), details::ItrEnder<ITR2>(aitr2) {}
};

/**
 * Main function for creation of an iterator range.
 * Rvalue references and pointers to members are stored by value.
 */
template <class ITR1, class ITR2>
inline auto make_itr_range(ITR1 &&itr1, ITR2 &&itr2);

/**
 * Convert pointers to member to references to concrete object's members.
 * In other words this function invokes pointer to members to given object.
 * Does nothing if the range is already stores regular references.
 * @param range - a range to convert.
 * @param obj - an object to substitute to pointer to members.
 */
template<class ITR1, class ITR2, class OBJ>
inline auto subst(ItrRange<ITR1, ITR2> range, OBJ &&obj);

/**
 * Check that the type is (cv) ItrRange.
 */
namespace details {
template <class T>
struct is_itr_range_h : std::false_type {};
template <class I1, class I2>
struct is_itr_range_h<ItrRange<I1, I2>> : std::true_type {};
} //namespace details {

template <class T>
constexpr bool is_itr_range_v =
	details::is_itr_range_h<std::remove_cv_t<T>>::value;

/////////////////////////////////////////////////////////////////////
/////////////////// make_itr_range Implementation ///////////////////
/////////////////////////////////////////////////////////////////////
template <class ITR1, class ITR2>
inline auto make_itr_range(ITR1 &&itr1, ITR2 &&itr2)
{
	using RR_ITR1 = std::remove_reference_t<ITR1&&>;
	constexpr bool is_itr1_member = tnt::is_member_ptr_v<RR_ITR1>;
	using RR_ITR2 = std::remove_reference_t<ITR2&&>;
	constexpr bool is_itr2_member = tnt::is_member_ptr_v<RR_ITR2>;

	// Pointer to member must be stored by value, rvalue reverence too.
	// Keep the original type otherwise.
	using RES_ITR1 = std::conditional_t<is_itr1_member, RR_ITR1, ITR1>;
	using RES_ITR2 = std::conditional_t<is_itr2_member, RR_ITR2, ITR2>;
	return ItrRange<RES_ITR1, RES_ITR2>{std::forward<ITR1>(itr1),
					    std::forward<ITR2>(itr2)};
}

/////////////////////////////////////////////////////////////////////
/////////////////// subst(ItrRange) Implementation //////////////////
/////////////////////////////////////////////////////////////////////
template<class ITR1, class ITR2, class OBJ>
inline auto subst(ItrRange<ITR1, ITR2> range, OBJ &&obj)
{
	if constexpr(tnt::is_member_ptr_v<ITR1> &&
		     tnt::is_member_ptr_v<ITR2>) {
		return make_itr_range(obj.*(range.itr1), obj.*(range.itr2));
	} else if constexpr(tnt::is_member_ptr_v<ITR1> &&
			    !tnt::is_member_ptr_v<ITR2>) {
		return make_itr_range(obj.*(range.itr1), range.itr2);
	} else if constexpr(!tnt::is_member_ptr_v<ITR1> &&
			    tnt::is_member_ptr_v<ITR2>) {
		return make_itr_range(range.itr1, obj.*(range.itr2));
	} else {
		return range;
	}
}

} // namespace tnt {