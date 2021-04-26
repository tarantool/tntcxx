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
#include <cstdint>
#include <iterator>

#include "Ring.hpp"

namespace tnt {

/**
 * List is an intrusive double-linked list data structure.
 * Main properties are: no hidden allocation, no internal loops and even
 * branches (except for debug).
 *
 * Each element holds a list node - a pair or pointers - that are used for
 * linking with neighbors. A user is supposed to inherit a special class -
 * ListLink - that internally holds that list node.
 *
 * This list supports mutlilinking - an element can be inserted into several
 * independent lists. For this purpose, though, the element have to hold
 * several list nodes. This is achieved by introducing a tag - additional
 * template parameter of ListLink. A List user can define several tags,
 * inherit ListLink several times using different tags and define several
 * types of list that tags that would use different ListLink.
 *
 * For the sake of convenience ListLink class (that is actually a storage
 * for link pointers and can be inherited several time using different tags)
 * is separated from LinkSelector class, that should inherited only once and
 * provides list access methods in list element.
 *
 * For a simple case when multilinking is not required, a SingleLink class
 * is introduced that is actually a superposition of ListLink and LinkSelector.
 * This class also provides a bit more convenient access methods.
 *
 * In addition to standard (insert/remove/next/etc) methods a variety of
 * constructors are present. They could be not so convenient as standard
 * methods but could be a bit more effective: it's cheaper to construct an
 * element right in proper place than to construct a detached element and then
 * insert it to list.
 * While copying of list and its elements is prohibited, the moving works fine
 * with both elements and list head.
 * Destructors simply unlink elements or lists, that's usually what is needed.
 *
 * When using LinkSelector (or SingleLink) template inside a class template,
 * base members of selector may become ambiguous, and error can happen:
 * there are no arguments to ‘isDetached’ that depend on a template parameter,..
 * A macro USING_LIST_LINK_METHODS can be used to solve the problem (see its
 * description for details).
 *
 * Singlelink pseudo example:
 *
 * struct Object : tnt::SingleLink<Object> {...};
 * tnt::List<Object> list, list2;
 * Object a;
 * list.insert(a);
 * assert(!a.isDetached());
 * for (Object& object : list) { ... }
 * if (!list.isEmpty())
 * 	list.first().remove();
 * if (!a.isFirst())
 * 	a.prev().remove();
 * if (!a.isLast())
 * 	a.next().remove();
 * list2.insert(list);
 * list.swap(list2);
 * list.clear();
 *
 * Multilink pseudo example:
 *
 * struct in_red;
 * struct in_blue;
 * struct Object : tnt::LinkSelector<Object>,
 * 	tnt::ListLink<Object, in_red>, tnt::ListLink<Object, in_blue> { ... };
 * tnt::List<Object, in_red> red_list, red_list2;
 * tnt::List<Object, in_blue> blue_list;
 * Object a;
 * red_list.insert(a);
 * assert(!a.isDetached<in_red>());
 * for (Object& object : blue_list) { ... }
 * if (!red_list.isEmpty())
 * 	red_list.first().remove<in_red>();
 * if (!a.isFirst<in_red>())
 * 	a.prev<in_red>().remove<in_red>();
 * if (!a.isLast<in_blue>())
 * 	a.next<in_blue>().remove<in_blue>();
 * red_list2.insert(red_list);
 * red_list.swap(red_list2);
 * red_list.clear();
 *
 */

constexpr size_t LIST_ALIGN = 4 * sizeof(void *);
constexpr size_t LINK_ALIGN = 2 * sizeof(void *);

template <class Elem, class Tag = void>
class alignas(LINK_ALIGN) ListLink;

template <class Elem, class Tag = void>
class alignas(LIST_ALIGN) List;

template <class Elem>
class LinkSelector;

/**
 * The list head.
 * @tparam Elem - element of list, a class inherited from ListLink<Elem, Tag>.
 * @tparam Tag - optional (void by default) tag that specifies which of
 *  ListLink bases must be used for element linking.
 */
template <class Elem, class Tag>
class alignas(LIST_ALIGN) List
{
	static_assert(std::is_base_of_v<ListLink<Elem, Tag>, Elem>, "Must be!");
public:
	using ListTag_t = Tag;

