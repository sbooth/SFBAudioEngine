/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <asl.h>
#include <sstream>

/*! @file Logger.h @brief A simplified interface to the Apple System Log (ASL) */

/*!
 * @brief Log a message at the \c logger::emerg level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_EMERG(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::emerg) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::emerg, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::alert level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_ALERT(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::alert) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::alert, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::crit level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_CRIT(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::crit) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::crit, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::err level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_ERR(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::err) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::err, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::warning level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_WARNING(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::warning) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::warning, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::notice level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_NOTICE(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::notice) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::notice, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::info level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_INFO(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::info) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::info, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*!
 * @brief Log a message at the \c logger::debug level
 * @param facility The sender's logging facility, or \c nullptr to use the default
 * @param message The log message
 */
#define LOGGER_DEBUG(facility, message) { \
	if(::SFB::Logger::currentLogLevel >= ::SFB::Logger::debug) { \
		::std::stringstream ss_; ss_ << message; \
		::SFB::Logger::Log(::SFB::Logger::debug, facility, ss_.str().c_str(), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	} \
}

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief The namespace containing all logging functionality */
	namespace Logger {

		/*! @brief The possible logging levels for ASL */
		enum levels {
			emerg		= ASL_LEVEL_EMERG,		/*!< The emergency log level */
			alert		= ASL_LEVEL_ALERT,		/*!< The alert log level */
			crit		= ASL_LEVEL_CRIT,		/*!< The critical log level */
			err			= ASL_LEVEL_ERR,		/*!< The error log level */
			warning		= ASL_LEVEL_WARNING,	/*!< The warning log level */
			notice		= ASL_LEVEL_NOTICE,		/*!< The notice log level */
			info		= ASL_LEVEL_INFO,		/*!< The information log level */
			debug		= ASL_LEVEL_DEBUG,		/*!< The debug log level */
			disabled	= 33,					/*!< Disable logging */
		};

		/*! @brief The log level below which messages are ignored */
		extern int currentLogLevel;

		/*! @brief Get the log level below which messages are ignored */
		inline levels	GetCurrentLevel()				{ return (levels)currentLogLevel; }

		/*! @brief Set the log level below which messages will be ignored */
		inline void		SetCurrentLevel(levels level)	{ currentLogLevel = level; }

		/*!
		 * @brief Log a message
		 * @note If \c level is below \c currentLogLevel nothing is logged.
		 * @param level The log level of the message
		 * @param facility The sender's logging facility, or \c nullptr to use the default
		 * @param message The log message
		 * @param function The name of the calling function or \c nullptr to omit
		 * @param file The name of the file containing \c function or \c nullptr to omit
		 * @param line The line number in \c file or \c -1 to omit
		 */
		void Log(levels level, const char * _Nullable facility, const char * _Nonnull message, const char * _Nullable function = nullptr, const char * _Nullable file = nullptr, int line = -1);


		/*! @name Convenience functions */
		//@{

		/*!
		 * @brief Log a message at the \c #emerg level
		 * @note It is preferable to use LOGGER_EMERG() for efficiency
		 * @param message The message to log
		 */
		inline void Emerg(const char * _Nonnull message)		{ Log(emerg, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #alert level
		 * @note It is preferable to use LOGGER_ALERT() for efficiency
		 * @param message The message to log
		 */
		inline void Alert(const char * _Nonnull message)		{ Log(alert, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #crit level
		 * @note It is preferable to use LOGGER_CRIT() for efficiency
		 * @param message The message to log
		 */
		inline void Crit(const char * _Nonnull message)			{ Log(crit, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #err level
		 * @note It is preferable to use LOGGER_ERR() for efficiency
		 * @param message The message to log
		 */
		inline void Err(const char * _Nonnull message)			{ Log(err, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #warning level
		 * @note It is preferable to use LOGGER_WARN() for efficiency
		 * @param message The message to log
		 */
		inline void Warn(const char * _Nonnull message)			{ Log(warning, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #notice level
		 * @note It is preferable to use LOGGER_NOTICE() for efficiency
		 * @param message The message to log
		 */
		inline void Notice(const char * _Nonnull message)		{ Log(notice, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #info level
		 * @note It is preferable to use LOGGER_INFO() for efficiency
		 * @param message The message to log
		 */
		inline void Info(const char * _Nonnull message)			{ Log(info, nullptr, message); }

		/*!
		 * @brief Log a message at the \c #debug level
		 * @note It is preferable to use LOGGER_DEBUG() for efficiency
		 * @param message The message to log
		 */
		inline void Debug(const char * _Nonnull message)		{ Log(debug, nullptr, message); }

		//@}
	}
}

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>

/*! @cond */

// Useful ostream overloads

std::ostream& operator<<(std::ostream& out, CFStringRef _Nullable s);
std::ostream& operator<<(std::ostream& out, CFNumberRef _Nullable n);
std::ostream& operator<<(std::ostream& out, CFURLRef _Nullable u);
std::ostream& operator<<(std::ostream& out, CFErrorRef _Nullable e);
std::ostream& operator<<(std::ostream& out, CFUUIDRef _Nullable u);
std::ostream& operator<<(std::ostream& out, CFUUIDBytes b);
std::ostream& operator<<(std::ostream& out, const AudioStreamBasicDescription& format);
std::ostream& operator<<(std::ostream& out, const AudioChannelLayout * _Nullable  layout);

// Helpers for common toll-free bridged classes
#ifdef __OBJC__

#include <Foundation/Foundation.h>

std::ostream& operator<<(std::ostream& out, NSString * _Nullable s);
std::ostream& operator<<(std::ostream& out, NSNumber * _Nullable n);
std::ostream& operator<<(std::ostream& out, NSURL * _Nullable u);
std::ostream& operator<<(std::ostream& out, NSError * _Nullable e);

#endif

/*! @endcond */
