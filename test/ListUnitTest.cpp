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

#include "../src/Utils/List.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

// Single-linked list.
struct Object : tnt::SingleLink<Object>
{
	int m_Data;

	Object(int id) : m_Data(id)
	{
	}

	Object(int id, Object& near, bool before)
		: tnt::SingleLink<Object>(near, before), m_Data(id)
	{
	}

	Object(int id, tnt::List<Object>& list, bool back = false)
		: tnt::SingleLink<Object>(list, back), m_Data(id)
	{
	}
};

using ObjectList = tnt::List<Object>;

// Multi-linked list.
struct in_red;
struct in_green;
struct in_blue;

struct MultilistObject;

using RedList = tnt::List<MultilistObject, in_red>;
using GreenList = tnt::List<MultilistObject, in_green>;
using BlueList = tnt::List<MultilistObject, in_blue>;

using LinkCommon = tnt::LinkSelector<MultilistObject>;
using InRed = tnt::ListLink<MultilistObject, in_red>;
using InGreen = tnt::ListLink<MultilistObject, in_green>;
using InBlue = tnt::ListLink<MultilistObject, in_blue>;

struct MultilistObject : LinkCommon, InRed, InGreen, InBlue
{
	int m_Data;

	MultilistObject(int id) : m_Data(id)
	{
	}

	MultilistObject(int id, MultilistObject& near, bool in_red_before,
			bool in_green_before, bool in_blue_before)
		: InRed(near, in_red_before),
		  InGreen(near, in_green_before),
		  InBlue(near, in_blue_before),
		  m_Data(id)
	{
	}

	MultilistObject(int id,
			RedList& red_list, bool red_back,
			GreenList& green_list, bool green_back,
			BlueList& blue_list, bool blue_back)
		: InRed(red_list, red_back),
		  InGreen(green_list, green_back),
		  InBlue(blue_list, blue_back),
		  m_Data(id)
	{
	}
};


int rc = 0;

void check(bool exp, const char *funcname, const char *filename, int line)
{
	if (!exp) {
		rc = 1;
		std::cerr << "Check failed in " << funcname << " at "
			  << filename << ":"
			  << line << std::endl;
	}
}

template <class T>
void check(const T& x, const T& y, const char *funcname, const char *filename,
	   int line)
{
	if (x != y) {
		rc = 1;
		std::cerr << "Check failed: " << x << " != " << y << " in "
			  << funcname
			  << " at " << filename << ":" << line << std::endl;
	}
}