	// Create an empty list.
	List() noexcept;

	// There's no way to copy an intrusive list.
	List(const List&) = delete;
	List& operator=(const List&) = delete;

	// Move all elements of another list to this list.
	List(List&& list) noexcept;

	// Swap elements with elements of another list.
	List& operator=(List&& list) noexcept;

	// Detach head of the list. The list becomes headless.
	~List() noexcept;

	// Insert an element to (back ? the end : the beginning) of this list.
	// If the element is in (another) list - it is removed first.
	void insert(Elem& elem, bool back = false) noexcept;

	// Insert all elements to (back ? the end : the beginning) of this list.
	// Given list becomes empty.
	void insert(List& list, bool back = false) noexcept;

	// Swap the contents of two lists.
	void swap(List& list) noexcept;

	// Detach head of the list. The list becomes empty, all elements
	// remain in headless list.
	void clear() noexcept;

	// Check that there's no elements here.
	bool isEmpty() const noexcept;

	// Get the first element in list. The list must not be empty.
	// But if (invert == true) get the last element in list.
	Elem& first(bool invert = false) noexcept;
	const Elem& first(bool invert = false) const noexcept;

	// Get the last element in list. The list must not be empty.
	// But if (invert == true) get the first element in list.
	Elem& last(bool invert = false) noexcept;
	const Elem& last(bool invert = false) const noexcept;

	// STL-like aliases.
	bool empty() const noexcept { return isEmpty(); }
	Elem& front() noexcept { return first(); }
	const Elem& front() const noexcept { return first(); }
	Elem& back() noexcept { return last(); }
	const Elem& back() const noexcept { return last(); }

	// Debug check for error, 0 if success.
	int selfCheck() const noexcept;

private:
	// Common class for both constant and normal iterators, see
	// iterator and const_iterator below.
	template <class IElem, class IRing>
	class iterator_common
		: std::iterator<std::bidirectional_iterator_tag, IElem>
	{
	public:
		IElem& operator*() const noexcept;
		IElem *operator->() const noexcept;
		bool operator==(const iterator_common& aItr) const noexcept;
		bool operator!=(const iterator_common& aItr) const noexcept;
		iterator_common& operator++() noexcept;
		iterator_common operator++(int) noexcept;
		iterator_common& operator--() noexcept;
		iterator_common operator--(int) noexcept;

	private:
		friend class List<Elem, Tag>;
		explicit iterator_common(IRing *aRing) noexcept;
		IRing *m_Ring;
	};

public:
	using iterator = iterator_common<Elem, Ring>;
	using const_iterator = iterator_common<const Elem, const Ring>;

	// Standart iterator getters.
	iterator begin() noexcept;
	iterator end() noexcept;
	const_iterator begin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;

private:
	friend class ListLink<Elem, Tag>;

	void *padding1;
	Ring m_Ring;
	void *padding2;

	static Elem& elem(Ring& ring) noexcept;
	static const Elem& elem(const Ring& ring) noexcept;
};

/**
 * The list link node.
 * @tparam Elem - element of list.
 * @tparam Tag - optional (void by default) tag that specifies which of
 *  ListLinks must be used for element linking.
 */
template <class Elem, class Tag>
class alignas(LINK_ALIGN) ListLink : private Ring {
public:
	// Create a detached link.
	ListLink() noexcept;

	// Add to the same list (back ? before : after) given node.
	// The given node must be in list.
	ListLink(ListLink &link, bool back) noexcept;

	// Add the item to the (back ? end : beginning) of the list.
	explicit ListLink(List<Elem, Tag>& list, bool back = false) noexcept;

