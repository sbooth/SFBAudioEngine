//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import <mach/mach.h>

#import <stdexcept>

namespace msema {

/// A semaphore_t wrapper.
class Semaphore final {
  public:
    // MARK: Construction and Destruction

    /// Creates a new semaphore.
    ///
    /// The semaphore uses a first-in-first-out policy for scheduling thread wakeup.
    /// @param value The starting value for the semaphore.
    /// @throw std::runtime_error if the semaphore could not be created.
    explicit Semaphore(int value);

    Semaphore(const Semaphore &) noexcept = delete;
    Semaphore &operator=(const Semaphore &) noexcept = delete;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    /// Releases the underlying mach semaphore.
    ~Semaphore() noexcept;

    // MARK: Primitives

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    /// @return true if the semaphore wait operation was successful, false otherwise.
    bool wait() noexcept;

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    /// @param wait_time How long to wait before a timeout occurs.
    /// @return true if the semaphore wait operation was successful, false otherwise.
    bool timedwait(mach_timespec_t wait_time) noexcept;

    /// Increments the semaphore count.
    ///
    /// If the count goes non-negative (i.e. greater than or equal to 0) and a thread is blocked on the semaphore, then
    /// the waiting thread is scheduled to execute.
    /// @return true if the semaphore was signalled, false otherwise.
    bool signal() noexcept;

    /// Wakes up all of the threads blocked on the semaphore.
    ///
    /// The semaphore count is reset to zero.
    /// @return true if the semaphore was signalled, false otherwise.
    bool signal_all() noexcept;

  private:
    /// The underlying mach semaphore.
    semaphore_t semaphore_{0};
    /// The mach task associated with the semaphore.
    task_t task_{0};
};

// MARK: - Implementation -

inline Semaphore::Semaphore(int value) : task_{mach_task_self()} {
    if (semaphore_create(task_, &semaphore_, SYNC_POLICY_FIFO, value) != KERN_SUCCESS) {
        throw std::runtime_error("Unable to create mach semaphore");
    }
}

inline Semaphore::~Semaphore() noexcept { (void)semaphore_destroy(task_, semaphore_); }

inline bool Semaphore::wait() noexcept { return semaphore_wait(semaphore_) == KERN_SUCCESS; }

inline bool Semaphore::timedwait(mach_timespec_t wait_time) noexcept {
    switch (semaphore_timedwait(semaphore_, wait_time)) {
    case KERN_SUCCESS:
        return true;
    case KERN_OPERATION_TIMED_OUT:
        return false;
    default:
        return false;
    }
}

inline bool Semaphore::signal() noexcept { return semaphore_signal(semaphore_) == KERN_SUCCESS; }

inline bool Semaphore::signal_all() noexcept { return semaphore_signal_all(semaphore_) == KERN_SUCCESS; }

} /* namespace msema */