void check(const ObjectList& list, std::vector<int> arr,
	   const char *funcname, const char *filename, int line)
{
	bool failed = false;

	if (list.selfCheck() != 0)
		failed = true;

	if (list.isEmpty() != (arr.size() == 0))
		failed = true;

	if (!list.isEmpty()) {
		if (&list.first() != &list.front())
			failed = true;
		if (&list.first() != &list.last(true))
			failed = true;
		if (&list.last() != &list.back())
			failed = true;
		if (&list.last() != &list.first(true))
			failed = true;
	}

	int first = 0 == arr.size() ? 0 : *arr.begin();
	int last = 0;
	auto itr1 = list.begin();
	auto itr2 = arr.begin();
	bool first_met = false;
	bool last_met = false;
	for (; itr1 != list.end() && itr2 != arr.end(); ++itr1, ++itr2) {
		if (itr1->m_Data != *itr2)
			failed = true;
		last = *itr2;
		if (itr1->isFirst() != itr1->isLast(true))
			failed = true;
		if (itr1->isFirst(true) != itr1->isLast())
			failed = true;
		if (!itr1->isFirst()) {
			if (&itr1->prev() != &itr1->next(true))
				failed = true;
		}
		if (!itr1->isLast()) {
			if (&itr1->next() != &itr1->prev(true))
				failed = true;
		}
		if (!first_met) {
			if (!itr1->isFirst())
				failed = true;
			first_met = true;
		} else {
			if (itr1->isFirst())
				failed = true;
		}
		if (!last_met) {
			if (itr1->isLast())
				last_met = true;
		} else {
			failed = true;
		}
	}
	if (first_met && !last_met)
		failed = true;
	if (itr1 != list.end() || itr2 != arr.end())
		failed = true;
	if (!list.isEmpty() && list.front().m_Data != first)
		failed = true;
	if (!list.isEmpty() && list.first().m_Data != first)
		failed = true;
	if (!list.isEmpty() && list.back().m_Data != last)
		failed = true;
	if (!list.isEmpty() && list.last().m_Data != last)
		failed = true;

	if (!list.empty() && !failed) {
		const Object *ptr = &list.first();
		itr2 = arr.begin();

		while (true) {
			if (*itr2 != ptr->m_Data)
				failed = true;
			++itr2;
			if (itr2 == arr.end())
				break;
			ptr = &ptr->next();
		}

		ptr = &list.last();
		itr2 = arr.end();
		--itr2;

		while (true) {
			if (*itr2 != ptr->m_Data)
				failed = true;
			if (itr2 == arr.begin())
				break;
			--itr2;
			ptr = &ptr->prev();
		}
	}

	if (arr.begin() != arr.end()) {
		itr1 = list.end();
		--itr1;
		itr2 = arr.end();
		--itr2;
		for (; itr1 != list.begin() &&
		       itr2 != arr.begin(); --itr1, --itr2) {
			if (itr1->m_Data != *itr2)
				failed = true;
		}
		if (itr1 != list.begin() || itr2 != arr.begin())
			failed = true;
	}

	if (failed) {
		std::cerr << "Check failed: list {";
		bool first = true;
		for (const Object& sObj : list) {
			if (!first)
				std::cerr << ", " << sObj.m_Data;
			else
				std::cerr << sObj.m_Data;
			first = false;
		}
		std::cerr << "} expected to be {";
		first = true;
		for (int sVal : arr) {
			if (!first)
				std::cerr << ", " << sVal;
			else
				std::cerr << sVal;
			first = false;
		}

		std::cerr << "} in " << funcname << " at " << filename << ":"
			  << line
			  << std::endl;
		rc = 1;
	}
}

#define CHECK(...) check(__VA_ARGS__, __func__, __FILE__, __LINE__)

struct Announcer
{
	const char *m_Func;

	explicit Announcer(const char *aFunc) : m_Func(aFunc)
	{
		std::cout << "Test " << m_Func << " started" << std::endl;
	}

	~Announcer()
	{
		std::cout << "Test " << m_Func << " finished" << std::endl;
	}
};

#define ANNOUNCE() Announcer sAnn(__func__)