	// Copy/assign are deleted. The constructor above or move must be used.
	ListLink(const ListLink &link) = delete;
	ListLink& operator=(const ListLink &link) = delete;

	// Add to the same list after given node.
	ListLink(ListLink &&link) noexcept;

	// Add to the same list after given node.
	ListLink& operator=(ListLink&& link) noexcept;

	// Auto remove from list (if in).
	~ListLink() noexcept;

private:
	friend class List<Elem, Tag>;
	friend class LinkSelector<Elem>;
};

/**
 * Provider of access methods in list element. Must be inherited along with
 * one or more ListLinks.
 * @tparam Elem - element of list.
 */
template <class Elem>
class LinkSelector {
public:

	// Insert an element (back ? before : after) this element.
	// If the element is already in (another) list - it is removed first.
	template <class Tag>
	void insert(Elem& elem, bool back = false) noexcept;

	// Remove this element from list. Has no effect for already detached.
	template <class Tag>
	void remove() noexcept;

	// Alias for remove..
	template <class Tag>
	void unlink() noexcept { remove <Tag>(); }

	// Return true if the element not in a list.
	template <class Tag>
	bool isDetached() const noexcept;

	// Check whether the element is the first in the list. Must be in list.
	// But if (invert == true) checks whether the element is the last.
	template <class Tag>
	bool isFirst(bool invert = false) const noexcept;

	// Check whether the element is the last in the list. Must be in list.
	// But if (invert == true) checks whether the element is the first.
	template <class Tag>
	bool isLast(bool invert = false) const noexcept;

	// Go to the next element. Must be in list, not last.
	// But if (invert == true) behaves list prev().
	template <class Tag>
	Elem& next(bool invert = false) noexcept;

	template <class Tag>
	const Elem& next(bool invert = false) const noexcept;

	// Go to the previous element. Must be in list, not first.
	// But if (invert == true) behaves list next().
	template <class Tag>
	Elem& prev(bool invert = false) noexcept;

	template <class Tag>
	const Elem& prev(bool invert = false) const noexcept;

	// Debug check for error, 0 if success.
	template <class Tag>
	int selfCheck() const noexcept;

private:
	template <class Tag>
	Ring& ring() noexcept;

	template <class Tag>
	const Ring& ring() const noexcept;

	template <class Tag>
	static Elem& elem(Ring& ring) noexcept;

	template <class Tag>
	static const Elem& elem(const Ring& ring) noexcept;
};

/**
 * An alias of a group of ListLink<Elem, void> and LinkSelector<Elem>.
 * Apart from LinkSelector<Elem> provides list access methods without Tag
 * template parameter.
 * @tparam Elem - element of list.
 */
template <class Elem>
class SingleLink : public ListLink<Elem, void>, public LinkSelector<Elem>
{
public:
	// Create detached link.
	SingleLink() = default;

	// Add to the same list (back ? before : after) given node.
	SingleLink(SingleLink& link, bool back) noexcept;

	// Add the item to the (back ? end : beginning) of the list.
	explicit SingleLink(List<Elem>& list, bool back = false) noexcept;

	// Insert an element (back ? before : after) this element.
	// If the element is already in (another) list - it is removed first.
	void insert(Elem& elem, bool back = false) noexcept;

	// Remove this element from list. Has no effect for already detached.
	void remove() noexcept;

	// Alias for remove.
	void unlink() noexcept { remove(); }

	// Return true if the element not in a list.
	bool isDetached() const noexcept;

	// Check whether the element is the first in the list. Must be in list.
	// But if (invert == true) checks whether the element is the last.
	bool isFirst(bool invert = false) const noexcept;

	// Check whether the element is the last in the list. Must be in list.
	// But if (invert == true) checks whether the element is the first.
	bool isLast(bool invert = false) const noexcept;

