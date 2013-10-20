/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include "Mutex.h"

/*! @file Guard.h @brief A \c pthread_mutex_t and \c pthread_cond_t wrapper */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief A wrapper around a pthread mutex and condition variable */
	class Guard : public Mutex
	{
	public:
		/*!
		 * @brief Create a new \c Guard
		 * @throws std::runtime_exception
		 */
		Guard();

		/*! @brief Destroy this \c Guard */
		virtual ~Guard();

		/*! @cond */

		/*! @internal This class is non-copyable */
		Guard(const Guard& rhs) = delete;

		/*! @internal This class is non-assignable */
		Guard& operator=(const Guard& rhs) = delete;

		/*! @endcond */

		/*!
		 * @brief Block the calling thread until the condition variable is signaled
		 * @note The \c Mutex must be locked or an exception will be thrown
		 * @throws std::runtime_exception
		 */
		void Wait();

		/*!
		 * @brief Block the calling thread until the condition variable is signaled
		 * @note The \c Mutex must be locked or an exception will be thrown
		 * @param absoluteTime The latest time to block
		 * @return \c true if the request timed out, \c false otherwise
		 * @throws std::runtime_exception
		 */
		bool WaitUntil(struct timespec absoluteTime);

		/*!
		 * @brief Unblock a thread waiting on the condition variable
		 * @throws std::runtime_exception
		 */
		void Signal();

		/*!
		 * @brief Unblock all threads waiting on the condition variable
		 * @throws std::runtime_exception
		 */
		void Broadcast();

	protected:
		pthread_cond_t mCondition;	/*!< @brief The pthread condition variable */

	public:
		/*! @brief A scope based wrapper around \c Guard::Lock() */
		class Locker
		{
		public:
			/*!
			 * @brief Create a new \c Guard::Locker
			 * On creation this class calls \c Guard::Lock().
			 * On destruction, if the lock was acquired \c Guard::Unlock() is called.
			 * @param guard The \c Guard to lock
			 * @throws std::runtime_exception
			 */
			Locker(Guard& guard);

			/*! @brief Destroy this \c Guard::Locker */
			~Locker();

			/*!
			 * @brief Block the calling thread until the condition variable is signaled
			 * @throws std::runtime_exception
			 */
			inline void Wait()										{ mGuard.Wait(); }

			/*!
			 * @brief Block the calling thread until the condition variable is signaled
			 * @param absoluteTime The latest time to block
			 * @return \c true if the request timed out, \c false otherwise
			 * @throws std::runtime_exception
			 */
			inline bool WaitUntil(struct timespec absoluteTime)		{ return mGuard.WaitUntil(absoluteTime); }

			/*!
			 * @brief Unblock a thread waiting on the condition variable
			 * @throws std::runtime_exception
			 */
			inline void Signal()									{ mGuard.Signal(); }

			/*!
			 * @brief Unblock all threads waiting on the condition variable
			 * @throws std::runtime_exception
			 */
			inline void Broadcast()									{ mGuard.Broadcast(); }

		private:
			Guard& mGuard;		/*!< The associated \c Guard */
			bool mReleaseLock;	/*!< Whether the destructor should call \c Guard::Unlock() */
		};
	};

}