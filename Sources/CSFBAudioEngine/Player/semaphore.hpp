//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <dispatch/dispatch.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace dsema {

/// A dispatch_semaphore_t wrapper.
class Semaphore final {
  public:
    /// Creates a new semaphore.
    /// @param value The starting value for the semaphore.
    /// @throw std::invalid_argument if value is less than zero or std::runtime_error if the semaphore could not be
    /// created.
    explicit Semaphore(std::intptr_t value);

    /// Creates a semaphore from an existing semaphore.
    /// @param other The semaphore to copy.
    Semaphore(const Semaphore &other) noexcept = default;

    /// Replaces this semaphore with an existing semaphore.
    /// @param other The semaphore to copy.
    /// @return A reference to this.
    Semaphore &operator=(const Semaphore &other) noexcept = default;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    /// Releases the underlying dispatch semaphore.
    ~Semaphore() noexcept = default;

    /// Waits for (decrements) the semaphore.
    ///
    /// If the resulting value is less than zero this function waits for a signal to occur before returning.
    void wait() noexcept;

    /// Waits for (decrements) the semaphore.
    ///
    /// If the resulting value is less than zero this function waits for a signal to occur before returning.
    /// @param timeout How long to wait before timing out.
    /// @return true if the semaphore was decremented, false otherwise.
    bool wait(std::chrono::nanoseconds timeout) noexcept;

    /// Signals (increments) the semaphore.
    ///
    /// If the previous value was less than zero, this function wakes a waiting thread.
    /// @return true if a thread was woken, false otherwise
    bool signal() noexcept;

  private:
    /// The underlying dispatch semaphore.
    dispatch_semaphore_t semaphore_{nullptr};
};

// MARK: -

inline Semaphore::Semaphore(std::intptr_t value) {
    if (value < 0) {
        throw std::invalid_argument("Semaphore starting value may not be less than zero");
    }

    semaphore_ = dispatch_semaphore_create(value);
    if (semaphore_ == nullptr) {
        throw std::runtime_error("Unable to create dispatch semaphore");
    }
}

inline void Semaphore::wait() noexcept { dispatch_semaphore_wait(semaphore_, DISPATCH_TIME_FOREVER); }

inline bool Semaphore::wait(std::chrono::nanoseconds timeout) noexcept {
    if (timeout <= std::chrono::nanoseconds::zero()) {
        return dispatch_semaphore_wait(semaphore_, DISPATCH_TIME_NOW) == 0;
    }
    return dispatch_semaphore_wait(semaphore_, dispatch_time(DISPATCH_TIME_NOW, timeout.count())) == 0;
}

inline bool Semaphore::signal() noexcept { return dispatch_semaphore_signal(semaphore_) != 0; }

} /* namespace dsema */
