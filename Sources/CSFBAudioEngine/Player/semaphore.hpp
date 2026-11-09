//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <system_error>

#include "host_time.hpp"

namespace msema {

/// A semaphore_t wrapper.
class Semaphore final {
  public:
    /// Creates a new semaphore.
    ///
    /// The semaphore uses a first-in-first-out policy for scheduling thread wakeup.
    /// @param value The starting value for the semaphore.
    /// @throw std::system_error if the semaphore could not be created.
    explicit Semaphore(int value);

    Semaphore(const Semaphore &) noexcept = delete;
    Semaphore &operator=(const Semaphore &) noexcept = delete;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    /// Releases the underlying mach semaphore.
    ~Semaphore() noexcept;

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    void wait() noexcept;

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    /// @param wait_time How long to wait before a timeout occurs.
    /// @return true if the semaphore was acquired, false if the wait operation timed out, `wait_time` is invalid, or
    /// an error occurred.
    bool wait(mach_timespec_t wait_time) noexcept;

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    /// @param timeout How long to wait before a timeout occurs.
    /// @return true if the semaphore was acquired, false if the wait operation timed out or an error occurred.
    bool wait(std::chrono::nanoseconds timeout) noexcept;

    /// Increments the semaphore count.
    ///
    /// If the count goes non-negative (i.e. greater than or equal to 0) and a thread is blocked on the semaphore, then
    /// the waiting thread is scheduled to execute.
    /// @return true if the semaphore was signalled, false otherwise.
    bool signal() noexcept;

    /// Wakes all threads blocked on the semaphore and, if any were blocked, resets the count to zero.
    /// @return true if the semaphore was signalled, false otherwise.
    bool signal_all() noexcept;

  private:
    /// The underlying mach semaphore.
    semaphore_t semaphore_{SEMAPHORE_NULL};
};

// MARK: -

namespace detail {

/// The number of nanoseconds in one second.
inline constexpr uint64_t nsec_per_sec = 1'000'000'000;

/// Converts a nanosecond count to a mach_timespec_t, clamping tv_sec to fit in an unsigned int.
[[nodiscard]] constexpr mach_timespec_t nsec_to_timespec(uint64_t ns) noexcept {
    constexpr uint64_t max_sec = std::numeric_limits<unsigned int>::max();
    uint64_t sec = ns / nsec_per_sec;
    uint64_t nsec = ns % nsec_per_sec;
    if (sec > max_sec) {
        sec = max_sec;
        nsec = nsec_per_sec - 1;
    }
    return mach_timespec_t{static_cast<unsigned int>(sec), static_cast<clock_res_t>(nsec)};
}

/// A zero mach_timespec_t
inline constexpr mach_timespec_t timespec_zero{0, 0};

/// A std::error_category for mach return values
class mach_error_category : public std::error_category {
  public:
    const char *name() const noexcept override { return "mach"; }
    std::string message(int condition) const override {
        return mach_error_string(static_cast<mach_error_t>(condition));
    }
};

/// The shared mach_error_category instance
inline const std::error_category &mach_category() {
    static mach_error_category instance;
    return instance;
}

/// Returns a std::error_code for the given kernel return value
inline std::error_code mach_error_code(kern_return_t kr) {
    return std::error_code(static_cast<int>(kr), mach_category());
}

} /* namespace detail */

inline Semaphore::Semaphore(int value) {
    const auto kr = semaphore_create(mach_task_self(), &semaphore_, SYNC_POLICY_FIFO, value);
    if (kr != KERN_SUCCESS) {
        throw std::system_error(detail::mach_error_code(kr), "Unable to create mach semaphore");
    }
}

inline Semaphore::~Semaphore() noexcept {
    [[maybe_unused]] const auto kr = semaphore_destroy(mach_task_self(), semaphore_);
    assert(kr == KERN_SUCCESS);
}

inline void Semaphore::wait() noexcept {
    kern_return_t kr;
    do {
        kr = semaphore_wait(semaphore_);
    } while (__builtin_expect(kr == KERN_ABORTED, 0));
    assert(kr == KERN_SUCCESS);
}

inline bool Semaphore::wait(mach_timespec_t wait_time) noexcept {
    const auto badTimespec = BAD_MACH_TIMESPEC(&wait_time);
    assert(!badTimespec);
    if (badTimespec) {
        return false;
    }
    return wait(std::chrono::seconds{wait_time.tv_sec} + std::chrono::nanoseconds{wait_time.tv_nsec});
}

inline bool Semaphore::wait(std::chrono::nanoseconds timeout) noexcept {
    const auto deadline =
            timeout > std::chrono::nanoseconds::zero()
                    ? host_time::current() + host_time::fromNanoseconds(static_cast<uint64_t>(timeout.count()))
                    : uint64_t{0};

    for (;;) {
        const auto now = host_time::current();
        const auto waitTime = now >= deadline ? detail::timespec_zero
                                              : detail::nsec_to_timespec(host_time::toNanoseconds(deadline - now));
        if (const auto kr = semaphore_timedwait(semaphore_, waitTime); kr != KERN_ABORTED) {
            // Other results indicate an invalid or destroyed semaphore
            assert(kr == KERN_SUCCESS || kr == KERN_OPERATION_TIMED_OUT);
            return kr == KERN_SUCCESS;
        }
    }
}

inline bool Semaphore::signal() noexcept { return semaphore_signal(semaphore_) == KERN_SUCCESS; }

inline bool Semaphore::signal_all() noexcept { return semaphore_signal_all(semaphore_) == KERN_SUCCESS; }

} /* namespace msema */
