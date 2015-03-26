/*
 *  Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#include "Semaphore.h"
#include "Logger.h"

SFB::Semaphore::Semaphore()
	: mSemaphore(nullptr)
{
	mSemaphore = dispatch_semaphore_create(0);

	if(nullptr == mSemaphore) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Semaphore", "dispatch_semaphore_create failed");
		throw std::runtime_error("Unable to create the semaphore");
	}
}

SFB::Semaphore::~Semaphore()
{
	dispatch_release(mSemaphore), mSemaphore = nullptr;
}

bool SFB::Semaphore::Signal()
{
	return dispatch_semaphore_signal(mSemaphore);
}

bool SFB::Semaphore::Wait()
{
	return TimedWait(DISPATCH_TIME_FOREVER);
}

bool SFB::Semaphore::TimedWait(dispatch_time_t duration)
{
	return dispatch_semaphore_wait(mSemaphore, duration);
}
