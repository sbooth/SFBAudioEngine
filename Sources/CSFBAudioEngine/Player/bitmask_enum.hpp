//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <type_traits>

namespace bits {

#if __cpp_lib_to_underlying >= 202102L
using std::to_underlying;
#else
/// Reimplementation of std::to_underlying
template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] constexpr std::underlying_type_t<E> to_underlying(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}
#endif

/// An enumeration supporting bitmask operations
template <typename T>
concept BitmaskEnum =
        std::is_enum_v<T> && std::unsigned_integral<std::underlying_type_t<T>> && requires(T t) { is_bitmask_enum(t); };

/// Returns the bitwise OR of lhs and rhs
template <BitmaskEnum E> [[nodiscard]] constexpr E operator|(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) | to_underlying(rhs));
}

/// Returns the bitwise AND of lhs and rhs
template <BitmaskEnum E> [[nodiscard]] constexpr E operator&(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) & to_underlying(rhs));
}

/// Returns the bitwise XOR of lhs and rhs
template <BitmaskEnum E> [[nodiscard]] constexpr E operator^(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) ^ to_underlying(rhs));
}

/// Returns true if all non-zero bits in mask are set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_all(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) == to_underlying(mask);
}

/// Returns true if at least one non-zero bit in mask is set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_any(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) != 0;
}

/// Returns true if all non-zero bits in mask are clear in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_none(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) == 0;
}

/// Returns true if all bits in value are clear
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_empty(E value) noexcept { return to_underlying(value) == 0; }

/// Returns true if only one bit is set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_single_bit(E value) noexcept {
    return std::has_single_bit(to_underlying(value));
}

/// Returns true if the non-zero bits from required are set in value and the non-zero bits from forbidden are clear in
/// value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_all_and_none(E value, E required, E forbidden) noexcept {
    assert((to_underlying(required) & to_underlying(forbidden)) == 0 &&
           "bits required and bits forbidden may not overlap");
    return (to_underlying(value) & (to_underlying(required) | to_underlying(forbidden))) == to_underlying(required);
}

} /* namespace bits */
