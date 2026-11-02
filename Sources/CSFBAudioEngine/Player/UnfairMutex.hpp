//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <os/availability.h>
#include <os/lock.h>

namespace mtx {

/// A wrapper around os_unfair_lock satisfying the Lockable requirements.
///
/// This class may be used with std::lock_guard for a scope-based lock.
class __attribute__((capability("mutex"))) UnfairMutex final {
  public:
    // MARK: Construction and Destruction

    /// Creates a new unfair mutex.
    UnfairMutex() noexcept = default;

    UnfairMutex(const UnfairMutex &) = delete;
    UnfairMutex &operator=(const UnfairMutex &) = delete;

    UnfairMutex(UnfairMutex &&) = delete;
    UnfairMutex &operator=(UnfairMutex &&) = delete;

    /// Destroys the unfair mutex.
    ~UnfairMutex() noexcept = default;

    // MARK: Lockable

    /// Locks the mutex.
    void lock() noexcept __attribute__((acquire_capability()));

    /// Locks the mutex.
    /// @param flags Flags to alter the behavior of the os_unfair_lock.
    void lock(os_unfair_lock_flags_t flags) noexcept __attribute__((acquire_capability()))
    API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0), visionos(2.0));

    /// Unlocks the mutex.
    void unlock() noexcept __attribute__((release_capability()));

    /// Attempts to lock the mutex.
    /// @return true if the mutex was successfully acquired, false if the mutex was already locked.
    [[nodiscard]] bool try_lock() noexcept __attribute__((try_acquire_capability(true)));

    // MARK: Ownership

    /// Asserts that the calling thread is the current owner of the mutex.
    ///
    /// If the mutex is currently owned by the calling thread, this function returns.
    ///
    /// If the mutex is unlocked or owned by a different thread, this function asserts and terminates the process.
    void assertIsOwner() const noexcept __attribute__((assert_capability(this)));

    /// Asserts that the calling thread is not the current owner of the mutex.
    ///
    /// If the mutex is unlocked or owned by a different thread, this function returns.
    ///
    /// If the mutex is currently owned by the current thread, this function asserts and terminates the process.
    void assertIsNotOwner() const noexcept;

  private:
    /// The locking primitive.
    os_unfair_lock lock_{OS_UNFAIR_LOCK_INIT};
};

// MARK: - Implementation -

// MARK: Lockable

inline void UnfairMutex::lock() noexcept { os_unfair_lock_lock(&lock_); }

inline void UnfairMutex::lock(os_unfair_lock_flags_t flags) noexcept { os_unfair_lock_lock_with_flags(&lock_, flags); }

inline void UnfairMutex::unlock() noexcept { os_unfair_lock_unlock(&lock_); }

inline bool UnfairMutex::try_lock() noexcept { return os_unfair_lock_trylock(&lock_); }

// MARK: Ownership

inline void UnfairMutex::assertIsOwner() const noexcept { os_unfair_lock_assert_owner(&lock_); }

inline void UnfairMutex::assertIsNotOwner() const noexcept { os_unfair_lock_assert_not_owner(&lock_); }

} /* namespace mtx */
