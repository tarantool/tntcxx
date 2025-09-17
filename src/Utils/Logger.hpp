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

#include <time.h>
#include <unistd.h>

#include <sstream>
#include <string_view>

namespace tnt {

enum LogLevel {
	DEBUG = 0,
	INFO = 1,
	WARNING = 2,
	ERROR = 3
};

static inline const char*
logLevelToStr(LogLevel lvl)
{
	switch (lvl) {
		case DEBUG   : return "DEBUG";
		case INFO    : return "INFO ";
		case WARNING : return "WARN ";
		case ERROR   : return "ERROR";
		default      : return "UNDEF";
	}
	assert(0 && "Unknown log level");
	return "Unknown log level";
}

inline std::ostream& operator<<(std::ostream& strm, LogLevel lvl)
{
	return strm << logLevelToStr(lvl);
}

class Logger {
public:
	Logger(LogLevel lvl) : m_LogLvl(lvl) {};

	template <class... ARGS>
	void log(int fd, LogLevel log_lvl, const char *file, int line, ARGS &&...args)
	{
		if (!isLogPossible(log_lvl))
			return;
		/* File and line were never printed (by mistake, I guess). */
		(void)file; (void)line;
		/*
		 * Standard streams (`cout` and `cerr`) are thread-safe
		 * according to C++11 or more modern standards, but it turns
		 * out that some compilers do not stick to this contract.
		 *
		 * Let's use `stringstream` to build a string and then write
		 * it manually with `write` since it is guaranteed to be
		 * thread-safe. Yes, that's slower because of unnnecessary
		 * allocations and copies, but the log is used generally for
		 * debug or logging exceptional cases (errors) anyway, so
		 * that's not a big deal.
		 *
		 * Related: https://github.com/llvm/llvm-project/issues/51851
		 */
		std::stringstream strm;
		strm << log_lvl << ": ";
		(strm << ... << std::forward<ARGS>(args)) << '\n';
		/*
		 * std::move is required to eliminate copying from
		 * std::stringstream if complied with C++20 or higher.
		 */
		std::string str = std::move(strm).str();
		ssize_t rc = write(fd, std::data(str), std::size(str));
		(void)rc;
	}
	void setLogLevel(LogLevel lvl)
	{
		m_LogLvl = lvl;
	}
private:
	bool isLogPossible(LogLevel lvl) const
	{
		return lvl >= m_LogLvl;
	};
	LogLevel m_LogLvl;
};

#ifndef NDEBUG
inline Logger gLogger(DEBUG);
#else
inline Logger gLogger(ERROR);
#endif

template <class... ARGS>
void
log(LogLevel level, const char *file, int line, ARGS&& ...args)
{
	int fd = level == ERROR ? STDERR_FILENO : STDOUT_FILENO;
	gLogger.log(fd, level, file, line, std::forward<ARGS>(args)...);
}

} /* namespace tnt */

#define TNT_LOG_DEBUG(...) tnt::log(tnt::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define TNT_LOG_INFO(...) tnt::log(tnt::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define TNT_LOG_WARNING(...) tnt::log(tnt::WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define TNT_LOG_ERROR(...) tnt::log(tnt::ERROR, __FILE__, __LINE__, __VA_ARGS__)
