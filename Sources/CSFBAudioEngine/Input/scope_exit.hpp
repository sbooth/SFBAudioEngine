//
// SPDX-FileCopyrightText: 2025 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <type_traits>

namespace sfb {

template <typename F>
    requires std::is_nothrow_invocable_v<F>
class scope_exit final {
  public:
    explicit scope_exit(F &&f) noexcept(std::is_nothrow_constructible_v<F>) : exit_func_(f) {}
    ~scope_exit() noexcept { exit_func_(); }

    // This class is non-copyable.
    scope_exit(const scope_exit<F> &) = delete;
    scope_exit(scope_exit<F> &&) = delete;

    // This class is non-assignable.
    scope_exit<F> &operator=(const scope_exit<F> &) = delete;
    scope_exit<F> &operator=(scope_exit<F> &&) = delete;

  private:
    F exit_func_;
};

} /* namespace sfb */
