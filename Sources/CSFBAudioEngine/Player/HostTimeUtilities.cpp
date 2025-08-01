//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
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

/// Returns a fraction used to convert host ticks to nanoseconds
auto MachTimebase() noexcept
{
	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return std::make_pair(timebase_info.numer, timebase_info.denom);
}

/// Mach timebase information
const auto kMachTimebase = MachTimebase();

} /* namespace */

uint64_t SFB::ConvertHostTimeToNanoseconds(uint64_t t) noexcept
{
	__uint128_t ns = t;
	if(kMachTimebase.first != kMachTimebase.second) {
		ns *= kMachTimebase.first;
		ns /= kMachTimebase.second;
	}
	return static_cast<uint64_t>(ns);
}

uint64_t SFB::ConvertNanosecondsToHostTime(uint64_t ns) noexcept
{
	__uint128_t t = ns;
	if(kMachTimebase.first != kMachTimebase.second) {
		t *= kMachTimebase.second;
		t /= kMachTimebase.first;
	}
	return static_cast<uint64_t>(t);
}
