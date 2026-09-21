//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import <mach/mach_error.h>
#import <mach/mach_init.h>
#import <mach/mach_time.h>
#import <mach/semaphore.h>
#import <mach/task.h>

#import <cassert>
#import <limits>
#import <system_error>

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
    /// @return true if the semaphore wait operation was successful, false otherwise.
    bool wait() noexcept;

    /// Possible results for a timed wait.
    enum class timedwait_result {
        /// The semaphore was acquired.
        acquired,
        /// The wait operation timed out.
        timed_out,
        /// An error occured.
        error,
    };

    /// Decrements the semaphore count.
    ///
    /// If the semaphore count is negative after decrementing, the calling thread blocks.
    /// @param wait_time How long to wait before a timeout occurs.
    /// @return A result indicating whether the semaphore was acquired, the wait operation timed out, or an error
    /// occurred.
    timedwait_result timedwait(mach_timespec_t wait_time) noexcept;

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
    semaphore_t semaphore_{SEMAPHORE_NULL};
    /// The mach task associated with the semaphore.
    task_t task_{TASK_NULL};
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
        t *= timebase.denom;
        t /= timebase.numer;
        return static_cast<uint64_t>(t);
    }
    return ns;
}

/// Converts a mach_timespec_t to a nanosecond count.
[[nodiscard]] constexpr uint64_t timespec_to_nsec(mach_timespec_t ts) noexcept {
    return static_cast<uint64_t>(ts.tv_sec) * nsec_per_sec + static_cast<uint64_t>(ts.tv_nsec);
}

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

/// Converts a kernel return value to a timedwait_result
[[nodiscard]] constexpr Semaphore::timedwait_result to_timedwait_result(kern_return_t kr) noexcept {
    switch (kr) {
    case KERN_SUCCESS:
        return Semaphore::timedwait_result::acquired;
    case KERN_OPERATION_TIMED_OUT:
        return Semaphore::timedwait_result::timed_out;
    default:
        return Semaphore::timedwait_result::error;
    }
}

} /* namespace detail */

inline Semaphore::Semaphore(int value) : task_{mach_task_self()} {
    const auto kr = semaphore_create(task_, &semaphore_, SYNC_POLICY_FIFO, value);
    if (kr != KERN_SUCCESS) {
        throw std::system_error(detail::mach_error_code(kr), "Unable to create mach semaphore");
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

inline auto Semaphore::timedwait(mach_timespec_t wait_time) noexcept -> timedwait_result {
    if (BAD_MACH_TIMESPEC(&wait_time)) {
        return timedwait_result::error;
    }

    if (wait_time.tv_sec == 0 && wait_time.tv_nsec == 0) {
        return detail::to_timedwait_result(semaphore_timedwait(semaphore_, detail::timespec_zero));
    }

    const auto wait_nsec = detail::timespec_to_nsec(wait_time);
    const auto deadline = mach_absolute_time() + detail::nsec_to_ticks(wait_nsec);

    for (;;) {
        const auto now = mach_absolute_time();
        if (now >= deadline) {
            return detail::to_timedwait_result(semaphore_timedwait(semaphore_, detail::timespec_zero));
        }

        const auto remaining_nsec = detail::ticks_to_nsec(deadline - now);
        const auto current_wait = detail::nsec_to_timespec(remaining_nsec);

        const auto kr = semaphore_timedwait(semaphore_, current_wait);
        if (kr == KERN_ABORTED) {
            continue;
        }
        return detail::to_timedwait_result(kr);
    }
}

inline bool Semaphore::signal() noexcept { return semaphore_signal(semaphore_) == KERN_SUCCESS; }

inline bool Semaphore::signal_all() noexcept { return semaphore_signal_all(semaphore_) == KERN_SUCCESS; }

} /* namespace msema */
