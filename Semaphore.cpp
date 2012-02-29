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

#include <mach/mach.h>
#include <stdexcept>

#include "Semaphore.h"
#include "Logger.h"

Semaphore::Semaphore()
{
	kern_return_t result = semaphore_create(mach_task_self(), &mSemaphore, SYNC_POLICY_FIFO, 0);

	if(KERN_SUCCESS != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Semaphore", "semaphore_create failed: " << mach_error_string(result));
		throw std::runtime_error("Unable to create the semaphore");
	}
}

Semaphore::~Semaphore()
{
	kern_return_t result = semaphore_destroy(mach_task_self(), mSemaphore);

	if(KERN_SUCCESS != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Semaphore", "semaphore_destroy failed: " << mach_error_string(result));
}

bool Semaphore::Signal()
{
	kern_return_t result = semaphore_signal(mSemaphore);

	if(KERN_SUCCESS != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Semaphore", "Couldn't signal the semaphore: " << mach_error_string(result));
		return false;
	}

	return true;
}

bool Semaphore::SignalAll()
{
	kern_return_t result = semaphore_signal_all(mSemaphore);

	if(KERN_SUCCESS != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Semaphore", "Couldn't signal the semaphore: " << mach_error_string(result));
		return false;
	}

	return true;
}

bool Semaphore::Wait()
{
	kern_return_t result = semaphore_wait(mSemaphore);

	if(KERN_SUCCESS != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Semaphore", "Semaphore couldn't wait: " << mach_error_string(result));
		return false;
	}

	return true;
}

bool Semaphore::TimedWait(mach_timespec_t duration)
{
	kern_return_t result = semaphore_timedwait(mSemaphore, duration);

	if(KERN_SUCCESS != result && KERN_OPERATION_TIMED_OUT != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Semaphore", "Semaphore couldn't timedwait: " << mach_error_string(result));
		return false;
	}

	return true;
}