void test_simple()
{
	ANNOUNCE();

	ObjectList list;
	CHECK(list, {});

	Object a(1);
	Object b(2);
	Object c(3);
	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// LIFO
	CHECK(a.isDetached());
	list.insert(a);
	CHECK(!a.isDetached());
	CHECK(list, { 1 });

	CHECK(b.isDetached());
	list.insert(b);
	CHECK(!b.isDetached());
	CHECK(list, { 2, 1 });

	CHECK(c.isDetached());
	list.insert(c);
	CHECK(!c.isDetached());
	CHECK(list, { 3, 2, 1 });

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(!a.isDetached() && !b.isDetached() && !c.isDetached());

	c.remove();
	CHECK(c.isDetached());
	CHECK(list, { 2, 1 });

	b.remove();
	CHECK(b.isDetached());
	CHECK(list, { 1 });

	a.remove();
	CHECK(a.isDetached());
	CHECK(list, {});

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// FIFO
	CHECK(a.isDetached());
	list.insert(a);
	CHECK(!a.isDetached());
	CHECK(list, { 1 });

	CHECK(b.isDetached());
	list.insert(b);
	CHECK(!b.isDetached());
	CHECK(list, { 2, 1 });

	CHECK(c.isDetached());
	list.insert(c);
	CHECK(!c.isDetached());
	CHECK(list, { 3, 2, 1 });

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(!a.isDetached() && !b.isDetached() && !c.isDetached());

	a.unlink();
	CHECK(a.isDetached());
	CHECK(list, { 3, 2 });

	b.unlink();
	CHECK(b.isDetached());
	CHECK(list, { 3 });

	c.unlink();
	CHECK(c.isDetached());
	CHECK(list, {});

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// LIFO Back
	CHECK(a.isDetached());
	list.insert(a, true);
	CHECK(!a.isDetached());
	CHECK(list, { 1 });

	CHECK(b.isDetached());
	list.insert(b, true);
	CHECK(!b.isDetached());
	CHECK(list, { 1, 2 });

	CHECK(c.isDetached());
	list.insert(c, true);
	CHECK(!c.isDetached());
	CHECK(list, { 1, 2, 3 });

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(!a.isDetached() && !b.isDetached() &&!c.isDetached());

	c.remove();
	CHECK(c.isDetached());
	CHECK(list, { 1, 2 });

	b.remove();
	CHECK(b.isDetached());
	CHECK(list, { 1 });

	a.remove();
	CHECK(a.isDetached());
	CHECK(list, {});

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// FIFO back
	CHECK(a.isDetached());
	list.insert(a, true);
	CHECK(!a.isDetached());
	CHECK(list, { 1 });

	CHECK(b.isDetached());
	list.insert(b, true);
	CHECK(!b.isDetached());
	CHECK(list, { 1, 2 });

	CHECK(c.isDetached());
	list.insert(c, true);
	CHECK(!c.isDetached());
	CHECK(list, { 1, 2, 3 });

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(!a.isDetached() && !b.isDetached() && !c.isDetached());

	CHECK(!a.isDetached());
	a.remove();
	CHECK(a.isDetached());
	CHECK(list, { 2, 3 });

	b.remove();
	CHECK(b.isDetached());
	CHECK(list, { 3 });

	c.remove();
	CHECK(c.isDetached());
	CHECK(list, {});

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// Middle
	CHECK(a.isDetached());
	list.insert(a);
	CHECK(!a.isDetached());
	CHECK(list, { 1 });

	CHECK(c.isDetached());
	a.insert(c);
	CHECK(!c.isDetached());
	CHECK(list, { 1, 3 });

	CHECK(b.isDetached());
	c.insert(b, true);
	CHECK(!b.isDetached());
	CHECK(list, { 1, 2, 3 });

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(!a.isDetached() && !b.isDetached() && !c.isDetached());

	b.remove();
	CHECK(b.isDetached());
	CHECK(list, { 1, 3 });

	a.remove();
	CHECK(a.isDetached());
	CHECK(list, { 3 });

	c.remove();
	CHECK(c.isDetached());
	CHECK(list, {});

	CHECK(a.selfCheck() == 0 && b.selfCheck() == 0 && c.selfCheck() == 0);
	CHECK(a.isDetached() && b.isDetached() && c.isDetached());

	// Clear
	list.insert(a);
	list.insert(b, true);
	list.insert(c, true);
	CHECK(list, { 1, 2, 3 });

	list.clear();
	CHECK(list, { });

	// Insert list
	{
		ObjectList list1, list2;
		Object o1(1), o2(2), o3(3), o4(4);
		list1.insert(o1);
		list1.insert(o2, true);
		list2.insert(o3);
		list2.insert(o4, true);
		list1.insert(list2);
		CHECK(list1, { 3, 4, 1, 2 });
		CHECK(list2, { });
	}
	{
		ObjectList list1, list2;
		Object o1(1), o2(2), o3(3), o4(4);
		list1.insert(o1);
		list1.insert(o2, true);
		list2.insert(o3);
		list2.insert(o4, true);
		list1.insert(list2, true);
		CHECK(list1, { 1, 2, 3, 4 });
		CHECK(list2, { });
	}
	for (size_t i = 0; i < 8; i++) {
		ObjectList list1, list2;
		Object o1(1), o2(2), o3(3), o4(4);
		std::vector<int> res;
		if (i & 1) {
			list1.insert(o1);
			list1.insert(o2, true);
			res.insert(res.end(), 1);
			res.insert(res.end(), 2);
		}
		if (i & 2) {
			list2.insert(o3);
			list2.insert(o4, true);
			res.insert(i >= 4 ? res.end() : res.begin(), 3);
			res.insert(i >= 4 ? res.end() : res.begin() + 1, 4);
		}
		list1.insert(list2, i >= 4);
		CHECK(list1, res);
		CHECK(list2, { });
	}

	// Swap lists
	for (size_t i = 0; i < 9; i++) {
		ObjectList list1, list2;
		Object o1(1), o2(2), o3(3), o4(4);
		std::vector<int> res1, res2;
		if (i % 3 > 0) {
			list1.insert(o1);
			res1.push_back(1);
		}
		if (i % 3 > 1) {
			list1.insert(o2, true);
			res1.push_back(2);
		}
		if (i / 3 > 0) {
			list2.insert(o3);
			res2.push_back(3);
		}
		if (i / 3 > 1) {
			list2.insert(o4, true);
			res2.push_back(4);
		}
		CHECK(list1, res1);
		CHECK(list2, res2);
		list1.swap(list2);
		CHECK(list1, res2);
		CHECK(list2, res1);
	}
}


