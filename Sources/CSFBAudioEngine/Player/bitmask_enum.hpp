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

// MARK: Single-Bit Tests

/// Returns true if only one bit is set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_single_bit(E value) noexcept {
    return std::has_single_bit(to_underlying(value));
}

/// Returns true if the non-zero bit in flag is set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_set(E value, E flag) noexcept {
#if DEBUG
    assert(is_single_bit(flag));
#endif /* DEBUG */
    return (to_underlying(value) & to_underlying(flag)) != 0;
}

/// Returns true if the non-zero bit in flag is clear in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_clear(E value, E flag) noexcept {
#if DEBUG
    assert(is_single_bit(flag));
#endif /* DEBUG */
    return (to_underlying(value) & to_underlying(flag)) == 0;
}

/// Returns true if the non-zero bit from required is set in value and the non-zero bit from forbidden is clear in
/// value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_set_and_is_clear(E value, E required, E forbidden) noexcept {
#if DEBUG
    assert(is_single_bit(required));
    assert(is_single_bit(forbidden));
    assert((to_underlying(required) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return (to_underlying(value) & (to_underlying(required) | to_underlying(forbidden))) == to_underlying(required);
}

/// Returns true if the non-zero bit from allowed is set in value or the non-zero bit from forbidden is clear in
/// value
template <BitmaskEnum E> [[nodiscard]] constexpr bool is_set_or_is_clear(E value, E allowed, E forbidden) noexcept {
#if DEBUG
    assert(is_single_bit(allowed));
    assert(is_single_bit(forbidden));
    assert((to_underlying(allowed) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return (to_underlying(value) & (to_underlying(allowed) | to_underlying(forbidden))) != to_underlying(forbidden);
}

// MARK: Multi-Bit Tests

/// Returns true if all non-zero bits in mask are set in value or if mask is empty
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_all(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) == to_underlying(mask);
}

/// Returns true if at least one non-zero bit in mask is set in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_any(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) != 0;
}

/// Returns true if all non-zero bits in mask are clear in value or if mask is empty
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_none(E value, E mask) noexcept {
    return (to_underlying(value) & to_underlying(mask)) == 0;
}

/// Returns true if all bits in value are clear
template <BitmaskEnum E> [[nodiscard]] constexpr bool none_set(E value) noexcept { return to_underlying(value) == 0; }

/// Returns true if all non-zero bits from required are set in value and all non-zero bits from forbidden are clear in
/// value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_all_and_has_none(E value, E required, E forbidden) noexcept {
#if DEBUG
    assert(!none_set(required));
    assert(!none_set(forbidden));
    assert((to_underlying(required) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return has_all(value, required) && has_none(value, forbidden);
}

/// Returns true if all non-zero bits from required are set in value or all non-zero bits from forbidden are clear in
/// value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_all_or_has_none(E value, E required, E forbidden) noexcept {
#if DEBUG
    assert(!none_set(required));
    assert(!none_set(forbidden));
    assert((to_underlying(required) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return has_all(value, required) || has_none(value, forbidden);
}

/// Returns true if any non-zero bits from allowed are set in value and all non-zero bits from forbidden are
/// clear in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_any_and_has_none(E value, E allowed, E forbidden) noexcept {
#if DEBUG
    assert(!none_set(allowed));
    assert(!none_set(forbidden));
    assert((to_underlying(allowed) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return has_any(value, allowed) && has_none(value, forbidden);
}

/// Returns true if any non-zero bits from allowed are set in value or all non-zero bits from forbidden are
/// clear in value
template <BitmaskEnum E> [[nodiscard]] constexpr bool has_any_or_has_none(E value, E allowed, E forbidden) noexcept {
#if DEBUG
    assert(!none_set(allowed));
    assert(!none_set(forbidden));
    assert((to_underlying(allowed) & to_underlying(forbidden)) == 0);
#endif /* DEBUG */
    return has_any(value, allowed) || has_none(value, forbidden);
}

} /* namespace bits */
