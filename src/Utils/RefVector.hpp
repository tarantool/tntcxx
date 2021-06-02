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

#include <array>
#include <cstddef>
#include <stdexcept>

#include "Traits.hpp"

namespace tnt {

/**
 * Ref vector is a container that have Standard container API (at least a
 * part of it) and is connected (via references) with an existing array
 * (C or C++ STL) and a size.
 * Since it only holds references it does not have own state. That make
 * copy/move construction and destruction trivial. Usually it's better to pass
 * such a vector by value.
 * It is highly recommended to construct ref vector using special creator
 * function - make_ref_vector.
 * Additional feature is that this creator can create a temporary ref vector
 * with pointers to array and size members of some class. Such a temporary
 * can be simply converted to regular ref vector using subst function.
 * Of course actual array and size must be stored separately and must live
 * while the ref vector is used.
 */
template<class STORED_DATA, class STORED_SIZE>
class RefVector {
private:
	using DRR_STORED_DATA = demember_t<std::remove_reference_t<STORED_DATA>>;
	using DRR_STORED_SIZE = demember_t<std::remove_reference_t<STORED_SIZE>>;
	// std::array or C bounded array are expected.
	static_assert(is_tuplish_v<DRR_STORED_DATA> &&
		      is_contiguous_v<DRR_STORED_DATA>);
	// Any integer except bool are expected.
	static_assert(std::is_integral_v<DRR_STORED_SIZE> &&
		      !std::is_same_v<std::remove_cv_t<DRR_STORED_SIZE>, bool>);
	using T = tuple_element_t<0, DRR_STORED_DATA>;
	using S = DRR_STORED_SIZE;
	static constexpr size_t N = tuple_size_v<DRR_STORED_DATA>;
public:
	RefVector() = default;
	RefVector(STORED_DATA data, STORED_SIZE size) : m_data(data), m_size(size) {}

	// Quite usual container API.
	static constexpr size_t static_capacity = N;
	using value_type = T;
	using size_type = S;
	using difference_type = ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = T*;
	using const_iterator = const T*;

	void clear() const noexcept;
	pointer data() const noexcept;
	reference operator[](size_t i) const noexcept;
	size_type size() const noexcept;
	constexpr static size_t capacity() { return static_capacity; }
	void push_back(const T& value) const;
	void push_back(T&& value) const;
	template< class... ARGS>
	reference emplace_back(ARGS&&... args) const;
	iterator begin() const;
	iterator end() const;
	const_iterator cbegin() const;
	const_iterator cend() const;

private:
	STORED_DATA m_data;
	STORED_SIZE m_size;

	template<class T, class S, class OBJ>
	friend auto subst(RefVector<T, S> vec, OBJ &&obj);
};

/**
 * Main function for creation of a ref vector.
 * @param r - there are two options:
 *   1a. an array (C or C++ STL) that is passed by reference.
 *   1b. pointer to some class member which is an array.
 * @param s - there are two options:
 *   2a. a size integer that is passed by reference.
 *   2b. pointer to some class member which is size integer.
 * @return RefVector<STORED_DATA, STORED_SIZE>. Note that template parameters
 * of RefVector may differ from ARR and SIZE.
 * STORED_DATA, class STORED_SIZE>
 */
template <class ARR, class SIZE>
inline auto make_ref_vector(ARR&& r, SIZE&& s);

/**
 * Convert pointers to member to references to concrete object's members.
 * In other words this function invokes pointer to members to given object.
 * Does nothing if the ref vector is already stores regular references.
 * @param vec - a vector to convert.
 * @param obj - an object to substitute to pointer to members.
 */
template<class T, class S, class OBJ>
inline auto subst(RefVector<T, S> vec, OBJ &&obj);

/**
 * Check that the type is (cv) RefVector.
 */
namespace details {
template <class T>
struct is_ref_vector_h : std::false_type {};
template <class T, class S>
struct is_ref_vector_h<RefVector<T, S>> : std::true_type {};
} //namespace details {

template <class T>
constexpr bool is_ref_vector_v =
	details::is_ref_vector_h<std::remove_cv_t<T>>::value;

/////////////////////////////////////////////////////////////////////
///////////////////// RefVector Implementation //////////////////////
/////////////////////////////////////////////////////////////////////
template <class DATA, class SIZE>
void RefVector<DATA, SIZE>::clear() const noexcept
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	static_assert(!std::is_const_v<T>, "Unable to modify read-only data!");
	static_assert(!std::is_const_v<S>, "Unable to modify read-only data!");
	auto i = m_size;
	while (i > 0) {
		--i;
		m_data[i].~T();
	}
	m_size = 0;
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::value_type *
RefVector<DATA, SIZE>::data() const noexcept
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	return &m_data[0];
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::size_type
RefVector<DATA, SIZE>::size() const noexcept
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	return m_size;
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::reference
RefVector<DATA, SIZE>::operator[](size_t i) const noexcept
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	return m_data[i];
}