// Template of single-linked list.
template <class T>
struct TplObject : tnt::SingleLink<TplObject<T>>
{
	USING_LIST_LINK_METHODS(tnt::SingleLink<TplObject<T>>);
	int m_Data;

	TplObject(int id) : m_Data(id)
	{
	}

	TplObject(int id, TplObject<T>& near, bool before)
		: tnt::SingleLink<TplObject<T>>(near, before), m_Data(id)
	{
	}

	TplObject(int id, tnt::List<TplObject<T>>& list, bool back = false)
		: tnt::SingleLink<TplObject<T>>(list, back), m_Data(id)
	{
	}

	void test()
	{
		CHECK(isDetached());
		CHECK(selfCheck() == 0);
		tnt::List<TplObject<T>> list;
		list.insert(*this);
		CHECK(!isDetached());
		CHECK(isFirst());
		CHECK(isLast());
		CHECK(selfCheck() == 0);

		TplObject b(2);
		TplObject c(3);
		insert(b);
		insert(c, true);
		CHECK(!isFirst());
		CHECK(!isLast());
		CHECK(selfCheck() == 0);
		CHECK(&prev() == &c);
		CHECK(&next() == &b);

		remove();
		unlink();
		CHECK(isDetached());
		CHECK(selfCheck() == 0);
		CHECK(&b.prev() == &c);
		CHECK(&c.next() == &b);
	}
};

template <class T>
void test_simple_tpl()
{
	ANNOUNCE();

	TplObject<T> a(1);
	a.test();
};

void test_iterations()
{
	ANNOUNCE();

	ObjectList list;
	Object obj[5] = {0, 1, 2, 3, 4};
	for (size_t i = 0; i < 5; i++)
		list.insert(obj[i], true);
	CHECK(list.selfCheck(), 0);
	CHECK(list, { 0, 1, 2, 3, 4 });

	bool sDel = false;
	for (auto sItr = list.begin(); sItr != list.end();) {
		if (sDel)
			(*sItr++).remove();
		else
			++sItr;
		sDel = !sDel;
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, { 0, 2, 4 });

	for (auto sItr = list.begin(); sItr != list.end();) {
		if (sDel)
			(*sItr++).remove();
		else
			++sItr;
		sDel = !sDel;
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, { 2 });

	for (auto sItr = list.begin(); sItr != list.end();) {
		if (sDel)
			(*sItr++).remove();
		else
			++sItr;
		sDel = !sDel;
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, { 2 });

	for (auto sItr = list.begin(); sItr != list.end();) {
		if (sDel)
			(*sItr++).remove();
		else
			++sItr;
		sDel = !sDel;
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, {});
}

