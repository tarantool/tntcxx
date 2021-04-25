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

namespace tnt {

/**
 * Ring is a simple headless double-linked list, in which every node holds
 * pointers to previous and next nodes.
 * There's a boundary case when a ring consist of on element (itself), it is
 * called as monoring.
 * A ring node may also be uninitialized (must be tracked by user). Operations
 * with such a node has undefined behavior unless opposite is directly stated.
 * The ring is designed to be a base class for other data structures.
 * Its methods have a prefix in order not to interfere with public names.
 */

struct Ring {
	// Make and uninitialized ring node.
	Ring() noexcept { trash(); };

	// Makes a monoring.
	explicit Ring(int) noexcept : m_Neigh{this, this} {}

	// Add a constructing node to existing ring, after (if back == false)
	// or before (if back == true) given node.
	Ring(Ring &a, bool back) noexcept { a.rgAdd(this, back); }

	// Copy/move ctor/assign are deleted. The ctor above should be used.
	Ring(const Ring &) noexcept = delete;
	Ring& operator=(const Ring &) noexcept = delete;
	Ring(Ring &&) noexcept = delete;
	Ring& operator=(Ring &&) noexcept = delete;

	// Note that destructor in empty. If a node is in ring (and not mono)
	// you have to remove the node manually.
	~Ring() noexcept = default;

	// Get the next (if back == false) or previous (if back == true)
	// neighbor of the ring node.
	Ring *rgNeigh(bool forward = true) noexcept;
	const Ring *rgNeigh(bool forward = true) const noexcept;

	// Makes a monoring. The node is allowed to be uninitialized here.
	void rgInit() noexcept;

	// Add a ring node `a` to the ring after (if back == false) or
	// before (if back == true) this node.
	// The given node may be uninitialized.
	// The given ring may also be the ring itself, if it is a monoring.
	void rgAdd(Ring *a, bool back = false) noexcept;

	// Remove the node from a list.
	// This node becomes uninitialized, unless it's a monoring.
	void rgRemove() noexcept;

	// Add entire ring `a` (given node and all its neighbors) to the ring
	// to the end (if invert == false) or to the beginning (otherwise).
	// The operation is done so that rgSplit(a, invert) reverts it.
	void rgJoin(Ring *a, bool invert = false) noexcept;

	// Leave in this ring all node from this up to `a` (if invert == false)
	// or from `a` up to this node (if invert = true).
	// All other nodes including `a` forms another ring.
	void rgSplit(Ring *a, bool invert = false) noexcept;

	// Swap the node with another.
	void rgSwap(Ring *a) noexcept;

	// Check whether this node is monoring.
	bool rgIsMono() const noexcept;

	// Calculate the size of the ring. O(N).
	size_t rgCalcSize() const noexcept;

	// Debug check. O(N). Return 0 if OK.
	int rgSelfCheck() const noexcept;

private:
	// Pointers to previous and next nodes.
	Ring *m_Neigh[2];

	static void link(Ring *prev, Ring *next, bool invert) noexcept;
	void unlink() noexcept;
	void make_monoring() noexcept;
	void trash() noexcept;
};

inline Ring *Ring::rgNeigh(bool forward) noexcept
{
	return m_Neigh[forward];
}

inline const Ring *Ring::rgNeigh(bool forward) const noexcept
{
	return m_Neigh[forward];
}

inline void Ring::rgInit() noexcept
{
	make_monoring();
}

inline void Ring::rgAdd(Ring *a, bool back) noexcept
{
	assert(a != this || a->rgIsMono());
	link(a, m_Neigh[!back], back);
	link(this, a, back);
}

inline void Ring::rgRemove() noexcept
{
#ifndef NDEBUG
	bool was_mono = rgIsMono();
#endif
	unlink();
#ifndef NDEBUG
	if (!was_mono)
		trash();
#endif
}

inline void Ring::rgJoin(Ring *a, bool invert) noexcept
{
	Ring *node = a->m_Neigh[invert];
	link(m_Neigh[invert], a, invert);
	link(node, this, invert);
}

inline void Ring::rgSplit(Ring *a, bool invert) noexcept
{
	Ring *s = a->m_Neigh[invert];
	link(m_Neigh[invert], a, invert);
	link(s, this, invert);
}

inline void Ring::rgSwap(Ring *a) noexcept
{
	rgJoin(a, false);
	rgSplit(a, true);
}

inline bool Ring::rgIsMono() const noexcept
{
	return this == m_Neigh[0];
}

inline size_t Ring::rgCalcSize() const noexcept
{
	size_t res = 1;
	for (const Ring *r = m_Neigh[0]; r != this; r = r->m_Neigh[0])
		++res;
	return res;
}

inline int Ring::rgSelfCheck() const noexcept
{
	const Ring *sRing = this;
	do {
		if (sRing != sRing->m_Neigh[1]->m_Neigh[0])
			return 1;
		sRing = sRing->m_Neigh[1];
	} while (sRing != this);
	return 0;
}

inline void Ring::link(Ring *prev, Ring *next, bool invert) noexcept
{
	prev->m_Neigh[!invert] = next;
	next->m_Neigh[invert] = prev;
}

inline void Ring::unlink() noexcept
{
	link(m_Neigh[0], m_Neigh[1], false);
}

inline void Ring::make_monoring() noexcept
{
	m_Neigh[0] = m_Neigh[1] = this;
}

inline void Ring::trash() noexcept
{
#ifndef NDEBUG
	m_Neigh[0] = m_Neigh[1] = reinterpret_cast<Ring*>(42);
#endif
}

} // namespace tnt {
