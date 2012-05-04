/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <asl.h>
#include <sstream>

// A simplified interface to ASL

// 9 times out of 10 these macros should be used for logging because they are the most efficient
#define LOGGER_EMERG(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::emerg) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::emerg, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_ALERT(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::alert) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::alert, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_CRIT(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::crit) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::crit, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_ERR(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::err) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::err, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_WARNING(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::warning) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::warning, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_NOTICE(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::notice) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::notice, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_INFO(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::info) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::info, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

#define LOGGER_DEBUG(facility, message) { \
	if(::logger::currentLogLevel >= ::logger::debug) { \
		::std::stringstream ss_; ss_ << message; \
		::logger::Log(::logger::debug, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

namespace logger {

	// The possible logging levels for ASL
	enum levels {
		emerg		= ASL_LEVEL_EMERG,
		alert		= ASL_LEVEL_ALERT,
		crit		= ASL_LEVEL_CRIT,
		err			= ASL_LEVEL_ERR,
		warning		= ASL_LEVEL_WARNING,
		notice		= ASL_LEVEL_NOTICE,
		info		= ASL_LEVEL_INFO,
		debug		= ASL_LEVEL_DEBUG,
		disabled	= 33,
	};

	// The current log level
	extern int currentLogLevel;

	// Utilities to get/set the current level
	inline levels	GetCurrentLevel()				{ return static_cast<levels>(currentLogLevel); }
	inline void		SetCurrentLevel(levels level)	{ currentLogLevel = level; }

	// The meat & potatoes
	void Log(levels level, const char *facility, const char *message, const char *function = nullptr, const char *file = nullptr, int line = -1);

	// Convenience functions
	inline void Emerg(const char *message)			{ Log(emerg, nullptr, message); }
	inline void Alert(const char *message)			{ Log(alert, nullptr, message); }
	inline void Crit(const char *message)			{ Log(crit, nullptr, message); }
	inline void Err(const char *message)			{ Log(err, nullptr, message); }
	inline void Warn(const char *message)			{ Log(warning, nullptr, message); }
	inline void Notice(const char *message)			{ Log(notice, nullptr, message); }
	inline void Info(const char *message)			{ Log(info, nullptr, message); }
	inline void Debug(const char *message)			{ Log(debug, nullptr, message); }
};

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>

// Useful ostream overloads
std::ostream& operator<<(std::ostream& out, CFStringRef s);
std::ostream& operator<<(std::ostream& out, CFNumberRef n);
std::ostream& operator<<(std::ostream& out, CFURLRef u);
std::ostream& operator<<(std::ostream& out, CFErrorRef e);
std::ostream& operator<<(std::ostream& out, CFUUIDRef u);
std::ostream& operator<<(std::ostream& out, CFUUIDBytes b);
std::ostream& operator<<(std::ostream& out, const AudioStreamBasicDescription& format);
std::ostream& operator<<(std::ostream& out, const AudioChannelLayout *layout);

// Helpers for common toll-free bridged classes
#ifdef __OBJC__

#include <Foundation/Foundation.h>

std::ostream& operator<<(std::ostream& out, NSString *s);
std::ostream& operator<<(std::ostream& out, NSNumber *n);
std::ostream& operator<<(std::ostream& out, NSURL *u);
std::ostream& operator<<(std::ostream& out, NSError *e);

#endif
