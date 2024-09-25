//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

//#import <ctime>
#import <mach/mach_time.h>

namespace SFB {

/// Converts host time `t` to nanoseconds and returns the result
uint64_t ConvertHostTimeToNanoseconds(uint64_t t) noexcept;

/// Converts `ns` nanoseconds to host time and returns the result
uint64_t ConvertNanosecondsToHostTime(uint64_t ns) noexcept;

/// Returns the current host time
inline uint64_t GetCurrentHostTime() noexcept
{
	return mach_absolute_time();
//	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

/// Converts `s` seconds to host time and returns the result
inline uint64_t ConvertSecondsToHostTime(double s) noexcept
{
	return ConvertNanosecondsToHostTime(static_cast<uint64_t>(s * 1e9));
}

/// Returns the absolute delta between `t1` and `t2` host time values in nanoseconds
inline uint64_t ConvertAbsoluteHostTimeDeltaToNanoseconds(uint64_t t1, uint64_t t2) noexcept
{
	return ConvertHostTimeToNanoseconds(t2 > t1 ? t2 - t1 : t1 - t2);
}

}
