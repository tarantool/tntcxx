/*
 Copyright 2010-2022 Tarantool AUTHORS: please see AUTHORS file.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 1. Redistributions of source code must retain the above
    copyright notice, this list of conditions and the
    following disclaimer.

 2. Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.
*/
#pragma once

#include <algorithm>
#include <memory>

/**
 * Helper of struct Resource, see below.
 */
template <class T>
struct DefaultResourceDestroyer {
	void destroy(T) noexcept {}
};

/**
 * A simple class that stores and supports value of given type that represents
 * some resource.
 */
template <class T, T Default, class Destroyer = DefaultResourceDestroyer<T>>
struct Resource : public Destroyer {
	using Destroyer::destroy;

	Resource() noexcept = default;
	Resource(T t) noexcept;
	Resource(Destroyer d) noexcept;
	Resource(T t, Destroyer d) noexcept;
	~Resource() noexcept;
	Resource(const Resource &) = delete;
	Resource &operator=(const Resource &) = delete;
	Resource(Resource &&a) noexcept;
	Resource &operator=(Resource &&a) noexcept;
	Resource &operator=(T &&t);

	void close();
	operator T() const noexcept { return value; }

private:
	T value = Default;
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer>::Resource(T t) noexcept : value(t)
{}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer>::Resource(Destroyer d) noexcept : Destroyer(d)
{}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer>::Resource(T t, Destroyer d) noexcept :
	Destroyer(d), value(t)
{}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer>::~Resource() noexcept
{
	close();
}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer>::Resource(Resource &&a) noexcept :
	value(a.value)
{
	a.value = Default;
}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer> &
Resource<T, Default, Destroyer>::operator=(Resource &&a) noexcept
{
	std::swap(value, a.value);
	return *this;
}

template <class T, T Default, class Destroyer>
Resource<T, Default, Destroyer> &
Resource<T, Default, Destroyer>::operator=(T &&t)
{
	if (value != Default)
		destroy(value);
	value = t;
	return *this;
}

template <class T, T Default, class Destroyer>
void Resource<T, Default, Destroyer>::close()
{
	if (value != Default) {
		destroy(value);
		value = Default;
	}
}
