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
#include <cstddef>

#include "../Utils/Mempool.hpp"
#include "Constants.hpp"

namespace mpp {

// See DefaultObjectMempool_t below.
template <class BUFFER, class MEMPOOL>
struct Object {
	using Buffer_t = BUFFER;
	using BufferIterator_t = typename BUFFER::iterator;
	using Mempool_t = MEMPOOL;

	explicit Object(Mempool_t& pool) : m_Pool(pool) {}
	~Object() noexcept
	{
		while (m_Child != nullptr) {
			Object *tmp = m_Child;
			m_Child = m_Child->next;
			Mempool_t& pool = tmp->m_Pool;
			tmp->~Object();
			Object::operator delete(tmp, pool);
		}
	}

	void* operator new(size_t size, Mempool_t &pool)
	{
		assert(size == sizeof(Object)); (void)size;
		return pool.allocate();
	}
	void operator delete(void *ptr, Mempool_t &pool) noexcept
	{
		pool.deallocate(ptr);
	}

	Value_t m_Value;
	const BufferIterator_t& Where() const { return m_Position; }

private:
	Object(const Object&) = delete;
	void operator=(const Object&) = delete;

	void* operator new(size_t) = delete;
	void* operator new[](size_t) = delete;
	void* operator new(size_t, const std::nothrow_t&) = delete;
	void* operator new[](size_t, const std::nothrow_t&) = delete;
	void operator delete(void *) = delete;
	void operator delete[](void *) = delete;

	Mempool_t &m_Pool;
	BufferIterator_t m_Position;
	Object *m_Next = nullptr;
	Object *m_NextShortcut[2] = {nullptr, nullptr};
	Object *m_Child = nullptr;
};

template<class BUFFER>
using DefaultObjectMempool_t =
	tnt::MempoolHolder<sizeof(Object<BUFFER, tnt::MempoolHolder<256>>)>;

} // namespace mpp {