	// Go to the next element. Must be in list, not last.
	// But if (invert == true) behaves list prev().
	Elem& next(bool invert = false) noexcept;
	const Elem& next(bool invert = false) const noexcept;

	// Go to the previous element. Must be in list, not first.
	// But if (invert == true) behaves list next().
	Elem& prev(bool invert = false) noexcept;
	const Elem& prev(bool invert = false) const noexcept;

	// Debug check for error, 0 if success.
	int selfCheck() const noexcept;

private:
	using Base_t = LinkSelector<Elem>;
};

/**
 * When using LinkSelector (or SingleLink) template inside a class template,
 * base members of selector may become ambiguous. Example:
 *
 * template <class T>
 * struct TplObject : tnt::SingleLink<TplObject<T>> {
 * 	bool check()
 * 	{
 * 		return isDetached(); // error: there are no arguments to ‘isDetached’ that depend on a template parameter,..
 * 		return this->isDetached(); // ok..
 * 	}
 * };
 * TplObject<int> a;
 * assert(a.isDetached()); // ok.
 *
 * In that case you may manually use using-declaration or the macro  below:
 * struct TplObject : tnt::SingleLink<TplObject<T>> {
 * 	using SingleLink<TplObject<T>::isDetached; // declare one method.
 * 	USING_LIST_LINK_METHODS(SingleLink<TplObject<T>>); // for all methods.
 */
#define USING_LIST_LINK_METHODS(BASE) \
	using BASE::insert; \
	using BASE::remove; \
	using BASE::unlink; \
	using BASE::isDetached; \
	using BASE::isFirst; \
	using BASE::isLast; \
	using BASE::next; \
	using BASE::prev; \
	using BASE::selfCheck

/////////////////////////////////////////////////////////////////////
//////////////////////// List Implementation ////////////////////////
/////////////////////////////////////////////////////////////////////

template <class Elem, class Tag>
inline List<Elem, Tag>::List() noexcept : m_Ring(0)
{
	static_assert(sizeof(List) == LIST_ALIGN, "Align?");
	assert((uintptr_t) this % LIST_ALIGN == 0);
	assert((uintptr_t) &m_Ring % LINK_ALIGN == sizeof(void *));
}

template <class Elem, class Tag>
inline List<Elem, Tag>::List(List&& list) noexcept
{
	static_assert(sizeof(List) == LIST_ALIGN, "Align?");
	assert((uintptr_t)this % LIST_ALIGN == 0);
	assert((uintptr_t)&m_Ring % LINK_ALIGN == sizeof(void*));

	list.m_Ring.rgAdd(&m_Ring);
	list.m_Ring.rgRemove();
	list.m_Ring.rgInit();
}

template <class Elem, class Tag>
inline List<Elem, Tag>& List<Elem, Tag>::operator=(List&& list) noexcept
{
	m_Ring.rgSwap(&list.m_Ring);
	return *this;
}

template <class Elem, class Tag>
inline List<Elem, Tag>::~List() noexcept
{
	m_Ring.rgRemove();
}

template <class Elem, class Tag>
inline void List<Elem, Tag>::insert(Elem& elem, bool back) noexcept
{
	Ring& ring = static_cast<ListLink<Elem, Tag>&>(elem);
	ring.rgRemove();
	m_Ring.rgAdd(&ring, back);
}

template <class Elem, class Tag>
inline void List<Elem, Tag>::insert(List& list, bool back) noexcept
{
	m_Ring.rgJoin(&list.m_Ring, !back);
	list.m_Ring.rgRemove();
	list.m_Ring.rgInit();
}

template <class Elem, class Tag>
inline void List<Elem, Tag>::swap(List& list) noexcept
{
	m_Ring.rgSwap(&list.m_Ring);
}

template <class Elem, class Tag>
inline void List<Elem, Tag>::clear() noexcept
{
	m_Ring.rgRemove();
	m_Ring.rgInit();
}

template <class Elem, class Tag>
inline bool List<Elem, Tag>::isEmpty() const noexcept
{
	return m_Ring.rgIsMono();
}

template <class Elem, class Tag>
inline Elem& List<Elem, Tag>::first(bool invert) noexcept
{
	assert(!isEmpty());
	return elem(*m_Ring.rgNeigh(!invert));
}

template <class Elem, class Tag>
inline const Elem& List<Elem, Tag>::first(bool invert) const noexcept
{
	assert(!isEmpty());
	return elem(*m_Ring.rgNeigh(!invert));
}

template <class Elem, class Tag>
inline Elem& List<Elem, Tag>::last(bool invert) noexcept
{
	assert(!isEmpty());
	return elem(*m_Ring.rgNeigh(invert));
}

template <class Elem, class Tag>
inline const Elem& List<Elem, Tag>::last(bool invert) const noexcept
{
	assert(!isEmpty());
	return elem(*m_Ring.rgNeigh(invert));
}

template <class Elem, class Tag>
inline int List<Elem, Tag>::selfCheck() const noexcept
{
	return m_Ring.rgSelfCheck();
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline List<Elem, Tag>::iterator_common<IElem, IRing>::iterator_common(IRing *aRing) noexcept
	: m_Ring(aRing)
{
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline IElem& List<Elem, Tag>::iterator_common<IElem, IRing>::operator*() const noexcept
{
	return elem(*m_Ring);
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline IElem *List<Elem, Tag>::iterator_common<IElem, IRing>::operator->() const noexcept
{
	return &elem(*m_Ring);
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline bool List<Elem, Tag>::iterator_common<IElem, IRing>::operator==(const iterator_common& a) const noexcept
{
	return m_Ring == a.m_Ring;
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline bool List<Elem, Tag>::iterator_common<IElem, IRing>::operator!=(const iterator_common& a) const noexcept
{
	return m_Ring != a.m_Ring;
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline typename List<Elem, Tag>::template iterator_common<IElem, IRing>&
List<Elem, Tag>::iterator_common<IElem, IRing>::operator++() noexcept
{
	m_Ring = m_Ring->rgNeigh(true);
	return *this;
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline typename List<Elem, Tag>::template iterator_common<IElem, IRing>
List<Elem, Tag>::iterator_common<IElem, IRing>::operator++(int) noexcept
{
	iterator_common aTmp = *this;
	m_Ring = m_Ring->rgNeigh(true);
	return aTmp;
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline typename List<Elem, Tag>::template iterator_common<IElem, IRing>&
List<Elem, Tag>::iterator_common<IElem, IRing>::operator--() noexcept
{
	m_Ring = m_Ring->rgNeigh(false);
	return *this;
}

template <class Elem, class Tag>
template <class IElem, class IRing>
inline typename List<Elem, Tag>::template iterator_common<IElem, IRing>
List<Elem, Tag>::iterator_common<IElem, IRing>::operator--(int) noexcept
{
	iterator_common aTmp = *this;
	m_Ring = m_Ring->rgNeigh(false);
	return aTmp;
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::iterator List<Elem, Tag>::begin() noexcept
{
	return iterator(m_Ring.rgNeigh(true));
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::iterator List<Elem, Tag>::end() noexcept
{
	return iterator(&m_Ring);
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::const_iterator List<Elem, Tag>::begin() const noexcept
{
	return const_iterator(m_Ring.rgNeigh(true));
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::const_iterator List<Elem, Tag>::end() const noexcept
{
	return const_iterator(&m_Ring);
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::const_iterator List<Elem, Tag>::cbegin() const noexcept
{
	return const_iterator(m_Ring.rgNeigh(true));
}

template <class Elem, class Tag>
inline typename List<Elem, Tag>::const_iterator List<Elem, Tag>::cend() const noexcept
{
	return const_iterator(&m_Ring);
}

template <class Elem, class Tag>
inline Elem& List<Elem, Tag>::elem(Ring& ring) noexcept
{
	ListLink<Elem, Tag>& link =
		static_cast<ListLink<Elem, Tag>&>(ring);
	return static_cast<Elem&>(link);
}

template <class Elem, class Tag>
inline const Elem& List<Elem, Tag>::elem(const Ring& ring) noexcept
{
	const ListLink<Elem, Tag>& link =
		static_cast<const ListLink<Elem, Tag>&>(ring);
	return static_cast<const Elem&>(link);
}

/////////////////////////////////////////////////////////////////////
////////////////////// ListLink Implementation //////////////////////
/////////////////////////////////////////////////////////////////////

template <class Elem, class Tag>
inline ListLink<Elem, Tag>::ListLink() noexcept : Ring(0)
{
	static_assert(sizeof(ListLink) == LINK_ALIGN, "Broken align?");
	assert((uintptr_t)this % LINK_ALIGN == 0);
}

template <class Elem, class Tag>
inline ListLink<Elem, Tag>::ListLink(ListLink &link, bool back) noexcept
	: Ring(link, back)
{
	static_assert(sizeof(ListLink) == LINK_ALIGN, "Broken align?");
	assert(!link.rgIsMono());
	assert((uintptr_t) this % LINK_ALIGN == 0);
}

template <class Elem, class Tag>
inline ListLink<Elem, Tag>::ListLink(List<Elem, Tag>& list, bool back) noexcept
	: Ring(list.m_Ring, back)
{
	assert((uintptr_t) this % LINK_ALIGN == 0);
}

template <class Elem, class Tag>
inline ListLink<Elem, Tag>::ListLink(ListLink&& link) noexcept : Ring(link, false)
{
	static_assert(sizeof(ListLink) == LINK_ALIGN, "Broken align?");
	assert((uintptr_t) this % LINK_ALIGN == 0);
}

template <class Elem, class Tag>
inline ListLink<Elem, Tag>& ListLink<Elem, Tag>::operator=(ListLink&& link) noexcept
{
	rgRemove();
	link.rgAdd(this);
	return *this;
}

template <class Elem, class Tag>
inline ListLink<Elem, Tag>::~ListLink() noexcept
{
	rgRemove();
}


/////////////////////////////////////////////////////////////////////
//////////////////// LinkSelector Implementation ////////////////////
/////////////////////////////////////////////////////////////////////

template <class Elem>
template <class Tag>
inline void LinkSelector<Elem>::insert(Elem& elem, bool back) noexcept
{
	Ring& add_ring = static_cast<ListLink<Elem, Tag>&>(elem);
	add_ring.rgRemove();
	ring<Tag>().rgAdd(&add_ring, back);
}

template <class Elem>
template <class Tag>
inline void LinkSelector<Elem>::remove() noexcept
{
	ring<Tag>().rgRemove();
	ring<Tag>().rgInit();
}

template <class Elem>
template <class Tag>
inline bool LinkSelector<Elem>::isDetached() const noexcept
{
	return ring<Tag>().rgIsMono();
}

template <class Elem>
template <class Tag>
inline bool LinkSelector<Elem>::isFirst(bool invert) const noexcept
{
	return (uintptr_t)ring<Tag>().rgNeigh(invert) % LINK_ALIGN != 0;
}

template <class Elem>
template <class Tag>
inline bool LinkSelector<Elem>::isLast(bool invert) const noexcept
{
	return (uintptr_t)ring<Tag>().rgNeigh(!invert) % LINK_ALIGN != 0;
}

template <class Elem>
template <class Tag>
inline Elem& LinkSelector<Elem>::next(bool invert) noexcept
{
	assert(!isLast<Tag>(invert));
	return elem<Tag>(*ring<Tag>().rgNeigh(!invert));
}

template <class Elem>
template <class Tag>
inline const Elem& LinkSelector<Elem>::next(bool invert) const noexcept
{
	assert(!isLast<Tag>(invert));
	return elem<Tag>(*ring<Tag>().rgNeigh(!invert));
}

template <class Elem>
template <class Tag>
inline Elem& LinkSelector<Elem>::prev(bool invert) noexcept
{
	assert(!isFirst<Tag>(invert));
	return elem<Tag>(*ring<Tag>().rgNeigh(invert));
}

template <class Elem>
template <class Tag>
inline const Elem& LinkSelector<Elem>::prev(bool invert) const noexcept
{
	assert(!isFirst<Tag>(invert));
	return elem<Tag>(*ring<Tag>().rgNeigh(invert));
}

template <class Elem>
template <class Tag>
inline int LinkSelector<Elem>::selfCheck() const noexcept
{
	return ring<Tag>().rgSelfCheck();
}

template <class Elem>
template <class Tag>
inline Ring& LinkSelector<Elem>::ring() noexcept
{
	Elem& e = static_cast<Elem&>(*this);
	return static_cast<ListLink<Elem, Tag>&>(e);
}

template <class Elem>
template <class Tag>
inline const Ring& LinkSelector<Elem>::ring() const noexcept
{
	const Elem& e = static_cast<const Elem&>(*this);
	return static_cast<const ListLink<Elem, Tag>&>(e);
}

template <class Elem>
template <class Tag>
inline Elem& LinkSelector<Elem>::elem(Ring& ring) noexcept
{
	ListLink<Elem, Tag>& link =
		static_cast<ListLink<Elem, Tag>&>(ring);
	return static_cast<Elem&>(link);
}

template <class Elem>
template <class Tag>
inline const Elem& LinkSelector<Elem>::elem(const Ring& ring) noexcept
{
	const ListLink<Elem, Tag>& link =
		static_cast<const ListLink<Elem, Tag>&>(ring);
	return static_cast<const Elem&>(link);
}

/////////////////////////////////////////////////////////////////////
///////////////////// SingleLink Implementation /////////////////////
/////////////////////////////////////////////////////////////////////

template <class Elem>
inline SingleLink<Elem>::SingleLink(SingleLink& link, bool back) noexcept
	: ListLink<Elem, void>(link, back)
{
}

template <class Elem>
inline SingleLink<Elem>::SingleLink(List<Elem>& list, bool back) noexcept
	: ListLink<Elem, void>(list, back)
{
}

template <class Elem>
inline void SingleLink<Elem>::insert(Elem& elem, bool back) noexcept
{
	Base_t::template insert<void>(elem, back);
}

template <class Elem>
inline void SingleLink<Elem>::remove() noexcept
{
	Base_t::template remove<void>();
}

template <class Elem>
inline bool SingleLink<Elem>::isDetached() const noexcept
{
	return Base_t::template isDetached<void>();
}

template <class Elem>
inline bool SingleLink<Elem>::isFirst(bool invert) const noexcept
{
	return Base_t::template isFirst<void>(invert);
}

template <class Elem>
inline bool SingleLink<Elem>::isLast(bool invert) const noexcept
{
	return Base_t::template isLast<void>(invert);
}

template <class Elem>
inline Elem& SingleLink<Elem>::next(bool invert) noexcept
{
	return Base_t::template next<void>(invert);
}

template <class Elem>
inline const Elem& SingleLink<Elem>::next(bool invert) const noexcept
{
	return Base_t::template next<void>(invert);
}

template <class Elem>
inline Elem& SingleLink<Elem>::prev(bool invert) noexcept
{
	return Base_t::template prev<void>(invert);
}

template <class Elem>
inline const Elem& SingleLink<Elem>::prev(bool invert) const noexcept
{
	return Base_t::template prev<void>(invert);
}

template <class Elem>
inline int SingleLink<Elem>::selfCheck() const noexcept
{
	return Base_t::template selfCheck<void>();
}

} // namespace tnt {
