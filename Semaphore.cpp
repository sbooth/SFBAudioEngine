/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
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
