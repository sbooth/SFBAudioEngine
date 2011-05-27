/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include <stdexcept>
#include <log4cxx/logger.h>

#include "Mutex.h"

Mutex::Mutex()
{
	int success = pthread_mutex_init(&mMutex, NULL);

	if(0 != success) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine");
		LOG4CXX_FATAL(logger, "pthread_mutex_init failed: " << strerror(success));

		throw std::runtime_error("Unable to initialize the mutex");
	}
}

Mutex::~Mutex()
{
	int success = pthread_mutex_destroy(&mMutex);

	if(0 != success) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine");
		LOG4CXX_ERROR(logger, "pthread_mutex_destroy failed: " << strerror(success));
	}
}

Mutex::Mutex(const Mutex& /*mutex*/)
{}

Mutex& Mutex::operator=(const Mutex& /*mutex*/)
{
	return *this;
}

bool Mutex::Lock()
{
	int lockResult = pthread_mutex_lock(&mMutex);

	if(0 != lockResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine");
		LOG4CXX_WARN(logger, "pthread_mutex_lock failed: " << strerror(lockResult));
		return false;
	}

	return true;
}

bool Mutex::Unlock()
{
	int lockResult = pthread_mutex_unlock(&mMutex);
	
	if(0 != lockResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine");
		LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(lockResult));
		return false;
	}
	
	return true;
}

bool Mutex::TryLock()
{
	int lockResult = pthread_mutex_trylock(&mMutex);
	
	if(0 != lockResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine");
		LOG4CXX_WARN(logger, "pthread_mutex_trylock failed: " << strerror(lockResult));
		return false;
	}
	
	return true;
}

#pragma mark Locker

Locker::Locker(Mutex& mutex)
	: mMutex(mutex)
{
	mLocked = mMutex.Lock();
}

Locker::~Locker()
{
	if(mLocked)
		mMutex.Unlock();
}
