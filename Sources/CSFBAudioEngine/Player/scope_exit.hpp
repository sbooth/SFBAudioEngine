//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace util {

template <typename F, typename... Args>
concept nothrow_invocable = std::invocable<F, Args...> && std::is_nothrow_invocable_v<F, Args...>;

/// A simple scope guard
template <nothrow_invocable F> class scope_exit final {
  public:
    explicit scope_exit(F f) noexcept : f_(std::move(f)), active_(true) {}

    ~scope_exit() {
        if (active_) {
            f_();
        }
    }

    scope_exit(const scope_exit &) = delete;
    scope_exit &operator=(const scope_exit &) = delete;

    scope_exit(scope_exit &&other) noexcept : f_(std::move(other.f_)), active_(other.active_) { other.active_ = false; }
    scope_exit &operator=(scope_exit &&) = delete;

  private:
    F f_;
    bool active_;
};

template <class F> scope_exit(F) -> scope_exit<F>;

} /* namespace util */
