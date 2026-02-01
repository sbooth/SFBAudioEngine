//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <dispatch/dispatch.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace dsema {

/// A dispatch_semaphore_t wrapper.
class Semaphore final {
  public:
    // MARK: Construction and Destruction

    /// Creates a new semaphore.
    /// @param value The starting value for the semaphore.
    /// @throw std::runtime_error if the semaphore could not be created.
    explicit Semaphore(intptr_t value);

    /// Creates a semaphore wrapping an existing dispatch semaphore.
    /// @note The results of passing a null dispatch semaphore are undefined.
    /// @param semaphore A dispatch semaphore.
    explicit Semaphore(dispatch_semaphore_t _Nonnull semaphore) noexcept;

    /// Creates a semaphore from an existing semaphore.
    /// @param other The semaphore to copy.
    Semaphore(const Semaphore &other) noexcept;

    /// Replaces this semaphore with an existing semaphore.
    /// @param other The semaphore to copy.
    /// @return A reference to this.
    Semaphore &operator=(const Semaphore &other) noexcept;

    Semaphore(Semaphore &&) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    /// Releases the underlying dispatch semaphore.
    ~Semaphore() noexcept;

    // MARK: Primitives

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

    // MARK: std::counting_semaphore Compatibility

    void acquire() noexcept;
    void release() noexcept;
    bool try_acquire() noexcept;

    template <class Rep, class Period> bool try_acquire_for(const std::chrono::duration<Rep, Period> &rel_time);

    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration> &abs_time);

  private:
    /// The underlying dispatch semaphore.
    dispatch_semaphore_t _Nonnull semaphore_{nullptr};
};

// MARK: SemaphoreGuard

/// Tag indicating that a semaphore has already been acquired and that the constructor should not wait.
struct already_acquired_t {
    explicit already_acquired_t() noexcept = default;
};

/// The semaphore has already been acquired and the constructor should not wait.
inline constexpr already_acquired_t already_acquired{};

/// A flexible scoped semaphore guard.
class SemaphoreGuard final {
  public:
    /// Constructs a semaphore guard and waits on the semaphore.
    /// @param semaphore A semaphore.
    explicit SemaphoreGuard(Semaphore &semaphore) noexcept;

    /// Constructs a semaphore guard and waits on the semaphore.
    ///
    /// If the semaphore is not acquired before the timeout expires, the guard is constructed in a non-acquired state.
    /// In this case `operator bool()` will return false and the destructor will not signal the semaphore.
    /// @param semaphore A semaphore.
    /// @param timeout The earliest time at which the function will stop waiting.
    SemaphoreGuard(Semaphore &semaphore, dispatch_time_t timeout) noexcept;

    /// Constructs a semaphore guard with an already-acquired semaphore.
    /// @param semaphore A semaphore.
    SemaphoreGuard(Semaphore &semaphore, already_acquired_t /*unused*/) noexcept;

    SemaphoreGuard(const SemaphoreGuard &) = delete;
    SemaphoreGuard &operator=(const SemaphoreGuard &) = delete;

    /// Constructs a semaphore guard by moving another.
    /// @param other The guard to move.
    SemaphoreGuard(SemaphoreGuard &&other) noexcept;

    /// Replaces this semaphore guard by moving another.
    /// @param other The guard to move.
    SemaphoreGuard &operator=(SemaphoreGuard &&other) noexcept;

    /// Signals the semaphore if it has been acquired.
    ~SemaphoreGuard() noexcept;

    /// Returns true if the semaphore has been acquired.
    [[nodiscard]] explicit operator bool() const noexcept;

    /// Returns true if the semaphore has been acquired.
    [[nodiscard]] bool acquired() const noexcept;

    /// Stops managing the semaphore without signaling.
    /// @return true if the semaphore was previously acquired, false otherwise
    bool dismiss() noexcept;

  private:
    /// A pointer to the semaphore.
    Semaphore *_Nullable semaphore_{nullptr};
    /// Whether the guard has acquired the semaphore.
    bool acquired_{false};
};

// MARK: - Implementation -

// MARK: Construction and Destruction

inline Semaphore::Semaphore(intptr_t value) : semaphore_{dispatch_semaphore_create(value)} {
    if (semaphore_ == nullptr) {
        throw std::runtime_error("Unable to create dispatch semaphore");
    }
}

