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

#pragma once

#include <pthread.h>

// ========================================
// A wrapper around a pthread mutex
// ========================================
class Mutex
{
public:
	Mutex();
	virtual ~Mutex();

	Mutex(const Mutex& rhs) = delete;
	Mutex& operator=(const Mutex& rhs) = delete;

	// Lock() and Unlock() return true if the operation was successful, false otherwise
	// TryLock() returns true if the lock is held by the current thread, false otherwise
	// All three may throw std::runtime_exception if something bad happens

	bool Lock();
	void Unlock();

	bool TryLock();
	bool TryLock(bool& acquiredLock);

	inline bool Owned() const { return pthread_equal(mOwner, pthread_self()); }

protected:
	pthread_mutex_t mMutex;
	pthread_t mOwner;

public:

	// ========================================
	// Scope-based helpers for Mutex
	// ========================================

	// Uses Mutex::Lock()
	class Locker
	{
	public:
		Locker(Mutex& mutex);
		~Locker();

	private:
		Mutex& mMutex;
		bool mReleaseLock;
	};

	// Uses Mutex::TryLock()
	class Tryer
	{
	public:
		Tryer(Mutex& mutex);
		~Tryer();

		// Returns true if mutex is owned and locked, false otherwise
		inline operator bool() const { return mLocked; }

	private:
		Mutex& mMutex;
		bool mLocked;
		bool mReleaseLock;
	};
};
