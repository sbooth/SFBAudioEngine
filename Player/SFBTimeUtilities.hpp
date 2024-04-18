//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

namespace SFB {

/// The number of host ticks per nanosecond
extern const double kHostTicksPerNano;
/// The number of nanoseconds per host tick
extern const double kNanosPerHostTick;

/// Converts \c ns nanoseconds to host ticks and returns the result
inline uint64_t ConvertNanosToHostTicks(double ns) noexcept
{
	return static_cast<uint64_t>(ns * kNanosPerHostTick);
}

/// Converts \c s seconds to host ticks and returns the result
inline uint64_t ConvertSecondsToHostTicks(double s) noexcept
{
	return ConvertNanosToHostTicks(s * NSEC_PER_SEC);
}

/// Converts \c t host ticks to nanoseconds and returns the result
inline double ConvertHostTicksToNanos(uint64_t t) noexcept
{
	return static_cast<double>(t) * kHostTicksPerNano;
}

}
