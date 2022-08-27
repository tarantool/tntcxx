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

#include <iostream>
#include <string_view>

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
	void log(std::ostream& strm, LogLevel log_lvl,
		 const char *file, int line, ARGS&& ...args)
	{
		if (!isLogPossible(log_lvl))
			return;
		time_t rawTime;
		time(&rawTime);
		struct tm *timeInfo = localtime(&rawTime);
		char timeString[10];
		strftime(timeString, sizeof(timeString), "%H:%M:%S", timeInfo);
		// The line below is commented for compatibility with previous
		// version. I'm not sure it was bug or feature, but the time,
		// filename and line was not printed.
		(void)file; (void)line;
		//strm << timeString << " " << file << ':' << line << ' ';
		strm << log_lvl << ": ";
		(strm << ... << std::forward<ARGS>(args)) << '\n';
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
	gLogger.log(level == ERROR ? std::cerr : std::cout,
		    level, file, line, std::forward<ARGS>(args)...);
}

#define LOG_DEBUG(...) log(DEBUG, __FILE__, __LINE__,  __VA_ARGS__)
#define LOG_INFO(...) log(INFO, __FILE__, __LINE__,  __VA_ARGS__)
#define LOG_WARNING(...) log(WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log(ERROR, __FILE__, __LINE__, __VA_ARGS__)
