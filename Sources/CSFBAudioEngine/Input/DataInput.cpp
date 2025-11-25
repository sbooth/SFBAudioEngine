//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "DataInput.hpp"

SFB::DataInput::DataInput(CFDataRef data)
{
	if(!data) {
		os_log_error(sLog, "Cannot create DataInput with null CFData");
		throw std::invalid_argument("Null data");
	}
	data_ = static_cast<CFDataRef>(CFRetain(data));
}

int64_t SFB::DataInput::_Read(void *buffer, int64_t count)
{
	if(count > LONG_MAX) {
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

void SFB::DataInput::_SeekToOffset(int64_t offset, int whence)
{
	const auto length = CFDataGetLength(data_);

	switch(whence) {
		case SEEK_SET:
			break;
		case SEEK_CUR:
			offset += pos_;
			break;
		case SEEK_END:
			offset += length;
			break;
		default:
			os_log_error(sLog, "_SeekToOffset() called on <DataInput: %p> with unknown whence %d", this, whence);
			throw std::invalid_argument("Unknown whence");
	}

	if(offset < 0 || offset > length) {
		os_log_error(sLog, "_SeekToOffset() called on <DataInput: %p> with invalid seek offset %lld", this, offset);
		throw std::out_of_range("Invalid seek offset");
	}

	pos_ = offset;
}