inline Semaphore::Semaphore(dispatch_semaphore_t _Nonnull semaphore) noexcept : semaphore_{semaphore} {
    assert(semaphore_ != nullptr);
#if !__has_feature(objc_arc)
    dispatch_retain(semaphore_);
#endif /* !__has_feature(objc_arc) */
}

inline Semaphore::Semaphore(const Semaphore &other) noexcept : Semaphore(other.semaphore_) {}

inline Semaphore &Semaphore::operator=(const Semaphore &other) noexcept {
    if (this != &other) {
#if !__has_feature(objc_arc)
        dispatch_release(semaphore_);
#endif /* !__has_feature(objc_arc) */
        semaphore_ = other.semaphore_;
#if !__has_feature(objc_arc)
        dispatch_retain(semaphore_);
#endif /* !__has_feature(objc_arc) */
    }
    return *this;
}

inline Semaphore::~Semaphore() noexcept {
#if !__has_feature(objc_arc)
    dispatch_release(semaphore_);
#endif /* !__has_feature(objc_arc) */
}

// MARK: Primitives

inline bool Semaphore::wait(dispatch_time_t timeout) noexcept {
    return dispatch_semaphore_wait(semaphore_, timeout) == 0;
}

inline bool Semaphore::signal() noexcept { return dispatch_semaphore_signal(semaphore_) != 0; }

inline void Semaphore::wait() noexcept { wait(DISPATCH_TIME_FOREVER); }

// MARK: std::counting_semaphore Compatibility

inline void Semaphore::acquire() noexcept { wait(); }

inline void Semaphore::release() noexcept { signal(); }

inline bool Semaphore::try_acquire() noexcept { return wait(DISPATCH_TIME_NOW); }

template <class Rep, class Period>
inline bool Semaphore::try_acquire_for(const std::chrono::duration<Rep, Period> &rel_time) {
    if (rel_time <= std::chrono::duration<Rep, Period>::zero()) {
        return wait(DISPATCH_TIME_NOW);
    }
    const auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time);
    const auto timeout = dispatch_time(DISPATCH_TIME_NOW, nsec.count());
    return wait(timeout);
}

template <class Clock, class Duration>
inline bool Semaphore::try_acquire_until(const std::chrono::time_point<Clock, Duration> &abs_time) {
    const auto now = Clock::now();
    if (abs_time <= now) {
        return wait(DISPATCH_TIME_NOW);
    }
    const auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(abs_time - now);
    const auto timeout = dispatch_time(DISPATCH_TIME_NOW, nsec.count());
    return wait(timeout);
}

// MARK: - SemaphoreGuard

inline SemaphoreGuard::SemaphoreGuard(Semaphore &semaphore) noexcept
    : SemaphoreGuard(semaphore, DISPATCH_TIME_FOREVER) {}

inline SemaphoreGuard::SemaphoreGuard(Semaphore &semaphore, dispatch_time_t timeout) noexcept
    : semaphore_{&semaphore}, acquired_{semaphore.wait(timeout)} {}

inline SemaphoreGuard::SemaphoreGuard(Semaphore &semaphore, already_acquired_t /*unused*/) noexcept
    : semaphore_{&semaphore}, acquired_{true} {}

inline SemaphoreGuard::SemaphoreGuard(SemaphoreGuard &&other) noexcept
    : semaphore_{std::exchange(other.semaphore_, nullptr)}, acquired_{std::exchange(other.acquired_, false)} {}

inline SemaphoreGuard &SemaphoreGuard::operator=(SemaphoreGuard &&other) noexcept {
    if (this != &other) {
        if (semaphore_ != nullptr && acquired_) {
            semaphore_->signal();
        }
        semaphore_ = std::exchange(other.semaphore_, nullptr);
        acquired_ = std::exchange(other.acquired_, false);
    }
    return *this;
}

inline SemaphoreGuard::~SemaphoreGuard() noexcept {
    if (semaphore_ != nullptr && acquired_) {
        semaphore_->signal();
    }
}

inline SemaphoreGuard::operator bool() const noexcept { return acquired_; }

inline bool SemaphoreGuard::acquired() const noexcept { return acquired_; }

inline bool SemaphoreGuard::dismiss() noexcept {
    semaphore_ = nullptr;
    return std::exchange(acquired_, false);
}

} /* namespace dsema */
