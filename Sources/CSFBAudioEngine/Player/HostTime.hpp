//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <mach/mach_time.h>

namespace HostTime {

/// Returns the current host time in ticks.
///
/// This is equivalent to the macOS-only function ``AudioGetCurrentHostTime``.
[[nodiscard]] inline uint64_t current() noexcept
{
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
[[nodiscard]] uint64_t toNanoseconds(uint64_t t) noexcept;

/// Converts `ns` nanoseconds to host time and returns the result.
///
/// This is equivalent to the macOS-only function ``AudioConvertNanosToHostTime``.
[[nodiscard]] uint64_t fromNanoseconds(uint64_t ns) noexcept;

} /* namespace HostTime */
