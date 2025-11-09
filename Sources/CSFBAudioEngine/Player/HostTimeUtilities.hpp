//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <mach/mach_time.h>

namespace SFB {

/// Converts host time `t` to nanoseconds and returns the result.
///
/// This is equivalent to the macOS-only function ``AudioConvertHostTimeToNanos``.
uint64_t ConvertHostTimeToNanoseconds(uint64_t t) noexcept;

/// Converts `ns` nanoseconds to host time and returns the result.
///
/// This is equivalent to the macOS-only function ``AudioConvertNanosToHostTime``.
uint64_t ConvertNanosecondsToHostTime(uint64_t ns) noexcept;

/// Returns the current host time.
///
/// This is equivalent to the macOS-only function ``AudioGetCurrentHostTime``.
inline uint64_t GetCurrentHostTime() noexcept
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

/// Converts `s` seconds to host time and returns the result.
inline uint64_t ConvertSecondsToHostTime(double s) noexcept
{
	return ConvertNanosecondsToHostTime(static_cast<uint64_t>(s * 1e9));
}

/// Returns the absolute value of the delta between `t1` and `t2` host time values in nanoseconds.
inline uint64_t ConvertAbsoluteHostTimeDeltaToNanoseconds(uint64_t t1, uint64_t t2) noexcept
{
	return ConvertHostTimeToNanoseconds(t2 > t1 ? t2 - t1 : t1 - t2);
}

} /* namespace SFB */
