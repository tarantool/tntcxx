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

#include <sys/uio.h> /* struct iovec */

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <type_traits>

#include "../Utils/Mempool.hpp"
#include "../Utils/List.hpp"
#include "../Utils/CStr.hpp"

namespace tnt {

/**
 * Exception safe C++ IO buffer.
 *
 * Allocator requirements (API):
 * allocate() - allocation method, must throw an exception in case it fails.
 * Must return a chunk of memory, which end is aligned by N.
 * In other words, address of a the next after the last byte in a chunk must
 * be round in terms of `% N == 0` (note that N is always a power of 2).
 * Returns chunk of memory of @REAL_SIZE size (which is less or equal to N).
 * deallocate() - release method, takes a pointer to memory allocated
 * by @allocate and frees it. Must not throw an exception.
 * REAL_SIZE - constant determines real size of allocated chunk (excluding
 * overhead taken by allocator).
 */
template <size_t N = 16 * 1024, class allocator = MempoolHolder<N>>
class Buffer
{
private:
	/** =============== Block definition =============== */
	/** Blocks are organized into linked list. */
	struct Block : SingleLink<Block>
	{
		Block(List<Block>& addTo, size_t aid)
			: SingleLink<Block>(addTo, true), id(aid) {}

		/**
		 * Each block is enumerated with incrementally increasing
		 * sequence id.
		 * It is used to compare block's positions in the buffer.
		 */
		size_t id;

		/**
		 * Block itself is allocated in the same chunk so the size
		 * of available memory to keep the data is less than allocator
		 * provides.
		 */
		static constexpr size_t DATA_SIZE = allocator::REAL_SIZE -
			sizeof(SingleLink<Block>) - sizeof(id);
		static constexpr size_t DATA_OFFSET = N - DATA_SIZE;
		char data[DATA_SIZE];

		/**
		 * Default new/delete are prohibited.
		 */
		void* operator new(size_t size) = delete;
		void operator delete(void *ptr) = delete;

		char  *begin() { return data; }
		char  *end()   { return data + DATA_SIZE; }
		// Find a block by pointer to its data (any byte of it).
		// The pointer must point to one of characters in data member.
		static Block *byPtr(const char *p)
		{
			return (Block *) (((uintptr_t) p & ~(N - 1)) |
					  (N - allocator::REAL_SIZE));
		}
	};

	Block *newBlock(size_t block_id);
	Block *newBlock() { return newBlock(m_blocks.last().id + 1); }
	void delBlock(Block *b);
	/** Check whether two pointers point to the same block. */
	bool isSameBlock(const char *ptr1, const char *ptr2);
	/** Count number of bytes are in block starting from byte @a ptr. */
	size_t leftInBlock(const char *ptr);
	/**
	 * Check whether the pointer points to the end of block, i.e.
	 * to the next byte after the last valid byte in block.
	 * */
	bool isEndOfBlock(const char *ptr);

public:
	/** =============== Convenient wrappers =============== */

	/**
	 * A pair data+size for convenient write to a buffer.
	 */
	struct WData {
		const char* data;
		size_t size;
	};

	/**
	 * A pair data+size for convenient read from a buffer.
	 */
	struct RData {
		char* data;
		size_t size;
	};

	/**
	* Special wrapper for reserving a place for an object of given size.
	*/
	struct Reserve {
		size_t size;
	};

	/**
	* Special wrapper for skipping an object of given size.
	*/
	struct Skip {
		size_t size;
	};

