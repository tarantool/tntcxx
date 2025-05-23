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
#include <algorithm>
#include <chrono>

class Timer {
public:
	Timer(int timeout) : m_Timeout{std::chrono::milliseconds{timeout}} {};
	void start()
	{
		m_Start = std::chrono::steady_clock::now();
	}
	bool isExpired() const
	{
		if (m_Timeout == std::chrono::milliseconds{-1})
			return false;
		std::chrono::time_point<std::chrono::steady_clock> end =
			std::chrono::steady_clock::now();
		std::chrono::milliseconds elapsed =
			std::chrono::duration_cast<std::chrono::milliseconds>(end - m_Start);
		return elapsed >= m_Timeout;
	}
	/**
	 * The function to obtain amount of time left. Returns:
	 * 1. `-1` if the initial timeout was `-1`.
	 * 2. `0` if the timer has expired.
	 * 3. Otherwise, amount of milliseconds left is returned.
	 * NB: the function should not be used for expiration check - use `isExpired` instead.
	 */
	int timeLeft() const
	{
		if (m_Timeout == std::chrono::milliseconds{-1})
			return -1;
		std::chrono::time_point<std::chrono::steady_clock> end =
			std::chrono::steady_clock::now();
		int timeLeft = m_Timeout.count() -
			std::chrono::duration_cast<std::chrono::milliseconds>(end - m_Start).count();
		return std::max(0, timeLeft);
	}
private:
	std::chrono::milliseconds m_Timeout;
	std::chrono::time_point<std::chrono::steady_clock> m_Start;
};
