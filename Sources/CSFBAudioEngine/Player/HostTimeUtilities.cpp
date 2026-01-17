//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <cassert>
#import <utility>

#import "HostTimeUtilities.hpp"

namespace {

// On Intel processors mach_timebase_info is always 1/1.
// On PPC it is either 1000000000/33333335 or 1000000000/25000000.
// On Apple Silicon it is 125/3.

/// Returns a fraction used to convert host ticks to nanoseconds.
auto machTimebase() noexcept
{
	// If `mach_timebase_info()` doesn't succeed there is no way to convert to/from host times.
	// Luckily the function seems to only return `KERN_SUCCESS`:
	// https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/mach_timebase_info.c#L29
	// https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/clock.c#L407

	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return std::make_pair(timebase_info.numer, timebase_info.denom);
}

/// Mach timebase information.
const auto kMachTimebase = machTimebase();

} /* namespace */

uint64_t SFB::hostTimeToNanoseconds(uint64_t t) noexcept
{
	if(kMachTimebase.first != kMachTimebase.second) {
		__uint128_t ns = t;
		ns *= kMachTimebase.first;
		ns /= kMachTimebase.second;
		return static_cast<uint64_t>(ns);
	}

	return t;
}

uint64_t SFB::nanosecondsToHostTime(uint64_t ns) noexcept
{
	if(kMachTimebase.first != kMachTimebase.second) {
		__uint128_t t = ns;
		t *= kMachTimebase.second;
		t /= kMachTimebase.first;
		return static_cast<uint64_t>(t);
	}

	return ns;
}