void test_ctors()
{
	ANNOUNCE();

	ObjectList list;
	CHECK(list, { });

	// Link ctor/dtor/assign.
	for (bool back: {false, true}) {
		{
			Object a(1, list, back);
			CHECK(list, { 1 });
		}
		CHECK(list, { });
	}

	{
		Object a(1, list);
		Object b(2, list);
		Object c(3, list, true);
		CHECK(list, { 2, 1, 3 });
	}
	CHECK(list, { });

	{
		Object a(10, list);
		Object b(20, list, true);
		Object c(30, list, true);
		CHECK(list, { 10, 20, 30 });

		Object al( 9, a, true);
		Object ar(11, a, false);
		Object bl(19, b, true);
		Object br(21, b, false);
		Object cl(29, c, true);
		Object cr(31, c, false);
		CHECK(list, { 9, 10, 11, 19, 20, 21, 29, 30, 31 });
	}
	CHECK(list, { });

	CHECK(list, { });
	{
		Object a(1);
		{
			Object b(2, list);
			list.insert(a);
			CHECK(list, { 1, 2 });
		}
		CHECK(list, { 1 });
	}

	{
		Object a(0);
		{
			Object b(1, list);
			a.~Object();
			Object *tmp = new(&a) Object(std::move(b));
			assert(tmp == &a); (void)tmp;
			b.m_Data = 0;
		}
		CHECK(list, { 1 });
	}

	{
		Object a(0);
		{
			Object b(1, list);
			a = std::move(b);
			b.m_Data = 0;
		}
		CHECK(list, { 1 });
	}

	{
		Object a(1, list);
		CHECK(list, { 1 });
		Object b(std::move(a));
		b.m_Data = 2;
		CHECK(list, { 1, 2 } );
	}

	{
		Object a(1);
		Object b(std::move(a));
		b.m_Data = 2;
		CHECK(!a.isDetached());
		CHECK(!b.isDetached());
		CHECK(a.selfCheck() == 0);
		CHECK(b.selfCheck() == 0);
	}

	{
		Object a(1);
		Object b(std::move(a));
		b.m_Data = 2;
		list.insert(a);
		CHECK(list, { 1 } );
	}

	{
		Object a(1, list);
		CHECK(list, { 1 });
		Object b(2);
		b = std::move(a);
		b.m_Data = 2;
		CHECK(list, { 1, 2 } );
	}

	{
		Object a(1, list);
		CHECK(list, { 1 });
		Object b(2, list, true);
		CHECK(list, { 1, 2 });
		b = std::move(a);
		b.m_Data = 2;
		CHECK(list, { 1, 2 } );
	}

	{
		Object a(1, list);
		CHECK(list, { 1 });
		ObjectList tmp_list;
		Object b(2, tmp_list);
		CHECK(tmp_list, { 2 });
		b = std::move(a);
		b.m_Data = 2;
		CHECK(list, { 1, 2 } );
	}

	// List dtor.
	{
		Object a(1);
		Object b(2);
		Object c(3);

		{
			ObjectList tmp_list;
			tmp_list.insert(c);
			tmp_list.insert(b);
			tmp_list.insert(a);
			CHECK(tmp_list, { 1, 2, 3 });
		}

		CHECK(!a.isDetached());
		CHECK(!b.isDetached());
		CHECK(!c.isDetached());

		b.remove();
		CHECK(!a.isDetached());
		CHECK(b.isDetached());
		CHECK(!c.isDetached());

		a.remove();
		CHECK(a.isDetached());
		CHECK(b.isDetached());
		CHECK(c.isDetached());
	}

	// List move ctor.
	{
		ObjectList tmp_list(std::move(list));
		CHECK(list, {  } );
		CHECK(tmp_list, {  } );
		Object a(1, list);
		Object b(2, tmp_list);
		CHECK(list, { 1 } );
		CHECK(tmp_list, { 2 } );
	}

	{
		Object a(1, list);
		Object b(2, list, true);
		ObjectList tmp_list(std::move(list));
		CHECK(list, {  } );
		CHECK(tmp_list, { 1, 2 } );
		Object c(3, tmp_list, true);
		Object d(4, list);
		CHECK(list, { 4 } );
		CHECK(tmp_list, { 1, 2, 3 } );
	}

	// List move assign.
	{
		ObjectList tmp_list;
		tmp_list = std::move(list);
		CHECK(list, {  } );
		CHECK(tmp_list, {  } );
		Object e(5, list, true);
		Object f(6, tmp_list, true);
		CHECK(list, { 5 } );
		CHECK(tmp_list, { 6 } );
	}

	{
		ObjectList tmp_list;
		Object a(1, list);
		Object b(2, list, true);
		tmp_list = std::move(list);
		CHECK(list, {  } );
		CHECK(tmp_list, { 1, 2 } );
		Object e(5, list, true);
		Object f(6, tmp_list, true);
		CHECK(list, { 5 } );
		CHECK(tmp_list, { 1, 2, 6 } );
	}

	{
		ObjectList tmp_list;
		Object a(1, tmp_list);
		Object b(2, tmp_list, true);
		tmp_list = std::move(list);
		CHECK(list, { 1, 2 } );
		CHECK(tmp_list, { } );
		Object e(5, list, true);
		Object f(6, tmp_list, true);
		CHECK(list, { 1, 2, 5 } );
		CHECK(tmp_list, { 6 } );
	}

	{
		ObjectList tmp_list;
		Object a(1, list);
		Object b(2, list, true);
		Object c(3, tmp_list);
		Object d(4, tmp_list, true);
		tmp_list = std::move(list);
		CHECK(list, { 3, 4 } );
		CHECK(tmp_list, { 1, 2 } );
		Object e(5, list, true);
		Object f(6, tmp_list, true);
		CHECK(list, { 3, 4, 5 } );
		CHECK(tmp_list, { 1, 2, 6 } );
	}

	CHECK(list, { } );
}

