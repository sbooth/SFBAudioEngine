//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <cassert>
#import <mach/mach_time.h>

#import "SFBTimeUtilities.hpp"

namespace {

// These functions are probably unnecessarily complicated because
// on Intel processors mach_timebase_info is always 1/1. However,
// on PPC it is either 1000000000/33333335 or 1000000000/25000000 so
// naively multiplying by .numer then dividing by .denom may result in
// integer overflow. To avoid the possibility double is used here, but
// __int128 would be an alternative.

/// Returns the number of host ticks per nanosecond
double HostTicksPerNano()
{
	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return static_cast<double>(timebase_info.numer) / static_cast<double>(timebase_info.denom);
}

/// Returns the number of nanoseconds per host tick
double NanosPerHostTick()
{
	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return static_cast<double>(timebase_info.denom) / static_cast<double>(timebase_info.numer);
}

}

/// The number of host ticks per nanosecond
const double SFB::kHostTicksPerNano = HostTicksPerNano();
/// The number of nanoseconds per host tick
const double SFB::kNanosPerHostTick = NanosPerHostTick();
