//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import <mach/mach_time.h>
#import <mach/semaphore.h>

#import <cassert>
#import <limits>
#import <stdexcept>

namespace msema {

/// A semaphore_t wrapper.
class Semaphore final {
  public:
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

namespace detail {

/// The number of nanoseconds in one second.
inline constexpr uint64_t nsec_per_sec = 1'000'000'000;

/// A fraction used to convert host ticks to nanoseconds.
inline const auto timebase = []() noexcept {
    mach_timebase_info_data_t timebase_info;
    [[maybe_unused]] const auto kr = mach_timebase_info(&timebase_info);
    assert(kr == KERN_SUCCESS);
    return timebase_info;
}();

/// Converts mach ticks to nanoseconds.
[[nodiscard]] inline uint64_t ticks_to_nsec(uint64_t ticks) noexcept {
    if (timebase.numer != timebase.denom) {
        __uint128_t ns = ticks;
        ns *= timebase.numer;
        ns /= timebase.denom;
        return static_cast<uint64_t>(ns);
    }
    return ticks;
}

/// Converts nanoseconds to mach ticks.
[[nodiscard]] inline uint64_t nsec_to_ticks(uint64_t ns) noexcept {
    if (timebase.numer != timebase.denom) {
        __uint128_t t = ns;
        t *= detail::timebase.denom;
        t /= detail::timebase.numer;
        return static_cast<uint64_t>(t);
    }
    return ns;
}

/// Converts a mach_timespec_t to a nanosecond count.
[[nodiscard]] constexpr uint64_t timespec_to_nsec(mach_timespec_t ts) noexcept {
    return static_cast<uint64_t>(ts.tv_sec) * nsec_per_sec + static_cast<uint64_t>(ts.tv_nsec);
}

/// Converts a nanosecond count to a mach_timespec_t, clamping tv_sec to fit in an unsigned int.
[[nodiscard]] constexpr mach_timespec_t nsec_to_timespec(uint64_t nanos) noexcept {
    constexpr uint64_t max_seconds = std::numeric_limits<unsigned int>::max();
    uint64_t sec = nanos / nsec_per_sec;
    uint64_t nsec = nanos % nsec_per_sec;
    if (sec > max_seconds) {
        sec = max_seconds;
        nsec = nsec_per_sec - 1;
    }
    return mach_timespec_t{static_cast<unsigned int>(sec), static_cast<clock_res_t>(nsec)};
}

} /* namespace detail */

inline Semaphore::Semaphore(int value) : task_{mach_task_self()} {
    if (semaphore_create(task_, &semaphore_, SYNC_POLICY_FIFO, value) != KERN_SUCCESS) {
        throw std::runtime_error("Unable to create mach semaphore");
    }
}

inline Semaphore::~Semaphore() noexcept { (void)semaphore_destroy(task_, semaphore_); }

inline bool Semaphore::wait() noexcept {
    kern_return_t kr;
    do {
        kr = semaphore_wait(semaphore_);
    } while (__builtin_expect(kr == KERN_ABORTED, 0));
    return kr == KERN_SUCCESS;
}

inline bool Semaphore::timedwait(mach_timespec_t wait_time) noexcept {
    if (wait_time.tv_sec == 0 && wait_time.tv_nsec == 0) {
        return semaphore_timedwait(semaphore_, wait_time) == KERN_SUCCESS;
    }

    const auto wait_nsec = detail::timespec_to_nsec(wait_time);
    const auto deadline = mach_absolute_time() + detail::nsec_to_ticks(wait_nsec);

    for (;;) {
        const auto now = mach_absolute_time();
        if (now >= deadline) {
            return semaphore_timedwait(semaphore_, mach_timespec_t{0, 0}) == KERN_SUCCESS;
        }

        const auto remaining_nsec = detail::ticks_to_nsec(deadline - now);
        const auto current_wait = detail::nsec_to_timespec(remaining_nsec);

        switch (semaphore_timedwait(semaphore_, current_wait)) {
        case KERN_SUCCESS:
            return true;
        case KERN_OPERATION_TIMED_OUT:
            return false;
        case KERN_ABORTED:
            continue;
        default:
            return false;
        }
    }
}

inline bool Semaphore::signal() noexcept { return semaphore_signal(semaphore_) == KERN_SUCCESS; }

inline bool Semaphore::signal_all() noexcept { return semaphore_signal_all(semaphore_) == KERN_SUCCESS; }

} /* namespace msema */