	/** =============== Iterator definition =============== */
	// Dummy class to be a base of light_iterator.
	struct light_base {
		template <class ...T>
		light_base(const T&...) {}
		static const bool insert, remove, unlink, isDetached, isFirst, isLast, next, prev, selfCheck;
	};
	template <bool LIGHT>
	class iterator_common
		: public std::iterator<std::input_iterator_tag, char>,
		  public std::conditional_t<LIGHT, light_base, SingleLink<iterator_common<LIGHT>>>
	{
	public:
		using Base_t = std::conditional_t<LIGHT, light_base, SingleLink<iterator_common<LIGHT>>>;
		USING_LIST_LINK_METHODS(Base_t);

		iterator_common();
		iterator_common(Buffer *buffer, char *offset, bool is_head);
		iterator_common(char *offset);
		iterator_common(const iterator_common &other) = delete;
		iterator_common(iterator_common &other);
		iterator_common(iterator_common &&other) noexcept = default;

		iterator_common<true> enlight() const noexcept;

		iterator_common& operator = (const iterator_common& other) = delete;
		iterator_common& operator = (iterator_common& other);
		iterator_common& operator = (iterator_common&& other) noexcept = default;
		iterator_common& operator ++ ();
		iterator_common& operator += (size_t step);
		iterator_common operator + (size_t step);
		const char& operator * () const { return *m_position; }
		char& operator * () { return *m_position; }
		template <bool OTHER_LIGHT>
		bool operator == (const iterator_common<OTHER_LIGHT> &a) const;
		template <bool OTHER_LIGHT>
		bool operator != (const iterator_common<OTHER_LIGHT> &a) const;
		template <bool OTHER_LIGHT>
		bool operator  < (const iterator_common<OTHER_LIGHT> &a) const;
		template <bool OTHER_LIGHT>
		size_t operator - (const iterator_common<OTHER_LIGHT> &a) const;
		bool has_contiguous(size_t size) const;

		/**
		 * Copy content of @a buf of size @a size (or object @a t) to
		 * the position in buffer @a itr pointing to.
		 */
		void set(WData data);
		template <class T>
		void set(T&& t);
		template <char... C>
		void set(CStr<C...>);

		/**
		 * Copy content of @a buf of size @a size (or object @a t) to
		 * the position in buffer @a itr pointing to. Advance the
		 * iterator to the end of value.
		 */
		void write(WData data);
		template <class T>
		void write(T&& t);
		template <char... C>
		void write(CStr<C...>);
		void write(Reserve data) { operator+=(data.size); }

		/**
		 * Copy content of data iterator pointing to to the buffer
		 * @a buf of size @a size.
		 */
		void get(RData data);
		template <class T>
		void get(T& t);
		template <class T>
		T get();

		/**
		 * Copy content of data iterator pointing to to the buffer
		 * @a buf of size @a size. Advance the iterator to the end of
		 * value.
		 */
		void read(RData data);
		template <class T>
		void read(T& t);
		template <class T>
		T read();
		void read(Skip data) { operator+=(data.size); }

	private:
		/** Adjust iterator_common's position in list of iterators after
		 * moveForward. */
		void adjustPositionForward();
		void moveForward(size_t step);
		void moveBackward(size_t step);
		Block *getBlock() const { return Block::byPtr(m_position); }

		/** Position inside block. */
		char *m_position;

		friend class Buffer;
	};
	using iterator = iterator_common<false>;
	using light_iterator = iterator_common<true>;

	/** =============== Buffer definition =============== */
	/** Copy of any kind is disabled. */
	Buffer(const allocator& all = allocator());
	Buffer(const Buffer& buf) = delete;
	Buffer& operator = (const Buffer& buf) = delete;
	~Buffer();

	/**
	 * Return iterator pointing to the start/end of buffer.
	 */
	template <bool LIGHT>
	iterator_common<LIGHT> begin() { return iterator_common<LIGHT>(this, m_begin, true); }
	template <bool LIGHT>
	iterator_common<LIGHT> end() { return iterator_common<LIGHT>(this, m_end, false); }
	iterator begin() { return iterator(this, m_begin, true); }
	iterator end() { return iterator(this, m_end, false); }

	/**
	 * Copy content of an object to the buffer's tail (append data).
	 * Can cause reallocation that may throw.
	 */
	void write(WData data);
	template <class T>
	void write(const T& t);
	template <char... C>
	void write(CStr<C...>);
	void write(Reserve reserve);

	void dropBack(size_t size);
	void dropFront(size_t size);

	/**
	 * Get and set current end of the buffer.
	 */
	char* getEnd() noexcept;
	void setEnd(char *) noexcept;

	/**
	 * Guard that get and stores current end and restores it on destruction
	 * unless disarmed.
	 */
	struct EndGuard {
		EndGuard(Buffer &buf) noexcept : m_buffer(buf), m_end(buf.getEnd()) {}
		~EndGuard() noexcept;
		EndGuard(const EndGuard&) = delete;
		EndGuard& operator=(const EndGuard&) = delete;
		void arm(bool armed = true) noexcept { m_disarmed = !armed; }
		void disarm(bool disarmed = true) noexcept { m_disarmed = disarmed; }
	private:
		bool m_disarmed = false;
		Buffer &m_buffer;
		char *m_end;
	};
	EndGuard endGuard() { return EndGuard{*this}; }

	/**
	 * Insert free space of size @a size at the position @a itr pointing to.
	 * Move other iterators and reallocate space on demand. @a size must
	 * be less than block size.
	 */
	void insert(const iterator &itr, size_t size);

	/**
	 * Release memory of size @a size at the position @a itr pointing to.
	 */
	void release(const iterator &itr, size_t size);

	/** Resize memory chunk @a itr pointing to. */
	void resize(const iterator &itr, size_t old_size, size_t new_size);

	/**
	 * Determine whether the buffer has @a size bytes after @ itr.
	 */
	template <bool LIGHT>
	bool has(const iterator_common<LIGHT>& itr, size_t size);

	/**
	 * Drop data till the first existing iterator. In case there's
	 * no iterators erase whole buffer.
	 */
	void flush();

	/**
	 * Move content of buffer starting from position @a itr pointing to
	 * to array of iovecs with size of @a max_size. Each buffer block
	 * is assigned to separate iovec (so at one we copy max @a max_size
	 * blocks).
	 */
	template <bool LIGHT>
	size_t getIOV(const iterator_common<LIGHT> &itr,
	       struct iovec *vecs, size_t max_size);
	template <bool LIGHT1, bool LIGHT2>
	size_t getIOV(const iterator_common<LIGHT1> &start,
		      const iterator_common<LIGHT2> &end,
		      struct iovec *vecs, size_t max_size);

