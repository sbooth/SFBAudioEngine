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

#include "Guard.h"
#include "Logger.h"

Guard::Guard()
{
	int success = pthread_cond_init(&mCondition, nullptr);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Guard", "pthread_cond_init failed: " << strerror(success));
		throw std::runtime_error("Unable to initialize the condition variable");
	}
}

Guard::~Guard()
{
	int success = pthread_cond_destroy(&mCondition);

	if(0 != success)
		LOGGER_ERR("org.sbooth.AudioEngine.Guard", "pthread_cond_destroy failed: " << strerror(success));
}

void Guard::Wait()
{
	pthread_t currentThread = pthread_self();
	if(!pthread_equal(mOwner, currentThread))
		throw std::runtime_error("A thread is attempting to wait on a condition variable without a locked mutex");

	mOwner = 0;

	int success = pthread_cond_wait(&mCondition, &mMutex);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Guard", "pthread_cond_wait failed: " << strerror(success));
		throw std::runtime_error("Unable to wait for the condition variable");
	}

	mOwner = currentThread;
}

bool Guard::WaitUntil(struct timespec absoluteTime)
{
	pthread_t currentThread = pthread_self();
	if(!pthread_equal(mOwner, currentThread))
		throw std::runtime_error("A thread is attempting to wait on a condition variable without a locked mutex");

	mOwner = 0;

	int success = pthread_cond_timedwait(&mCondition, &mMutex, &absoluteTime);

	if(ETIMEDOUT != success && 0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Guard", "pthread_cond_timedwait failed: " << strerror(success));
		throw std::runtime_error("Unable to wait for the condition variable");
	}

	mOwner = currentThread;

	return (ETIMEDOUT == success);
}

void Guard::Signal()
{
	int success = pthread_cond_signal(&mCondition);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Guard", "pthread_cond_signal failed: " << strerror(success));
		throw std::runtime_error("Unable to signal the condition variable");
	}
}

void Guard::Broadcast()
{
	int success = pthread_cond_broadcast(&mCondition);

	if(0 != success) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Guard", "pthread_cond_broadcast failed: " << strerror(success));
		throw std::runtime_error("Unable to broadcast the condition variable");
	}
}

#pragma mark Locker

Guard::Locker::Locker(Guard& guard)
	: mGuard(guard)
{
	mReleaseLock = mGuard.Lock();
}

Guard::Locker::~Locker()
{
	if(mReleaseLock)
		mGuard.Unlock();
}
