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

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tnt {

template <bool ENABLE_STATS>
class MempoolStats {
protected:
	void statAddSlab() { ++m_SlabCount; }
	void statAddBlock() { ++m_BlockCount; }
	void statDelBlock() { --m_BlockCount; }
public:
	/** Count of allocated (used) blocks. */
	size_t statBlockCount() const { return m_BlockCount; }
	/** Count of allocated (total) slabs. */
	size_t statSlabCount() const { return m_SlabCount; }
private:
	size_t m_SlabCount = 0;
	size_t m_BlockCount = 0;
};

template <>
class MempoolStats<false> {
protected:
	void statAddSlab() { }
	void statAddBlock() { }
	void statDelBlock() { }
public:
	/** Disabled. return SIZE_MAX. */
	size_t statBlockCount() const { return SIZE_MAX; }
	/** Disabled. return SIZE_MAX. */
	size_t statSlabCount() const { return SIZE_MAX; }
};

/**
 * Classic mempool allocator designed for fast allocation of memory @a blocks
 * of compile-time specified size. Internally allocated significantly bigger
 * memory @a slabs that are split into blocks.
 * Slabs are allocated with operator new. It is expected that on memory error
 * std::bad_alloc is thrown which should be caught by user.
 * Uses a single linked reuse list.
 * By design alignment of block is the highest power of two that divides block
 * size. In particular, if block size is a power of two, then the alignment is
 * the same as block size.
 * Provides static @a defaultInstance, with certain benefits and drawbacks.
 * @tparam B size of an allocation block. mustn't be less than sizeof(void*).
 * @tparam M slab size / block size ratio. must be > 1 (and should be > 8).
 * @tparam ENABLE_STATS enable stat calculation.
 */
template <size_t B, size_t M = 256, bool ENABLE_STATS = false>
class MempoolInstance : public MempoolStats<ENABLE_STATS> {
private:
	static_assert(B >= sizeof(void *), "Block size is too small");
	static_assert(M > 1, "Multiplicator is too small");
	static_assert(B * M % sizeof(void*) == 0, "Alignment is too low");

	/* Alignment of block. */
	static constexpr size_t BA = (B ^ (B - 1)) / 2 + 1;
	/* Alignment of slab. */
	static constexpr size_t SA = BA > sizeof(void*) ? BA : sizeof(void*);
	static_assert((BA & (BA - 1)) == 0, "Smth went wrong");
	static_assert((SA & (SA - 1)) == 0, "Smth went wrong");
	static_assert(BA <= B, "Smth went wrong");

	struct alignas(SA) Slab {
		Slab *next;
		char data[B * M - sizeof(next)];
		explicit Slab(Slab *list) : next(list) { }
	};
	static_assert(sizeof(Slab) == B * M, "Smth went wrong");
	static constexpr size_t FIRST_OFFSET = B - sizeof(Slab::next);

	using Stats_t = MempoolStats<ENABLE_STATS>; 

public:
	// Constants for stat.
	static constexpr size_t REAL_SIZE = B;
	static constexpr size_t BLOCK_SIZE = B;
	static constexpr size_t SLAB_SIZE = B * M;
	static constexpr size_t BLOCK_ALIGN = BA;
	static constexpr size_t SLAB_ALIGN = SA;

	MempoolInstance() = default;
	~MempoolInstance() noexcept
	{
		while (m_SlabList != nullptr) {
			Slab *tmp = m_SlabList;
			m_SlabList = m_SlabList->next;
			delete tmp;
		}
	}
	static MempoolInstance& defaultInstance()
	{
		static MempoolInstance instance;
		return instance;
	}
	char *allocate()
	{
		if (m_SlabDataBeg != m_SlabDataEnd) {
			char *res = m_SlabDataBeg;
			m_SlabDataBeg += B;
			Stats_t::statAddBlock();
			return res;
		}
		if (m_FreeList != nullptr) {
			char *res = m_FreeList;
			memcpy(&m_FreeList, m_FreeList, sizeof(char *));
			Stats_t::statAddBlock();
			return res;
		}
		m_SlabList = new Slab(m_SlabList);
		Stats_t::statAddSlab();
		m_SlabDataBeg = m_SlabList->data + FIRST_OFFSET + B;
		m_SlabDataEnd = m_SlabList->data + sizeof(m_SlabList->data);
		Stats_t::statAddBlock();
		return m_SlabList->data + FIRST_OFFSET;
	}
	void deallocate(char *ptr) noexcept
	{
#ifndef NDEBUG
		const char* trash = "\xab\xad\xba\xbe";
		for (size_t i = 0; i < B; i++)
			ptr[i] = trash[i % 4];
#endif
		memcpy(ptr, &m_FreeList, sizeof(m_FreeList));
		m_FreeList = ptr;
		Stats_t::statDelBlock();
	}

	/**
	 * Debug selfcheck
	 * Return 0 if there's no problems. Otherwise see the code below.
	 */
	int selfcheck() const
	{
		int res = 0;

		size_t calc_slab_count = 0;
		Slab *s = m_SlabList;
		while (s != nullptr) {
			s = s->next;
			calc_slab_count++;
		}

		size_t calc_free_block_count = 0;
		char *f = m_FreeList;
		while (f != nullptr) {
			memcpy(&f, f, sizeof(char *));
			calc_free_block_count++;
		}

		if constexpr (ENABLE_STATS) {
			size_t sc =  Stats_t::statSlabCount();
			if (calc_slab_count != sc)
				res |= 1;

			size_t bc =  Stats_t::statBlockCount();
			size_t total_block_count = sc * (M - 1);
			size_t prealloc = (m_SlabDataEnd - m_SlabDataBeg) / B;
			size_t expect_free = total_block_count - prealloc - bc;
			if (calc_free_block_count != expect_free)
				res |= 2;

		} else {
			(void)calc_slab_count;
			(void)calc_free_block_count;
		}
		return res;
	}

private:
	Slab *m_SlabList = nullptr;
	char *m_FreeList = nullptr;
	char *m_SlabDataBeg = nullptr;
	char *m_SlabDataEnd = nullptr;
};

/**
 * Mempool holder is an object that holds a reference to mempool instance.
 * Provides exactly the same API as mempool instance (except copying), all the
 * calls are bypassed to referenced instance.
 * @sa MempoolInstance.
 */
template <size_t B, size_t M = 256, bool ENABLE_STATS = false>
class MempoolHolder {
private:
	using Base_t = MempoolInstance<B, M, ENABLE_STATS>;
public:
	MempoolHolder() : m_Instance(Base_t::defaultInstance()) {}
	explicit MempoolHolder(Base_t &instance) : m_Instance(instance) {}
	char *allocate() { return m_Instance.allocate(); }
	void deallocate(char *ptr) noexcept { m_Instance.deallocate(ptr); }
	int selfcheck() const { return m_Instance.selfcheck(); } 

	static constexpr size_t REAL_SIZE = Base_t::REAL_SIZE;
	static constexpr size_t BLOCK_SIZE = Base_t::BLOCK_SIZE;
	static constexpr size_t SLAB_SIZE = Base_t::SLAB_SIZE;
	static constexpr size_t BLOCK_ALIGN = Base_t::BLOCK_ALIGN;
	static constexpr size_t SLAB_ALIGN = Base_t::SLAB_ALIGN;
	/** See MempoolStats<ENABLE_STATS>::statBlockCount() description. */
	size_t statBlockCount() const { return m_Instance.statBlockCount(); }
	/** See MempoolStats<ENABLE_STATS>::statSlabCount() description. */
	size_t statSlabCount() const { return m_Instance.statSlabCount(); }
private:
	Base_t &m_Instance;
};

/**
 * Mempool static is an object without state.
 * Provides exactly the same API as mempool instance (except copying), all the
 * calls are bypassed to the default mempool's instance.
 * @sa MempoolInstance.
 */
template <size_t B, size_t M = 256, bool ENABLE_STATS = false>
class MempoolStatic {
private:
	using Base_t = MempoolInstance<B, M, ENABLE_STATS>;
	static Base_t& instance() { return Base_t::defaultInstance(); }
public:
	static char *allocate() { return instance().allocate(); }
	static void deallocate(char *ptr) noexcept { instance().deallocate(ptr); }
	int selfcheck() const { return instance().selfcheck(); } 

	static constexpr size_t REAL_SIZE = Base_t::REAL_SIZE;
	static constexpr size_t BLOCK_SIZE = Base_t::BLOCK_SIZE;
	static constexpr size_t SLAB_SIZE = Base_t::SLAB_SIZE;
	static constexpr size_t BLOCK_ALIGN = Base_t::BLOCK_ALIGN;
	static constexpr size_t SLAB_ALIGN = Base_t::SLAB_ALIGN;
	/** See MempoolStats<ENABLE_STATS>::statBlockCount() description. */
	size_t statBlockCount() const { return instance().statBlockCount(); }
	/** See MempoolStats<ENABLE_STATS>::statSlabCount() description. */
	size_t statSlabCount() const { return instance().statSlabCount(); }
};

} // namespace tnt {