	/** Return true if there's no data in the buffer. */
	bool empty() const { return m_begin == m_end; }

	/** Return 0 if everythng is correct. */
	int debugSelfCheck() const;

	static int blockSize() { return N; }
#ifndef NDEBUG
	/** Print content of buffer to @a output in human-readable format. */
	template<size_t size, class alloc>
	friend std::string dump(Buffer<size, alloc> &buffer);
#endif
private:
	class List<Block> m_blocks;
	/** List of all data iterators created via @a begin method. */
	class List<iterator> m_iterators;
	/**
	 * Offset of the data in the first block. Data may start not from
	 * the beginning of the block due to ::dropFront invocation.
	 */
	char *m_begin;
	/** Last block can be partially filled, so store end border as well. */
	char *m_end;

	/** Instance of an allocator. */
	allocator m_all;
};

// Macro that explains to compiler that the expression os always true.
#define TNT_INV(expr) if (!(expr)) (assert(false), __builtin_unreachable())

#define TNT_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define TNT_UNLIKELY(expr) __builtin_expect(!!(expr), 0)

template <size_t N, class allocator>
typename Buffer<N, allocator>::Block *
Buffer<N, allocator>::newBlock(size_t block_id)
{
	char *ptr = m_all.allocate();
	assert(ptr != nullptr);
	assert((uintptr_t(ptr) + m_all.REAL_SIZE) % N == 0);
	return ::new(ptr) Block(m_blocks, block_id);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::delBlock(Block *b)
{
	b->~Block();
	m_all.deallocate(reinterpret_cast<char *>(b));
}

template <size_t N, class allocator>
bool
Buffer<N, allocator>::isSameBlock(const char *ptr1, const char *ptr2)
{
	uintptr_t addr1 = (uintptr_t)ptr1;
	uintptr_t addr2 = (uintptr_t)ptr2;
	/*
	 * Blocks are aligned as N (N is a power of two). That makes pointers
	 * to block to consist of two pasts - high bits part that describes
	 * block address and low bits part that describes offset in the block.
	 * To be precise, having a pointer into block `ptr`, `ptr & (N - 1)`
	 * would be offset in a block, which address is `ptr & ~(N - 1)`.
	 * In order to check whether two pointers point to the same block we
	 * must compare high bit parts of addresses:
	 * return add1 / N == addr2 / N;
	 * But it make three operations. We can make in with two, using a trick:
	 * XOR fills with zero bytes equal parts of addresses.
	 */
	return (addr1 ^ addr2) < N;
}

template <size_t N, class allocator>
size_t
Buffer<N, allocator>::leftInBlock(const char *ptr)
{
	uintptr_t addr = (uintptr_t)ptr;
	/*
	 * Blocks are aligned as N (N is a power of two). That makes pointers
	 * to block to consist of two pasts - high bits part that describes
	 * block address and low bits part that describes offset in the block.
	 * In order to check how many bytes are in block we have to offset in
	 * block and compare it with block boundary.
	 */
	return N - addr % N;
}

template <size_t N, class allocator>
bool
Buffer<N, allocator>::isEndOfBlock(const char *ptr)
{
	uintptr_t addr = (uintptr_t)ptr;
	/*
	 * Blocks are aligned as N (N is a power of two). That makes pointers
	 * to block to consist of two pasts - high bits part that describes
	 * block address and low bits part that describes offset in the block.
	 * That means that end-of-block address is round in terms of N.
	 */
	return addr % N == 0;
 }

template <size_t N, class allocator>
template <bool LIGHT>
Buffer<N, allocator>::iterator_common<LIGHT>::iterator_common()
	: m_position(nullptr)
{
}

template <size_t N, class allocator>
template <bool LIGHT>
Buffer<N, allocator>::iterator_common<LIGHT>::iterator_common(Buffer *buffer,
							      char *offset,
							      bool is_head)
	: Base_t(buffer->m_iterators, !is_head),
	  m_position(offset)
{
}

template <size_t N, class allocator>
template <bool LIGHT>
Buffer<N, allocator>::iterator_common<LIGHT>::iterator_common(char *offset)
	: m_position(offset)
{
}

template <size_t N, class allocator>
template <bool LIGHT>
Buffer<N, allocator>::iterator_common<LIGHT>::iterator_common(iterator_common& other)
	: Base_t(other, false),
	  m_position(other.m_position)
{
}

template <size_t N, class allocator>
template <bool LIGHT>
typename Buffer<N, allocator>::template iterator_common<true>
Buffer<N, allocator>::iterator_common<LIGHT>::enlight() const noexcept
{
	return iterator_common<true>(m_position);
}

template <size_t N, class allocator>
template <bool LIGHT>
typename Buffer<N, allocator>::template iterator_common<LIGHT>&
Buffer<N, allocator>::iterator_common<LIGHT>::operator= (iterator_common& other)
{
	if (TNT_UNLIKELY(this == &other))
		return *this;
	m_position = other.m_position;
	other.insert(*this);
	return *this;
}

template <size_t N, class allocator>
template <bool LIGHT>
typename Buffer<N, allocator>::template iterator_common<LIGHT>&
Buffer<N, allocator>::iterator_common<LIGHT>::operator++()
{
	moveForward(1);
	adjustPositionForward();
	return *this;
}

template <size_t N, class allocator>
template <bool LIGHT>
typename Buffer<N, allocator>::template iterator_common<LIGHT>&
Buffer<N, allocator>::iterator_common<LIGHT>::operator+=(size_t step)
{
	moveForward(step);
	/* Adjust iterator_common's position in the list of iterators. */
	adjustPositionForward();
	return *this;
}

template <size_t N, class allocator>
template <bool LIGHT>
typename Buffer<N, allocator>::template iterator_common<LIGHT>
Buffer<N, allocator>::iterator_common<LIGHT>::operator+(size_t step)
{
	iterator_common res(*this);
	res += step;
	return res;
}

template <size_t N, class allocator>
template <bool LIGHT>
template <bool OTHER_LIGHT>
bool
Buffer<N, allocator>::iterator_common<LIGHT>::operator==(const iterator_common<OTHER_LIGHT>& a) const
{
	return m_position == a.m_position;
}

template <size_t N, class allocator>
template <bool LIGHT>
template <bool OTHER_LIGHT>
bool
Buffer<N, allocator>::iterator_common<LIGHT>::operator!=(const iterator_common<OTHER_LIGHT>& a) const
{
	return m_position != a.m_position;
}

template <size_t N, class allocator>
template <bool LIGHT>
template <bool OTHER_LIGHT>
bool
Buffer<N, allocator>::iterator_common<LIGHT>::operator<(const iterator_common<OTHER_LIGHT>& a) const
{
	uintptr_t this_addr = (uintptr_t)m_position;
	uintptr_t that_addr = (uintptr_t)a.m_position;
	if (TNT_UNLIKELY((this_addr ^ that_addr) < N))
		return m_position < a.m_position;
	assert(getBlock()->id != a.getBlock()->id);
	return getBlock()->id < a.getBlock()->id;
}

template <size_t N, class allocator>
template <bool LIGHT>
template <bool OTHER_LIGHT>
size_t
Buffer<N, allocator>::iterator_common<LIGHT>::operator-(const iterator_common<OTHER_LIGHT>& a) const
{
	size_t res = (getBlock()->id - a.getBlock()->id) * Block::DATA_SIZE;
	res += (uintptr_t) m_position % N;
	res -= (uintptr_t) a.m_position % N;
	return res;
}

template <size_t N, class allocator>
template <bool LIGHT>
bool
Buffer<N, allocator>::iterator_common<LIGHT>::has_contiguous(const size_t size) const
{
	return size <= N - (uintptr_t) m_position % N;
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::adjustPositionForward()
{
	if constexpr (!LIGHT) {
		if (TNT_LIKELY(isLast() || !(next() < *this)))
			return;
		iterator_common *itr = &next();
		while (!itr->isLast() && itr->next() < *this)
			itr = &itr->next();
		itr->insert(*this);
	}
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::moveForward(size_t step)
{
	TNT_INV((uintptr_t) m_position % N >= Block::DATA_OFFSET);
	while (TNT_UNLIKELY(step >= N - ((uintptr_t )m_position % N)))
	{
		step -= N - ((uintptr_t )m_position % N);
		m_position = getBlock()->next().data;
		TNT_INV((uintptr_t) m_position % N >= Block::DATA_OFFSET);
	}
	m_position += step;
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::moveBackward(size_t step)
{
	TNT_INV((uintptr_t) m_position % N >= Block::DATA_OFFSET);
	while (TNT_UNLIKELY(step > (uintptr_t) m_position % N - Block::DATA_OFFSET)) {
		step -= (uintptr_t) m_position % N - Block::DATA_OFFSET + 1;
		m_position = getBlock()->prev().data + (Block::DATA_SIZE - 1);
		TNT_INV((uintptr_t) m_position % N >= Block::DATA_OFFSET);
	}
	m_position -= step;
}

template <size_t N, class allocator>
Buffer<N, allocator>::Buffer(const allocator &all) : m_all(all)
{
	static_assert((N & (N - 1)) == 0, "N must be power of 2");
	static_assert(allocator::REAL_SIZE % alignof(Block) == 0,
		      "Allocation size must be multiple of 16 bytes");
	static_assert(sizeof(Block) == allocator::REAL_SIZE,
		      "size of buffer block is expected to match with "
			      "allocation size");
	static_assert(Block::DATA_OFFSET + Block::DATA_SIZE == N,
		      "DATA_OFFSET must be offset of data");

	Block *b = newBlock(0);
	m_begin = m_end = b->data;
}

template <size_t N, class allocator>
Buffer<N, allocator>::~Buffer()
{
	/* Delete blocks and release occupied memory. */
	while (!m_blocks.isEmpty())
		delBlock(&m_blocks.first());
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::write(WData data)
{
	assert(data.size != 0);

	char *new_end = m_end + data.size;
	if (TNT_LIKELY(isSameBlock(m_end, new_end))) {
		// new_addr is still in block. just copy and advance.
		memcpy(m_end, data.data, data.size);
		m_end = new_end;
		return;
	}

	EndGuard guard(*this);

	// Flipped out-of-block bit, go to the next block.
	size_t left_in_block = leftInBlock(m_end);
	memcpy(m_end, data.data, left_in_block);
	data.size -= left_in_block;
	data.data += left_in_block;

	m_end = newBlock()->begin();
	while (TNT_UNLIKELY(data.size >= Block::DATA_SIZE)) {
		memcpy(m_end, data.data, Block::DATA_SIZE);
		data.size -= Block::DATA_SIZE;
		data.data += Block::DATA_SIZE;
		m_end = newBlock()->begin();
	}
	memcpy(m_end, data.data, data.size);
	m_end = m_end + data.size;

	guard.disarm();
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::write(Reserve reserve)
{
	assert(reserve.size != 0);

	char *new_end = m_end + reserve.size;
	if (TNT_LIKELY(isSameBlock(m_end, new_end))) {
		// new_addr is still in block. just advance.
		m_end = new_end;
		return;
	}

	EndGuard guard(*this);

	// Flipped out-of-block bit, go to the next block.
	reserve.size -= leftInBlock(m_end);

	m_end = newBlock()->begin();
	while (TNT_UNLIKELY(reserve.size >= Block::DATA_SIZE)) {
		reserve.size -= Block::DATA_SIZE;
		m_end = newBlock()->begin();
	}
	m_end = m_end + reserve.size;

	guard.disarm();
}

template <size_t N, class allocator>
template <class T>
void
Buffer<N, allocator>::write(const T& t)
{
	static_assert(sizeof(T) <= Block::DATA_SIZE,
		"Please use struct WData for big objects");
	if constexpr (sizeof(T) == 1) {
		memcpy(m_end, &t, sizeof(T));
		++m_end;
		if (TNT_UNLIKELY(isEndOfBlock(m_end))) {
			// Went out of block, have to go to the next.
			--m_end; // Set back for the case of exception.
			m_end = newBlock()->begin();
		}
	} else {
		char *new_end = m_end + sizeof(T);
		if (TNT_UNLIKELY(!isSameBlock(m_end, new_end))) {
			// Flipped out-of-block bit, go to the next block.
			Block *b = newBlock();
			size_t part1 = leftInBlock(m_end);
			size_t part2 = sizeof(T) - part1;
			char data[sizeof(T)];
			memcpy(data, &t, sizeof(T));
			memcpy(m_end, data, part1);
			m_end = b->begin();
			memcpy(m_end, data + part1, part2);
			m_end += part2;
		} else {
			memcpy(m_end, &t, sizeof(T));
			m_end = new_end;
		}
	}
}

template <size_t N, class allocator>
template <char... C>
void
Buffer<N, allocator>::write(CStr<C...>)
{
	if constexpr (CStr<C...>::size == 0) {
	} else if constexpr (CStr<C...>::size == 1) {
		*m_end = CStr<C...>::data[0];
		++m_end;
		if (TNT_UNLIKELY(isEndOfBlock(m_end))) {
			// Went out of block, have to go to the next.
			--m_end; // Set back for the case of exception.
			m_end = newBlock()->begin();
		}
	} else {
		if (TNT_LIKELY(leftInBlock(m_end) > CStr<C...>::rnd_size)) {
			memcpy(m_end, CStr<C...>::data, CStr<C...>::rnd_size);
			m_end += CStr<C...>::size;
		} else {
			write({CStr<C...>::data, CStr<C...>::size});
		}
	}
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropBack(size_t size)
{
	assert(size != 0);
	assert(!m_blocks.isEmpty());

	Block *block = &m_blocks.last();
	size_t left_in_block = m_end - block->begin();

	/* Do not delete the block if it is empty after drop. */
	while (TNT_UNLIKELY(size > left_in_block)) {
		assert(!m_blocks.isEmpty());
		delBlock(block);
		block = &m_blocks.last();

		/*
		 * Make sure there's no iterators pointing to the block
		 * to be dropped.
		 */
		assert(m_iterators.isEmpty() ||
		       m_iterators.last().getBlock() != block);

		m_end = block->end();
		size -= left_in_block;
		left_in_block = Block::DATA_SIZE;
	}
	m_end -= size;
#ifndef NDEBUG
	assert(m_end >= block->begin());
	/*
	 * Two sanity checks: there's no iterators pointing to the dropped
	 * part of block; end of buffer does not cross start of buffer.
	 */
	if (!m_iterators.isEmpty() && m_iterators.last().getBlock() == block)
		assert(m_iterators.last().m_position <= m_end);
	if (&m_blocks.first() == block)
		assert(m_end >= m_begin);
#endif
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropFront(size_t size)
{
	assert(size != 0);
	assert(!m_blocks.isEmpty());

	Block *block = &m_blocks.first();
	size_t left_in_block = block->end() - m_begin;

	while (TNT_UNLIKELY(size >= left_in_block)) {
#ifndef NDEBUG
		/*
		 * Make sure block to be dropped does not have pointing to it
		 * iterators.
		 */
		if (! m_iterators.empty()) {
			assert(m_iterators.first().getBlock() != block);
		}
#endif
		delBlock(block);
		block = &m_blocks.first();
		m_begin = block->begin();
		size -= left_in_block;
		left_in_block = Block::DATA_SIZE;
	}
	m_begin += size;
#ifndef NDEBUG
	assert(m_begin < block->end());
	if (!m_iterators.isEmpty() && m_iterators.last().getBlock() == block)
		assert(m_iterators.last().m_position >= m_begin);
	if (&m_blocks.last() == block)
		assert(m_begin <= m_end);
#endif
}

template <size_t N, class allocator>
char *
Buffer<N, allocator>::getEnd() noexcept
{
	return m_end;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::setEnd(char *ptr) noexcept
{
	while (TNT_UNLIKELY(!isSameBlock(m_end, ptr))) {
		while (!m_iterators.empty() &&
		       isSameBlock(m_end, m_iterators.last().m_position))
			m_iterators.last().remove();
		delBlock(&m_blocks.last());
		m_end = m_blocks.last().begin();
	}
	m_end = ptr;
}

template <size_t N, class allocator>
Buffer<N, allocator>::EndGuard::~EndGuard() noexcept
{
	if (TNT_UNLIKELY(!m_disarmed))
		m_buffer.setEnd(m_end);
}


template <size_t N, class allocator>
void
Buffer<N, allocator>::insert(const iterator &itr, size_t size)
{
	//TODO: rewrite without iterators.
	/* Remember last block before extending the buffer. */
	Block *src_block = &m_blocks.last();
	char *src_block_end = m_end;
	write(Reserve{size});
	Block *dst_block = &m_blocks.last();
	char *src = nullptr;
	char *dst = nullptr;
	/*
	 * Special treatment for starting block: we should not go over
	 * iterator's position.
	 * TODO: remove this awful define (but it least it works).
	 */
#define src_block_begin ((src_block == itr.getBlock()) ? itr.m_position : src_block->begin())
	/* Firstly move data in blocks. */
	size_t left_in_dst_block = m_end - dst_block->begin();
	size_t left_in_src_block = src_block_end - src_block_begin;
	if (left_in_dst_block > left_in_src_block) {
		src = src_block_begin;
		dst = m_end - left_in_src_block;
	} else {
		src = src_block_end - left_in_dst_block;
		dst = dst_block->begin();
	}
	assert(dst <= m_end);
	size_t copy_chunk_sz = std::min(left_in_src_block, left_in_dst_block);
	for (;;) {
		/*
		 * During copying data in block may split into two parts
		 * which get in different blocks. So let's use two-step
		 * memcpy of data in source block.
		 */
		assert(dst_block->id > itr.getBlock()->id || dst >= itr.m_position);
		std::memmove(dst, src, copy_chunk_sz);
		if (left_in_dst_block > left_in_src_block) {
			left_in_dst_block -= copy_chunk_sz;
			if (src_block == itr.getBlock())
				break;
			src_block = &src_block->prev();
			src = src_block->end() - left_in_dst_block;
			left_in_src_block = src_block->end() - src_block_begin;
			dst = dst_block->begin();
			copy_chunk_sz = left_in_dst_block;
		} else {
			/* left_in_src_block >= left_in_dst_block */
			left_in_src_block -= copy_chunk_sz;
			dst_block = &dst_block->prev();
			dst = dst_block->end() - left_in_src_block;
			left_in_dst_block = Block::DATA_SIZE;
			src = src_block->begin();
			copy_chunk_sz = left_in_src_block;
		}
	}
	/* Adjust position for copy in the first block. */
	assert(src_block == itr.getBlock());
	assert(itr.m_position >= src);
	/* Select all iterators from end until the same position. */
	for (iterator *tmp = &m_iterators.last();
	     *tmp != itr; tmp = &tmp->prev())
		tmp->moveForward(size);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::release(const iterator &itr, size_t size)
{
	//TODO: rewrite without iterators.
	Block *src_block = itr.getBlock();
	Block *dst_block = itr.getBlock();
	char *src = itr.m_position;
	char *dst = itr.m_position;
	/* Locate the block to start copying with. */
	size_t step = size;
	assert(src_block->end() > src);
	while (step >= (size_t)(src_block->end() - src)) {
		step -= src_block->end() - src;
		src_block = &src_block->next();
		src = src_block->begin();
	}
	src += step;
	/* Firstly move data in blocks. */
	size_t left_in_dst_block = dst_block->end() - dst;
	size_t left_in_src_block = src_block->end() - src;
	size_t copy_chunk_sz = std::min(left_in_src_block, left_in_dst_block);
	for (;;) {
		std::memmove(dst, src, copy_chunk_sz);
		if (left_in_dst_block > left_in_src_block) {
			left_in_dst_block -= copy_chunk_sz;
			/*
			 * We don't care if in the last iteration we copy a
			 * little bit more in destination block since
			 * this data anyway will be truncated by ::dropBack()
			 * call in the end of function.
			 */
			if (src_block == &m_blocks.last())
				break;
			src_block = &src_block->next();
			src = src_block->begin();
			left_in_src_block = Block::DATA_SIZE;
			dst += copy_chunk_sz;
			copy_chunk_sz = left_in_dst_block;
		} else {
			/* left_in_src_block >= left_in_dst_block */
			left_in_src_block -= copy_chunk_sz;
			dst_block = &dst_block->next();
			dst = dst_block->begin();
			left_in_dst_block = Block::DATA_SIZE;
			src += copy_chunk_sz;
			copy_chunk_sz = left_in_src_block;
		}
	};

	/* Now adjust iterators' positions. */
	/* Select all iterators from end until the same position. */
	for (iterator *tmp = &m_iterators.last();
	     *tmp != itr; tmp = &tmp->prev())
		tmp->moveBackward(size);

	/* Finally drop unused chunk. */
	dropBack(size);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::resize(const iterator &itr, size_t size, size_t new_size)
{
	if (new_size > size)
		insert(itr, new_size - size);
	else
		release(itr, size - new_size);
}

template <size_t N, class allocator>
template <bool LIGHT>
size_t
Buffer<N, allocator>::getIOV(const iterator_common<LIGHT> &itr,
			     struct iovec *vecs, size_t max_size)
{
	return getIOV(itr, end<true>(), vecs, max_size);
}

template <size_t N, class allocator>
template <bool LIGHT1, bool LIGHT2>
size_t
Buffer<N, allocator>::getIOV(const iterator_common<LIGHT1> &start,
			     const iterator_common<LIGHT2> &end,
			     struct iovec *vecs, size_t max_size)
{
	assert(vecs != NULL);
	assert(start < end || start == end);
	Block *block = start.getBlock();
	Block *last_block = end.getBlock();
	char *pos = start.m_position;
	size_t vec_cnt = 0;
	for (; vec_cnt < max_size;) {
		struct iovec *vec = &vecs[vec_cnt];
		++vec_cnt;
		vec->iov_base = pos;
		if (block == last_block) {
			vec->iov_len = (size_t) (end.m_position - pos);
			break;
		}
		vec->iov_len = (size_t) (block->end() - pos);
		block = &block->next();
		pos = block->begin();
	}
	return vec_cnt;
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::set(WData data)
{
	assert(data.size > 0);
	char *pos = m_position;
	size_t left_in_block = N - (uintptr_t) pos % N;
	while (TNT_UNLIKELY(data.size > left_in_block)) {
		std::memcpy(pos, data.data, left_in_block);
		data.size -= left_in_block;
		data.data += left_in_block;
		pos = Block::byPtr(pos)->next().data;
		left_in_block = Block::DATA_SIZE;
	}
	memcpy(pos, data.data, data.size);
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
void
Buffer<N, allocator>::iterator_common<LIGHT>::set(T&& t)
{
	/*
	 * Do not even attempt at copying non-standard classes (such as
	 * containing vtabs).
	 */
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	const char *tc = &reinterpret_cast<const char &>(t);
	if (TNT_LIKELY(has_contiguous(sizeof(T))))
		memcpy(m_position, tc, sizeof(T));
	else
		set({tc, sizeof(T)});
}

template <size_t N, class allocator>
template <bool LIGHT>
template <char... C>
void
Buffer<N, allocator>::iterator_common<LIGHT>::set(CStr<C...>)
{
	if constexpr (CStr<C...>::size == 0) {
	} else if constexpr (CStr<C...>::size == 1) {
		*m_position = CStr<C...>::data[0];
	} else {
		if (TNT_LIKELY(has_contiguous(CStr<C...>::rnd_size))) {
			memcpy(m_position, CStr<C...>::data, CStr<C...>::rnd_size);
		} else {
			set({CStr<C...>::data, CStr<C...>::size});
		}
	}
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::write(WData data)
{
	assert(data.size > 0);
	size_t left_in_block = N - (uintptr_t) m_position % N;
	while (TNT_UNLIKELY(data.size >= left_in_block)) {
		std::memcpy(m_position, data.data, left_in_block);
		data.size -= left_in_block;
		data.data += left_in_block;
		m_position = Block::byPtr(m_position)->next().data;
		left_in_block = Block::DATA_SIZE;
	}
	memcpy(m_position, data.data, data.size);
	m_position += data.size;
	adjustPositionForward();
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
void
Buffer<N, allocator>::iterator_common<LIGHT>::write(T&& t)
{
	/*
	 * Do not even attempt at copying non-standard classes (such as
	 * containing vtabs).
	 */
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	const char *tc = &reinterpret_cast<const char &>(t);
	if (TNT_LIKELY(has_contiguous(sizeof(T)))) {
		memcpy(m_position, tc, sizeof(T));
		m_position += sizeof(T);
	} else {
		write({tc, sizeof(T)});
	}
}

template <size_t N, class allocator>
template <bool LIGHT>
template <char... C>
void
Buffer<N, allocator>::iterator_common<LIGHT>::write(CStr<C...>)
{
	if constexpr (CStr<C...>::size != 0) {
		if (TNT_LIKELY(has_contiguous(CStr<C...>::rnd_size) + 1)) {
			memcpy(m_position, CStr<C...>::data, CStr<C...>::rnd_size);
			m_position += CStr<C...>::size;
		} else {
			write({CStr<C...>::data, CStr<C...>::size});
		}
	}
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::get(RData data)
{
	assert(data.size > 0);
	/*
	 * The same implementation as in ::set() method buf vice versa:
	 * buffer and data sources are swapped.
	 */
	char *pos = m_position;
	size_t left_in_block = N - (uintptr_t) pos % N;
	while (TNT_UNLIKELY(data.size > left_in_block)) {
		memcpy(data.data, pos, left_in_block);
		data.size -= left_in_block;
		data.data += left_in_block;
		pos = Block::byPtr(pos)->next().data;
		left_in_block = Block::DATA_SIZE;
	}
	memcpy(data.data, pos, data.size);
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
void
Buffer<N, allocator>::iterator_common<LIGHT>::get(T& t)
{
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	char *tc = &reinterpret_cast<char &>(t);
	if (TNT_LIKELY(has_contiguous(sizeof(T))))
		memcpy(tc, m_position, sizeof(T));
	else
		get({tc, sizeof(T)});
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
T
Buffer<N, allocator>::iterator_common<LIGHT>::get()
{
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	T t;
	get(t);
	return t;
}

template <size_t N, class allocator>
template <bool LIGHT>
void
Buffer<N, allocator>::iterator_common<LIGHT>::read(RData data)
{
	assert(data.size > 0);
	/*
	 * The same implementation as in ::set() method buf vice versa:
	 * buffer and data sources are swapped.
	 */
	size_t left_in_block = N - (uintptr_t) m_position % N;
	while (TNT_UNLIKELY(data.size >= left_in_block)) {
		memcpy(data.data, m_position, left_in_block);
		data.size -= left_in_block;
		data.data += left_in_block;
		m_position = Block::byPtr(m_position)->next().data;
		left_in_block = Block::DATA_SIZE;
	}
	memcpy(data.data, m_position, data.size);
	m_position += data.size;
	adjustPositionForward();
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
void
Buffer<N, allocator>::iterator_common<LIGHT>::read(T& t)
{
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	char *tc = &reinterpret_cast<char &>(t);
	if (TNT_LIKELY(has_contiguous(sizeof(T) + 1))) {
		memcpy(tc, m_position, sizeof(T));
		m_position += sizeof(T);
		adjustPositionForward();
	} else {
		read({tc, sizeof(T)});
	}
}

template <size_t N, class allocator>
template <bool LIGHT>
template <class T>
T
Buffer<N, allocator>::iterator_common<LIGHT>::read()
{
	static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>,
		      "T is expected to have standard layout");
	T t;
	read(t);
	return t;
}

template <size_t N, class allocator>
template <bool LIGHT>
bool
Buffer<N, allocator>::has(const iterator_common<LIGHT>& itr, size_t size)
{
	const char *pos = itr.m_position;
	uintptr_t itr_addr = (uintptr_t) pos;
	const char *block_end = (const char *)((itr_addr | (N - 1)) + 1);
	const char *bound = isSameBlock(pos, m_end) ? m_end : block_end;
	if (TNT_LIKELY(pos + size <= bound))
		return true;
	else
		return size <= end<true>() - itr;
}

template<size_t N, class allocator>
void
Buffer<N, allocator>::flush()
{
	size_t distance = m_iterators.isEmpty() ?
		end<true>() - begin<true>() : m_iterators.first() - begin<true>();
	if (distance > 0)
		dropFront(distance);
}

template <size_t N, class allocator>
int
Buffer<N, allocator>::debugSelfCheck() const
{
	int res = 0;
	bool first = true;
	size_t prevId;
	for (const Block& block : m_blocks) {
		if (first)
			first = false;
		else if (block.id != prevId + 1)
			res |= 1;
		prevId = block.id;
	}

	for (const iterator& itr : m_iterators) {
		if ((uintptr_t) itr.m_position % N < Block::DATA_OFFSET)
			res |= 4;
	}
	return res;
}

#ifndef NDEBUG
template <size_t N, class allocator>
std::string
dump(Buffer<N, allocator> &buffer)
{
	size_t vec_len = 0;
	size_t IOVEC_MAX = 1024;
	size_t block_cnt = 0;
	struct iovec vec[IOVEC_MAX];
	std::string output;
	for (auto itr = buffer.begin(); itr != buffer.end(); itr += vec_len) {
		size_t vec_cnt = buffer.getIOV(itr, (struct iovec*)&vec, IOVEC_MAX);
		for (size_t i = 0; i < vec_cnt; ++i) {
			output.append("|sz=" + std::to_string(vec[i].iov_len) + "|");
			output.append((const char *) vec[i].iov_base,
				      vec[i].iov_len);
			output.append("|");
			vec_len += vec[i].iov_len;
		}
		block_cnt += vec_cnt;
	}
	output.insert(0, "bcnt=" + std::to_string(block_cnt));
	return output;
}
#endif

} // namespace tnt {
