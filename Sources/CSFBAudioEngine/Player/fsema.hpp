//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <os/os_sync_wait_on_address.h>

#include <atomic>
#include <cerrno>

namespace fsema {

/// A counting semaphore implemented using os_sync
class Semaphore final {
  private:
    /// The semaphore permit count and waiter count as a single 64-bit value.
    ///
    /// The permit count is the lower 32 bits and the waiter count is the upper 32 bits. This memory address itself
    /// doubles as the synchronization queue.
    alignas(8) std::atomic<uint64_t> state_;
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "Lock-free std::atomic<uint64_t> required");

  public:
    /// Creates a new semaphore.
    /// @param value The starting value for the semaphore.
    explicit Semaphore(uint32_t value = 0) noexcept : state_{value} {}

    Semaphore(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &) = delete;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    ~Semaphore() noexcept = default;

    /// Signals (increments) the semaphore.
    /// @return true if a thread was woken, false otherwise.
    bool signal() noexcept {
        const auto prev = state_.fetch_add(1, std::memory_order_release);
        const auto waiters = static_cast<uint32_t>(prev >> 32);
        if (waiters == 0) {
            return false;
        }
        return os_sync_wake_by_address_any(&state_, sizeof(state_), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0;
    }

    /// Waits for (decrements) the semaphore.
    /// @return true on success, false otherwise.
    bool wait() noexcept {
        constexpr uint64_t one_waiter = uint64_t{1} << 32;

        auto state = state_.load(std::memory_order_relaxed);

    fast_path:

        // Fast path: consume an available permit
        for (;;) {
            const auto permits = static_cast<uint32_t>(state);
            if (permits == 0) {
                break;
            }

            const auto desired = state - 1;
            if (state_.compare_exchange_weak(state, desired, std::memory_order_acquire, std::memory_order_relaxed)) {
                return true;
            }
        }

        // Slow path: become a waiter
        for (;;) {
            state = state_.load(std::memory_order_relaxed);

            // Check whether a permit appeared before we register as a waiter
            const auto permits = static_cast<uint32_t>(state);
            if (permits > 0) {
                goto fast_path;
            }

            const auto desired = state + one_waiter;
            if (state_.compare_exchange_weak(state, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }

        // We are now a registered waiter
        for (;;) {
            state = state_.load(std::memory_order_relaxed);

            const auto permits = static_cast<uint32_t>(state);
            if (permits != 0) {
                // Consume the permit and remove ourselves from the waiter population
                const auto desired = (state - 1) - one_waiter;
                if (state_.compare_exchange_weak(state, desired, std::memory_order_acquire,
                                                 std::memory_order_relaxed)) {
                    return true;
                }
                continue;
            }

            // No permits available; wait if the state hasn't changed
            const auto ret = os_sync_wait_on_address(&state_, state, sizeof(state_), OS_SYNC_WAIT_ON_ADDRESS_NONE);

            if (ret < 0) {
                // Retry if interrupted
                if (errno == EINTR /*|| errno == EFAULT || errno == ENOMEM*/) {
                    continue;
                }

                // Rollback the waiter registration on error
                state_.fetch_sub(one_waiter, std::memory_order_relaxed);
                return false;
            }

            // Woken or the value changed- do not assume we own a permit.
            // Re-enter the loop and compete for one.
        }
    }

    /// Waits for (decrements) the semaphore.
    ///
    /// If the resulting value is less than zero this function waits for a signal to occur before returning.
    /// @param nsec The smallest number of nanoseconds after which the function will stop waiting.
    /// @return true if the semaphore was decremented, false otherwise.
    //    bool wait(uint64_t nsec) noexcept {
    //        const auto prev = count_.fetch_sub(1, std::memory_order_release);
    //        if (prev >= 0) {
    //            return true;
    //        }
    //
    //        while (true) {
    //            auto current = count_.load(std::memory_order_relaxed);
    //            while (current > 0) {
    //                if (count_.compare_exchange_weak(current, current - 1, std::memory_order_acquire,
    //                                                 std::memory_order_relaxed)) {
    //                    return true;
    //                }
    //            }
    //
    //            if (count_.compare_exchange_weak(current, current - 1, std::memory_order_acquire,
    //            std::memory_order_relaxed)) {
    //                const auto expected = current - 1;
    //                const auto ret = os_sync_wait_on_address_with_timeout(&count_, expected, sizeof(count_),
    //                OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_CLOCK_MACH_ABSOLUTE_TIME, nsec);
    //
    //                if (ret >= 0) {
    //                    continue;
    //                }
    //
    //                if (errno == ETIMEDOUT) {
    //                    count_.fetch_add(1, std::memory_order_release);
    //                    return false;
    //                }
    //
    ////                if (errno == EINTR || errno == EFAULT) {
    ////                    continue;
    ////                }
    //
    //                return false;
    //            }
    //        }
    //    }
};

} /* namespace fsema */
