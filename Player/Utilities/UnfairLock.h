/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <os/lock.h>

/*! @file UnfairLock.h @brief A scoped wrapper around @c os_unfair_lock */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief A wrapper around @c os_unfair_lock implementing C++ Lockable*/
	class UnfairLock
	{
	public:
		// ========================================
		/*! @name Creation and Destruction */
		//@{

		/*! @brief Create a new \c UnfairLock */
		inline UnfairLock() : mLock(OS_UNFAIR_LOCK_INIT) {}

		/*! @cond */

		/*! @internal This class is non-copyable */
		UnfairLock(const UnfairLock& rhs) = delete;

		/*! @internal This class is non-assignable */
		UnfairLock& operator=(const UnfairLock& rhs) = delete;

		/*! @endcond */

		//@}


		// ========================================
		/*! @name Lockable */
		//@{

		/*!@brief Lock the lock. */
		inline void lock() noexcept 		{ os_unfair_lock_lock(&mLock); }

		/*!@brief Unlock the lock. */
		inline void unlock() noexcept 		{ os_unfair_lock_unlock(&mLock); }

		/*!
		 * @brief Attempt to lock the lock.
		 * @return \c true if the lock was successfully locked, \c false on error
		 */
		inline bool try_lock() noexcept 	{ return os_unfair_lock_trylock(&mLock); }

		//@}

	private:

		os_unfair_lock		mLock;					/*!< The primitive lock */
	};

}
