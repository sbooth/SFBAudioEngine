//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <limits>

#import "DataInput.hpp"

SFB::DataInput::DataInput(CFDataRef data)
{
	if(!data) {
		os_log_error(sLog, "Cannot create DataInput with null data");
		throw std::invalid_argument("Null data");
	}
	data_ = static_cast<CFDataRef>(CFRetain(data));
}

SFB::DataInput::~DataInput() noexcept
{
	CFRelease(data_);
}

int64_t SFB::DataInput::_Read(void *buffer, int64_t count)
{
	if(count > std::numeric_limits<CFIndex>::max()) {
		os_log_error(sLog, "_Read() called on <DataInput: %p> with count greater than maximum allowable value", this);
		throw std::invalid_argument("Count greater than maximum allowable value");
	}
	const int64_t remaining = CFDataGetLength(data_) - pos_;
	count = std::min(count, remaining);
	const auto range = CFRangeMake(pos_, count);
	CFDataGetBytes(data_, range, static_cast<UInt8 *>(buffer));
	pos_ += count;
	return count;
}

CFStringRef SFB::DataInput::_CopyDescription() const noexcept
{
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<DataInput %p: %@>"), this, data_);
}
