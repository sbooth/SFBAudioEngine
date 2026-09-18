//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <dispatch/dispatch.h>

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

    Semaphore(const Semaphore &other) = delete;
    Semaphore &operator=(const Semaphore &other) = delete;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    /// Releases the underlying dispatch semaphore.
    ~Semaphore() noexcept;

    /// Waits for (decrements) the semaphore.
    ///
    /// If the resulting value is less than zero this function waits for a signal to occur before returning.
    /// @param timeout The earliest time at which the function will stop waiting.
    /// @return true if the semaphore was decremented, false otherwise.
    bool wait(dispatch_time_t timeout) noexcept;

    /// Signals (increments) the semaphore.
    ///
    /// If the previous value was less than zero, this function wakes a waiting thread.
    /// @return true if a thread was woken, false otherwise
    bool signal() noexcept;

    /// Waits for (decrements) the semaphore.
    ///
    /// If the resulting value is less than zero this function waits for a signal to occur before returning.
    void wait() noexcept;

  private:
    /// The underlying dispatch semaphore.
    dispatch_semaphore_t _Nonnull semaphore_{nullptr};
};

// MARK: - Implementation -

inline Semaphore::Semaphore(std::intptr_t value) {
    if (value < 0) {
        throw std::invalid_argument("Semaphore starting value may not be less than zero");
    }
    semaphore_ = dispatch_semaphore_create(value);
    if (semaphore_ == nullptr) {
        throw std::runtime_error("Unable to create dispatch semaphore");
    }
}

inline Semaphore::~Semaphore() noexcept {
#if !__has_feature(objc_arc)
    dispatch_release(semaphore_);
#endif /* !__has_feature(objc_arc) */
}

inline bool Semaphore::wait(dispatch_time_t timeout) noexcept {
    return dispatch_semaphore_wait(semaphore_, timeout) == 0;
}

inline bool Semaphore::signal() noexcept { return dispatch_semaphore_signal(semaphore_) != 0; }

inline void Semaphore::wait() noexcept { wait(DISPATCH_TIME_FOREVER); }

} /* namespace dsema */
