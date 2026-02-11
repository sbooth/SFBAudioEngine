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

/// Returns true if all non-zero bits in mask are set in value
template <BitmaskEnum E> constexpr bool has_all(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) == static_cast<U>(mask);
}

/// Returns true if at least one non-zero bit in mask is set in value
template <BitmaskEnum E> constexpr bool has_any(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) != 0;
}

/// Returns true if all non-zero bits in mask are clear in value
template <BitmaskEnum E> constexpr bool has_none(E value, E mask) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(mask)) == 0;
}

/// Returns true if all bits in value are clear
template <BitmaskEnum E> constexpr bool is_empty(E value) noexcept {
    return static_cast<std::underlying_type_t<E>>(value) == 0;
}

/// Returns true if only one bit is set in value
template <BitmaskEnum E> constexpr bool is_single_bit(E value) noexcept {
    using U = std::underlying_type_t<E>;
    U v = static_cast<U>(value);
    return v != 0 && (v & (v - 1)) == 0;
}

/// Returns true if all non-zero bits in flag are set in value
template <BitmaskEnum E> constexpr bool has_flag(E value, E flag) noexcept { return has_all(value, flag); }

/// Returns true if the non-zero bits from set are set in value and the non-zero bits from clear are clear in value
template <BitmaskEnum E> constexpr bool has_flag_but_not(E value, E set, E clear) noexcept {
    using U = std::underlying_type_t<E>;
    assert((static_cast<U>(set) & static_cast<U>(clear)) == 0 && "bits set and bits clear may not overlap");
    return (static_cast<U>(value) & (static_cast<U>(set) | static_cast<U>(clear))) == static_cast<U>(set);
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
