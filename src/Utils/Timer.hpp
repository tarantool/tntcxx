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
#include <chrono>
#include "Logger.hpp"

class Timer {
public:
	Timer(int timeout);
	void start();
	bool isExpired() const;
	int elapsed() const;
private:
	std::chrono::milliseconds m_Timeout;
	std::chrono::time_point<std::chrono::steady_clock> m_Start;
};

Timer::Timer(int timeout) : m_Timeout{std::chrono::milliseconds{timeout}}
{
}

void
Timer::start()
{
	m_Start = std::chrono::steady_clock::now();
}

bool
Timer::isExpired() const
{
	if (m_Timeout == std::chrono::milliseconds{0})
		return false;
	std::chrono::time_point<std::chrono::steady_clock> end =
		std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed =
		std::chrono::duration_cast<std::chrono::milliseconds>(end - m_Start);
	//LOG_DEBUG("timeout %d elapsed %d", m_Timeout.count(), elapsed.count());
	return elapsed >= m_Timeout;
}

int
Timer::elapsed() const
{
	if (m_Timeout == std::chrono::milliseconds{0})
		return 0;
	std::chrono::time_point<std::chrono::steady_clock> end =
		std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_Start).count();
}