void test_ctors_more()
{
	ANNOUNCE();

	ObjectList list;
	CHECK(list.selfCheck(), 0);
	CHECK(list, {});
	{
		Object obj(1);
		list.insert(obj);
		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, {});

	{
		Object obj(1);
		list.insert(obj);
		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });
		{
			Object obj2(2);
			list.insert(obj2, true);
			CHECK(list.selfCheck(), 0);
			CHECK(list, { 1, 2 });
		}
		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, {});

	{
		Object obj(1);
		CHECK(obj.isDetached());

		Object obj3(std::move(obj));
		CHECK(!obj.isDetached());
		CHECK(!obj3.isDetached());
		obj.remove();
		CHECK(obj.isDetached());
		CHECK(obj3.isDetached());
	}

	{
		Object obj(1);
		list.insert(obj);
		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });

		{
			Object obj2(2, obj, false);
			CHECK(list.selfCheck(), 0);
			CHECK(list, { 1, 2 });
		}
		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });

		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });

		{
			Object obj2(std::move(obj));
			obj2.m_Data = 2;
			CHECK(list.selfCheck(), 0);
			CHECK(list, { 1, 2 });
		}

		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });

		{
			Object obj2(2);
			obj2 = std::move(obj);
			obj2.m_Data = 2;
			CHECK(list.selfCheck(), 0);
			CHECK(list, { 1, 2 });
		}

		CHECK(list.selfCheck(), 0);
		CHECK(list, { 1 });
	}
	CHECK(list, { });
	{
		Object obj(1);
		{
			ObjectList list2;
			list.insert(obj);
			CHECK(list.selfCheck(), 0);
			CHECK(list, { 1 });

			{
				Object obj2(2);
				list2.insert(obj2);
				CHECK(list2.selfCheck(), 0);
				CHECK(list2, { 2 });

				obj = std::move(obj2);
				obj.m_Data = 1;

				CHECK(list.selfCheck(), 0);
				CHECK(list, { });
				CHECK(list2.selfCheck(), 0);
				CHECK(list2, { 2, 1 });
				CHECK(!obj.isDetached());
			}

			CHECK(list2, { 1 });
			CHECK(!obj.isDetached());
		}
		CHECK(list.selfCheck(), 0);
		CHECK(list, {});
		CHECK(obj.isDetached());
	}
	CHECK(list.selfCheck(), 0);
	CHECK(list, {});
}

