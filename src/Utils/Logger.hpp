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
#include <cstdarg>
#include <stdio.h>
#include <time.h>

#include <string_view>

enum LogLevel {
	DEBUG = 0,
	WARNING = 1,
	ERROR = 2
};

static inline const char*
logLevelToStr(LogLevel lvl)
{
	switch (lvl) {
		case DEBUG   : return "DEBUG";
		case WARNING : return "WARNING";
		case ERROR   : return "ERROR";
	}
	assert(0 && "Unknown log level");
	return "Unknown log level";
}

class Logger {
public:
	Logger(LogLevel lvl) : m_LogLvl(lvl) {};

	void log(FILE *stream, LogLevel log_lvl, const std::string_view &msg,
		 const char *file, int line, ...)
	{
		if (!isLogPossible(log_lvl))
			return;
		time_t rawTime;
		struct tm *timeInfo = nullptr;
		time(&rawTime);
		timeInfo = localtime(&rawTime);
		char timeString[10];
		strftime(timeString, sizeof(timeString), "%H:%M:%S", timeInfo);
		/* Format: time file line msg*/
		sprintf(Logger::format_msg, "%s %s %d", timeString, file, line);

		va_list args;
		va_start (args, line);
		vsprintf(Logger::format_msg, std::string(msg).c_str(), args);
		va_end(args);

		fprintf(stream, "%s: %s\n", logLevelToStr(log_lvl), Logger::format_msg);
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
	static constexpr size_t MAX_MSG_LEN = 1024;
	inline static char format_msg[MAX_MSG_LEN];
	LogLevel m_LogLvl;
};

#ifndef NDEBUG
inline Logger gLogger(DEBUG);
#else
inline Logger gLogger(ERROR);
#endif

#define LOG_DEBUG(format, ...) gLogger.log(stdout, DEBUG, format, __FILE__, __LINE__,  ##__VA_ARGS__)
#define LOG_WARNING(format, ...) gLogger.log(stdout, WARNING, format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) gLogger.log(stderr, ERROR, format, __FILE__, __LINE__, ##__VA_ARGS__)
