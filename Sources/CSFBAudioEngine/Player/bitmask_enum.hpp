//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <cassert>
#include <concepts>
#include <type_traits>

namespace bits {

/// Returns the bitwise OR of l and r
template <typename E> constexpr E or_impl(E l, E r) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(l) | static_cast<U>(r));
}

/// Returns the bitwise AND of l and r
template <typename E> constexpr E and_impl(E l, E r) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(l) & static_cast<U>(r));
}

/// Returns the bitwise XOR of l and r
template <typename E> constexpr E xor_impl(E l, E r) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(l) ^ static_cast<U>(r));
}

/// Returns the bitwise NOT of v
template <typename E> constexpr E not_impl(E v) noexcept {
    return static_cast<E>(~static_cast<std::underlying_type_t<E>>(v));
}

/// An enumeration supporting bitmask operations
template <typename T>
concept BitmaskEnum =
        std::is_enum_v<T> && std::is_unsigned_v<std::underlying_type_t<T>> && requires(T t) { is_bitmask_enum(t); };

/// Returns true if all bits in mask are set in value
template <BitmaskEnum E> constexpr bool has_all(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) == static_cast<U>(mask);
}

/// Returns true if at least one bit in mask is set in value
template <BitmaskEnum E> constexpr bool has_any(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) != 0;
}

/// Returns true if none of the bits are set in value
template <BitmaskEnum E> constexpr bool is_empty(E value) noexcept {
    return static_cast<std::underlying_type_t<E>>(value) == 0;
}

/// Returns true if only one bit is set in value
template <BitmaskEnum E> constexpr bool is_single_bit(E value) noexcept {
    using U = std::underlying_type_t<E>;
    U v = static_cast<U>(value);
    return v != 0 && (v & (v - 1)) == 0;
}

/// Returns true if all bits in flag are set in value
template <BitmaskEnum E> constexpr bool has_flag(E value, E flag) noexcept { return has_all(value, flag); }

/// Returns true if the mask bits from value match expected
template <BitmaskEnum E> constexpr bool masked_matches(E value, E mask, E expected) noexcept {
    using U = std::underlying_type_t<E>;
    assert((static_cast<U>(expected) & ~static_cast<U>(mask)) == 0 && "expected bits must be a subset of mask bits");
    return (static_cast<U>(value) & static_cast<U>(mask)) == static_cast<U>(expected);
}

/// Returns value with the specified bits turned off
template <BitmaskEnum E> constexpr E remove_mask(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(value) & ~static_cast<U>(mask));
}

/// Returns value with the specified bits toggled
template <BitmaskEnum E> constexpr E toggle_mask(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(value) ^ static_cast<U>(mask));
}

} /* namespace bits */

// Global operators
//
// template <bits::BitmaskEnum E> constexpr E operator|(E l, E r) noexcept { return bits::or_impl(l, r); }
// template <bits::BitmaskEnum E> constexpr E operator&(E l, E r) noexcept { return bits::and_impl(l, r); }
// template <bits::BitmaskEnum E> constexpr E operator^(E l, E r) noexcept { return bits::xor_impl(l, r); }
// template <bits::BitmaskEnum E> constexpr E operator~(E v) noexcept { return bits::not_impl(v); }
//
// template <bits::BitmaskEnum E> constexpr E& operator|=(E& l, E r) noexcept { return l = (l | r); }
// template <bits::BitmaskEnum E> constexpr E& operator&=(E& l, E r) noexcept { return l = (l & r); }
// template <bits::BitmaskEnum E> constexpr E& operator^=(E& l, E r) noexcept { return l = (l ^ r); }
