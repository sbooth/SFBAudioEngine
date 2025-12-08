//
// Copyright (c) 2025 Stephen F. Booth
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <atomic>
#import <cassert>
#import <cstdint>

namespace SFB {

#ifndef __cpp_lib_atomic_wait
#error "SharedMutex requires C++20 or later for std::atomic::wait and notify functions."
#endif

/// A non-recursive shared mutex implemented using atomic operations.
/// No preference is given to writers over readers.
class __attribute__((capability("mutex"))) __attribute__((shared_capability("mutex"))) SharedMutex {
public:
	SharedMutex() noexcept = default;

	SharedMutex(const SharedMutex&) = delete;
	SharedMutex& operator=(const SharedMutex&) = delete;

	~SharedMutex() noexcept = default;

	/// Acquires shared ownership of the mutex, blocking if the mutex is not available.
	void lock_shared() noexcept __attribute__((acquire_shared_capability()))
	{
		int32_t previous_state;
		for(;;) {
			// Check the current state
			previous_state = state_.load(std::memory_order_relaxed);
			// A writer is active
			if(previous_state < 0) {
				// Block the caller until notified by the writer
				state_.wait(previous_state, std::memory_order_acquire);
				// Recheck state after wake
				continue;
			}
			// Fast path: try to increment the reader count
			if(state_.compare_exchange_weak(previous_state, previous_state + 1, std::memory_order_acquire, std::memory_order_relaxed))
				return;
			// CAS failure means another thread changed the state; loop again to recheck
		}
	}

	/// Tries to acquire shared ownership of the mutex, returning true if the mutex was acquired.
	bool try_lock_shared() noexcept __attribute__((try_acquire_shared_capability(true)))
	{
		// Read the current state
		auto previous_state = state_.load(std::memory_order_relaxed);
		// Fail if a writer is active
		if(previous_state < 0)
			return false;
		// Try to increment the reader count; failure means another thread changed the state
		return state_.compare_exchange_weak(previous_state, previous_state + 1, std::memory_order_acquire, std::memory_order_relaxed);
	}

	/// Releases shared ownership of the mutex.
	void unlock_shared() noexcept __attribute__((release_shared_capability()))
	{
#ifndef NDEBUG
		assert(state_.load(std::memory_order_relaxed) >= 1);
#endif
		// Decrement the reader count
		if(const auto previous_state = state_.fetch_sub(1, std::memory_order_release); previous_state == 1)
			// If the last reader exited wake any waiting readers or writers
			state_.notify_all();
	}

	/// Acquires exclusive ownership of the mutex, blocking if the mutex is not available.
	void lock() noexcept __attribute__((acquire_capability()))
	{
		int32_t expected = 0;
		// Loop until the state transitions from 0 (unlocked) to -1 (writer active)
		while(!state_.compare_exchange_strong(expected, -1, std::memory_order_acquire, std::memory_order_relaxed)) {
			// CAS failure means readers or another writer is active; block the caller and wait for the state to change
			state_.wait(expected, std::memory_order_acquire);
			expected = 0;
		}
	}

	/// Tries to acquire exclusive ownership of the mutex, returning true if the mutex was acquired.
	bool try_lock() noexcept __attribute__((try_acquire_capability(true)))
	{
		int32_t expected = 0;
		// Attempt to transition the state from 0 (unlocked) to -1 (writer active)
		return state_.compare_exchange_strong(expected, -1, std::memory_order_acquire, std::memory_order_relaxed);
	}

	/// Releases exclusive ownership of the mutex.
	void unlock() noexcept __attribute__((release_capability()))
	{
#ifndef NDEBUG
		assert(state_.load(std::memory_order_relaxed) == -1);
#endif
		// Reset state from -1 (writer active) to 0 (unlocked)
		state_.store(0, std::memory_order_release);
		// Notify all waiting threads (readers and potential writers) to re-contend for the lock
		state_.notify_all();
	}

private:
	/// State counter:
	/// > 0: Number of active readers.
	/// = 0: Unlocked (no readers or writer).
	/// = -1: Writer is active (critical section).
	std::atomic<int32_t> state_{0};
	static_assert(std::atomic<int32_t>::is_always_lock_free, "Lock-free std::atomic<int32_t> required");
};

} /* namespace SFB */