template <class DATA, class SIZE>
void
RefVector<DATA, SIZE>::push_back(const T& value) const
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	static_assert(!std::is_const_v<T>, "Unable to modify read-only data!");
	static_assert(!std::is_const_v<S>, "Unable to modify read-only data!");
	if (m_size >= N)
		throw std::bad_alloc{};
	new (&m_data[m_size]) T(value);
	++m_size;
}

template <class DATA, class SIZE>
void
RefVector<DATA, SIZE>::push_back(T&& value) const
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	static_assert(!std::is_const_v<T>, "Unable to modify read-only data!");
	static_assert(!std::is_const_v<S>, "Unable to modify read-only data!");
	if (m_size >= N)
		throw std::bad_alloc{};
	new (&m_data[m_size]) T(value);
	++m_size;
}

template <class DATA, class SIZE>
template< class... ARGS>
typename RefVector<DATA, SIZE>::reference
RefVector<DATA, SIZE>::emplace_back(ARGS&&... args) const
{
	static_assert(!tnt::is_member_ptr_v<DATA>, "Must be substituted!");
	static_assert(!tnt::is_member_ptr_v<SIZE>, "Must be substituted!");
	static_assert(!std::is_const_v<T>, "Unable to modify read-only data!");
	static_assert(!std::is_const_v<S>, "Unable to modify read-only data!");
	if (m_size >= N)
		throw std::bad_alloc{};
	new (&m_data[m_size]) T(std::forward<ARGS>(args)...);
	return m_data[m_size++];
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::iterator
RefVector<DATA, SIZE>::begin() const
{
	return &m_data[0];
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::iterator
RefVector<DATA, SIZE>::end() const
{
	return &m_data[0] + m_size;
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::const_iterator
RefVector<DATA, SIZE>::cbegin() const
{
	return &m_data[0];
}

template <class DATA, class SIZE>
typename RefVector<DATA, SIZE>::const_iterator
RefVector<DATA, SIZE>::cend() const
{
	return &m_data[0] + m_size;
}

/////////////////////////////////////////////////////////////////////
////////////////// make_ref_vector Implementation ///////////////////
/////////////////////////////////////////////////////////////////////
template <class ARR, class SIZE>
inline auto make_ref_vector(ARR&& r, SIZE&& s)
{
	using RR_ARR = std::remove_reference_t<ARR&&>;
	using DRR_ARR = demember_t<RR_ARR>;
	constexpr bool is_arr_member = tnt::is_member_ptr_v<RR_ARR>;
	static_assert(is_tuplish_v<DRR_ARR> &&
		      is_contiguous_v<DRR_ARR> &&
		      (is_arr_member || std::is_lvalue_reference_v<ARR>),
		      "Array must be a reference to C or std array"
		      " OR member pointer to such an array");

	using RR_SIZE = std::remove_reference_t<SIZE&&>;
	using DRR_SIZE = demember_t<RR_SIZE>;
	constexpr bool is_size_member = tnt::is_member_ptr_v<RR_SIZE>;
	static_assert(std::is_integral_v<DRR_SIZE> &&
		      !std::is_same_v<std::remove_cv_t<DRR_SIZE>, bool> &&
		      (is_size_member || std::is_lvalue_reference_v<SIZE>),
		      "Size must be a reference integer size"
		      " OR member pointer to such an integer");

	// Pointer to member must be stored by value, keep reference otherwise.
	// Note that we have already checked that the type is reference in case
	// when it's not a pointer to member.
	using RES_ARR = std::conditional_t<is_arr_member, RR_ARR, ARR>;
	using RES_SIZE = std::conditional_t<is_size_member, RR_SIZE, SIZE>;
	return RefVector<RES_ARR, RES_SIZE>{std::forward<ARR>(r),
					    std::forward<SIZE>(s)};
}

/////////////////////////////////////////////////////////////////////
////////////////// subst(RefVector) Implementation //////////////////
/////////////////////////////////////////////////////////////////////
template<class T, class S, class OBJ>
inline auto subst(RefVector<T, S> arr, OBJ &&obj)
{
	if constexpr(tnt::is_member_ptr_v<T> && tnt::is_member_ptr_v<S>) {
		return make_ref_vector(obj.*(arr.m_data), obj.*(arr.m_size));
	} else if constexpr(tnt::is_member_ptr_v<T> && !tnt::is_member_ptr_v<S>) {
		return make_ref_vector(obj.*(arr.m_data), arr.m_size);
	} else if constexpr(!tnt::is_member_ptr_v<T> && tnt::is_member_ptr_v<S>) {
		return make_ref_vector(arr.m_data, obj.*(arr.m_size));
	} else {
		return arr;
	}
}

} // namespace tnt {