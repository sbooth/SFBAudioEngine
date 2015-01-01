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

#pragma once

#include <mach/task.h>

/*! @file Semaphore.h @brief A mach \c semaphore_t wrapper */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief A wrapper around a mach semaphore */
	class Semaphore
	{
	public:
		/*!
		 * @brief Create a new \c Semaphore
		 * @throws std::runtime_error
		 */
		Semaphore();

		/*! @brief Destroy this \c Semaphore */
		~Semaphore();

		/*! @cond */

		/*! @internal This class is non-copyable */
		Semaphore(const Semaphore& rhs) = delete;

		/*! @internal This class is non-assignable */
		Semaphore& operator=(const Semaphore& rhs) = delete;

		/*! @endcond */

		/*!
		 * @brief Signal the \c Semaphore to wake a blocked thread
		 * @return \c true if successful, \c false otherwise
		 */
		bool Signal();

		/*!
		 * @brief Signal the \c Semaphore to wake all blocked threads
		 * @return \c true if successful, \c false otherwise
		 */
		bool SignalAll();

		/*!
		 * @brief Block the calling thread until the \c Semaphore is signaled
		 * @return \c true if successful, \c false otherwise
		 */
		bool Wait();

		/*!
		 * @brief Block the calling thread until the \c Semaphore is signaled
		 * @param duration The maximum duration to block
		 * @return \c true if successful, \c false otherwise
		 */
		bool TimedWait(mach_timespec_t duration);

	private:
		semaphore_t mSemaphore;		/*!< The mach semahore */
	};

}
