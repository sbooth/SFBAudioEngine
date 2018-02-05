/*
 * Copyright (c) 2010 - 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <dispatch/dispatch.h>

/*! @file Semaphore.h @brief A \c dispatch_semaphore_t wrapper */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief A wrapper around a libdispatch semaphore */
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
		 * @return \c true if a thread was woken, \c false otherwise
		 */
		bool Signal();

		/*!
		 * @brief Block the calling thread until the \c Semaphore is signaled
		 * @return \c true if successful, \c false if the timeout occurred
		 */
		bool Wait();

		/*!
		 * @brief Block the calling thread until the \c Semaphore is signaled
		 * @param duration The maximum duration to block
		 * @return \c true if successful, \c false if the timeout occurred
		 */
		bool TimedWait(dispatch_time_t duration);

	private:
		dispatch_semaphore_t mSemaphore; /*!< The libdispatch semaphore */
	};

}
