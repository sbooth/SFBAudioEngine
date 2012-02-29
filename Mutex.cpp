/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include <errno.h>

#include "Mutex.h"
#include "Logger.h"

Mutex::Mutex()
	: mOwner(0)
{
	int success = pthread_mutex_init(&mMutex, nullptr);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Mutex", "pthread_mutex_init failed: " << strerror(success));
		throw std::runtime_error("Unable to initialize the mutex");
	}
}

Mutex::~Mutex()
{
	int success = pthread_mutex_destroy(&mMutex);

	if(0 != success)
		LOGGER_ERR("org.sbooth.AudioEngine.Mutex", "pthread_mutex_destroy failed: " << strerror(success));
}

bool Mutex::Lock()
{
	pthread_t currentThread = pthread_self();
	if(pthread_equal(mOwner, currentThread))
		return false;

	int success = pthread_mutex_lock(&mMutex);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Mutex", "pthread_mutex_lock failed: " << strerror(success));
		throw std::runtime_error("Unable to lock the mutex");
	}

	mOwner = currentThread;

	return true;
}

void Mutex::Unlock()
{
	pthread_t currentThread = pthread_self();
	if(pthread_equal(mOwner, currentThread)) {
		int success = pthread_mutex_unlock(&mMutex);

		if(0 != success) {
			LOGGER_CRIT("org.sbooth.AudioEngine.Mutex", "pthread_mutex_unlock failed: " << strerror(success));
			throw std::runtime_error("Unable to unlock the mutex");
		}

		mOwner = 0;
	}
	else
		LOGGER_INFO("org.sbooth.AudioEngine.Mutex", "A thread is attempting to unlock a mutex it doesn't own");
}

bool Mutex::TryLock()
{
	bool acquiredLock;
	return TryLock(acquiredLock);
}

bool Mutex::TryLock(bool& acquiredLock)
{
	acquiredLock = false;

	pthread_t currentThread = pthread_self();
	if(pthread_equal(mOwner, currentThread))
		return true;

	int success = pthread_mutex_trylock(&mMutex);
	
	// The mutex is already locked by another thread
	if(EBUSY == success)
		return false;
	// Something bad happened
	else if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Mutex", "pthread_mutex_trylock failed: " << strerror(success));
		throw std::runtime_error("Unable to lock the mutex");
	}

	acquiredLock = true;
	mOwner = currentThread;

	return true;
}

#pragma mark Locker

Mutex::Locker::Locker(Mutex& mutex)
	: mMutex(mutex)
{
	mReleaseLock = mMutex.Lock();
}

Mutex::Locker::~Locker()
{
	if(mReleaseLock)
		mMutex.Unlock();
}

#pragma mark Tryer

Mutex::Tryer::Tryer(Mutex& mutex)
	: mMutex(mutex)
{
	mLocked = mMutex.TryLock(mReleaseLock);
}

Mutex::Tryer::~Tryer()
{
	if(mReleaseLock)
		mMutex.Unlock();
}
