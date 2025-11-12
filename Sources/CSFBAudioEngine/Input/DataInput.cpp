//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "DataInput.hpp"

std::expected<int64_t, int> SFB::DataInput::_Read(void *buffer, int64_t count) noexcept
{
	if(count > LONG_MAX)
		return std::unexpected{EINVAL};
	const int64_t remaining = CFDataGetLength(data_) - pos_;
	count = std::min(count, remaining);
	const auto range = CFRangeMake(pos_, count);
	CFDataGetBytes(data_, range, static_cast<UInt8 *>(buffer));
	pos_ += count;
	return count;
}

std::expected<void, int> SFB::DataInput::_SeekToOffset(int64_t offset, int whence) noexcept
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
			return std::unexpected{EINVAL};
	}

	if(offset < 0 || offset > length)
		return std::unexpected{EINVAL};

	pos_ = offset;
	return {};
}
