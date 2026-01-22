//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <mach/mach_time.h>

#import <cassert>
#import <cstdint>

namespace host_time {
namespace detail {

// On Intel processors mach_timebase_info is always 1/1.
// On PPC it is either 1000000000/33333335 or 1000000000/25000000.
// On Apple Silicon it is 125/3.

/// A fraction used to convert host ticks to nanoseconds.
inline const auto timebase = [] {
    // If `mach_timebase_info()` doesn't succeed there is no way to convert to/from host times.
    // Luckily the function seems to only return `KERN_SUCCESS`:
    // https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/mach_timebase_info.c#L29
    // https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/clock.c#L407

    mach_timebase_info_data_t timebase_info;
    [[maybe_unused]] const auto kr = mach_timebase_info(&timebase_info);
    assert(kr == KERN_SUCCESS);
    return timebase_info;
}();

} /* namespace detail */

/// Returns the current host time in ticks.
///
/// This is equivalent to the macOS-only function ``AudioGetCurrentHostTime``.
[[nodiscard]] inline uint64_t current() noexcept {
    // Apple recommends replacing the use of `mach_absolute_time()` with `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)`
    // (https://developer.apple.com/documentation/kernel/1462446-mach_absolute_time) because of the potential
    // to misuse the mach absolute time clock.
    //
    // However, Core Audio host time is based on the mach absolute clock time.
    //
    // On macOS the header <CoreAudio/HostTime.h> contains equivalent functions but it isn't available on iOS
    // (https://developer.apple.com/library/archive/qa/qa1643/_index.html), hence this file.

    return mach_absolute_time();
}

/// Converts host time `t` to nanoseconds and returns the result.
///
/// This is equivalent to the macOS-only function ``AudioConvertHostTimeToNanos``.
[[nodiscard]] inline uint64_t toNanoseconds(uint64_t t) noexcept {
    if (detail::timebase.numer != detail::timebase.denom) {
        __uint128_t ns = t;
        ns *= detail::timebase.numer;
        ns /= detail::timebase.denom;
        return static_cast<uint64_t>(ns);
    }

    return t;
}

/// Converts `ns` nanoseconds to host time and returns the result.
///
/// This is equivalent to the macOS-only function ``AudioConvertNanosToHostTime``.
[[nodiscard]] inline uint64_t fromNanoseconds(uint64_t ns) noexcept {
    if (detail::timebase.numer != detail::timebase.denom) {
        __uint128_t t = ns;
        t *= detail::timebase.denom;
        t /= detail::timebase.numer;
        return static_cast<uint64_t>(t);
    }

    return ns;
}

} /* namespace host_time */