void test_massive()
{
	ObjectList list;
	std::vector<int> sReference;
	const size_t ITER_COUNT = 100000;
	const size_t SIZE_LIM = 10;
	for (size_t i = 0; i < ITER_COUNT; i++) {
		bool sAdd = true;
		if (sReference.size() == SIZE_LIM)
			sAdd = false;
		else if (!sReference.empty() && (rand() & 1) == 0)
			sAdd = false;
		bool sBegin = (rand() & 1) == 0;
		if (sAdd) {
			int sValue = rand();
			Object *sObj = new Object(sValue);
			if (sBegin) {
				list.insert(*sObj);
				sReference.insert(sReference.begin(), sValue);
			} else {
				list.insert(*sObj, true);
				sReference.insert(sReference.end(), sValue);
			}
		} else {
			if (sBegin) {
				delete &list.front();
				sReference.erase(sReference.begin());
			} else {
				delete &list.back();
				sReference.pop_back();
			}
		}

		CHECK(list, sReference);
		if (rc != 0)
			break;
	}
}

constexpr size_t MULTITEST_ROUNDS = 16;
constexpr size_t MULTITEST_ITERATIONS = 1024;

void test_multilinik_round()
{
	int next_id = 0;
	std::vector<MultilistObject> objects;
	RedList red_list;
	GreenList green_list;
	BlueList blue_list;
	using expected_t = std::vector<int>;
	expected_t expected_red, expected_green, expected_blue;
	auto expected_insert = [](expected_t &exp, bool to_back, int id) {
		exp.insert(to_back ? exp.end() : exp.begin(), id);
	};
	auto expected_insert_near = [](expected_t &exp, bool before, int near_id, int id) {
		auto itr = std::find(exp.begin(), exp.end(), near_id);
		if (itr == exp.end())
			return;
		if (!before)
			++itr;
		exp.insert(itr, id);
	};
	auto expected_remove = [](expected_t &exp, int id) {
		exp.erase(std::remove(exp.begin(), exp.end(), id), exp.end());
	};

	auto fail = []() { assert(false); return false; };
	auto verify = [&](const auto &list, const expected_t& exp) {
		if (list.selfCheck() != 0)
			return fail();

		using List_t = std::decay_t<decltype(list)>;
		using Tag_t = typename List_t::ListTag_t;

		if (!exp.empty()) {
			if (list.first().m_Data != exp.front())
				return fail();
			if (list.last().m_Data != exp.back())
				return fail();
		}

		auto itr1 = list.begin();
		auto itr2 = exp.begin();
		for (size_t i = 0;
		     itr1 != list.end() && itr2 != exp.end();
		     ++itr1, ++itr2, ++i) {
			if (itr1->m_Data != *itr2)
				return fail();
			const MultilistObject &obj = *itr1;
			if (obj.isFirst<Tag_t>() != (i == 0))
				return fail();
			if (obj.isLast<Tag_t>() != (i == (exp.size() - 1)))
				return fail();
			if (!obj.isFirst<Tag_t>())
				if (obj.prev<Tag_t>().m_Data != exp[i - 1])
					return fail();
			if (!obj.isLast<Tag_t>())
				if (obj.next<Tag_t>().m_Data != exp[i + 1])
					return fail();
		}
		if (itr1 != list.end() || itr2 != exp.end())
			return fail();

		return true;
	};
	auto verify_all = [&]() {
		return verify(red_list, expected_red) &&
		       verify(green_list, expected_green) &&
		       verify(blue_list, expected_blue);
	};

	auto create = [&]() {
		objects.emplace_back(next_id++);
	};

	auto create_add_to_list = [&]() {
		int id = next_id++;
		int r = rand();
		bool to_red_back = 0 != (r & 1);
		bool to_green_back = 0 != (r & 2);
		bool to_blue_back = 0 != (r & 4);
		objects.emplace_back(id,
				     red_list, to_red_back,
				     green_list, to_green_back,
				     blue_list, to_blue_back);

		expected_insert(expected_red, to_red_back, id);
		expected_insert(expected_green, to_green_back, id);
		expected_insert(expected_blue, to_blue_back, id);
	};

	auto create_add_near = [&]() {
		if (objects.empty())
			return;
		MultilistObject& near = objects[rand() % objects.size()];
		int id = next_id++;
		int r = rand();
		bool red_before = 0 != (r & 1);
		bool green_before = 0 != (r & 2);
		bool blue_before = 0 != (r & 4);

		expected_insert_near(expected_red, red_before, near.m_Data, id);
		expected_insert_near(expected_green, green_before, near.m_Data, id);
		expected_insert_near(expected_blue, blue_before, near.m_Data, id);

		objects.emplace_back(id, near, red_before, green_before, blue_before);
	};

	auto insert_to_list = [&]() {
		if (objects.empty())
			return;
		MultilistObject& obj = objects[rand() % objects.size()];
		int color = rand() % 3;
		bool to_back = 0 == (rand() & 1);
		if (color == 0) {
			red_list.insert(obj, to_back);
			expected_remove(expected_red, obj.m_Data);
			expected_insert(expected_red, to_back, obj.m_Data);
		} else if (color == 1) {
			green_list.insert(obj, to_back);
			expected_remove(expected_green, obj.m_Data);
			expected_insert(expected_green, to_back, obj.m_Data);
		} else {
			blue_list.insert(obj, to_back);
			expected_remove(expected_blue, obj.m_Data);
			expected_insert(expected_blue, to_back, obj.m_Data);
		}
	};

	auto insert_to_near = [&]() {
		if (objects.size() < 2)
			return;
		int r1 = rand() % objects.size(), r2;
		do r2 = rand() % objects.size(); while (r1 == r2);
		MultilistObject& obj = objects[r1];
		MultilistObject& near = objects[r2];
		int color = rand() % 3;
		bool before = 0 == (rand() & 1);
		if (color == 0) {
			near.insert<in_red>(obj, before);
			expected_remove(expected_red, obj.m_Data);
			expected_insert_near(expected_red, before,
					     near.m_Data, obj.m_Data);
		} else if (color == 1) {
			near.insert<in_green>(obj, before);
			expected_remove(expected_green, obj.m_Data);
			expected_insert_near(expected_green, before,
					     near.m_Data, obj.m_Data);
		} else {
			near.insert<in_blue>(obj, before);
			expected_remove(expected_blue, obj.m_Data);
			expected_insert_near(expected_blue, before,
					     near.m_Data, obj.m_Data);
		}
	};

	auto remove = [&]() {
		if (objects.empty())
			return;
		MultilistObject& victim = objects[rand() % objects.size()];
		int color = rand() % 3;
		if (color == 0) {
			victim.remove<in_red>();
			expected_remove(expected_red, victim.m_Data);
		} else if (color == 1) {
			victim.remove<in_green>();
			expected_remove(expected_green, victim.m_Data);
		} else {
			victim.remove<in_blue>();
			expected_remove(expected_blue, victim.m_Data);
		}
	};

	for (size_t i = 0; rc == 0 && i < MULTITEST_ITERATIONS; i++) {
		int r = rand() % 6;
		switch (r) {
		case 0:
			create();
			break;
		case 1:
			create_add_to_list();
			break;
		case 2:
			create_add_near();
			break;
		case 3:
			insert_to_list();
			break;
		case 4:
			insert_to_near();
			break;
		case 5:
			remove();
			break;
		default:
			assert(false);
			__builtin_unreachable();
		};
		if (!verify_all()) {
			std::cout << "Check failed" << std::endl;
			rc = -1;
			return;
		}
	}
//	std::cout << objects.size()
//		  << " " << expected_red.size()
//		  << " " << expected_green.size()
//		  << " " << expected_blue.size() << std::endl;
}

void test_multilinik()
{
	for (size_t i = 0; rc == 0 && i < MULTITEST_ROUNDS; i++)
		test_multilinik_round();
}


int main()
{
	test_simple();
	test_simple_tpl<int>();
	test_iterations();
	test_ctors();
	test_ctors_more();
	test_massive();
	test_multilinik();

	if (rc == 0)
		std::cout << "Success" << std::endl;
	else
		std::cout << "Failed" << std::endl;
	return rc;
}
