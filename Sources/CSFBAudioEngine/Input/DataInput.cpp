//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "DataInput.hpp"

SFB::DataInput::DataInput(CFDataRef data) noexcept
{
	if(data)
		data_ = (CFDataRef)CFRetain(data);
}

SFB::DataInput::~DataInput() noexcept
{
	if(data_)
		CFRelease(data_);
}

std::expected<void, int> SFB::DataInput::_Open() noexcept
{
	if(!data_)
		return std::unexpected(ENOENT);
	pos_ = 0;
	return {};
}

std::expected<void, int> SFB::DataInput::_Close() noexcept
{
	return {};
}

std::expected<int64_t, int> SFB::DataInput::_Read(void *buffer, int64_t count) noexcept
{
	int64_t remaining = CFDataGetLength(data_) - pos_;
	count = std::min(count, remaining);

	auto range = CFRangeMake(pos_, pos_ + count);
	CFDataGetBytes(data_, range, static_cast<UInt8 *>(buffer));

	pos_ += count;

	return count;
}

std::expected<bool, int> SFB::DataInput::_AtEOF() const noexcept
{
	return CFDataGetLength(data_) == pos_;
}

std::expected<int64_t, int> SFB::DataInput::_GetOffset() const noexcept
{
	return pos_;
}

std::expected<int64_t, int> SFB::DataInput::_GetLength() const noexcept
{
	return CFDataGetLength(data_);
}

bool SFB::DataInput::_SupportsSeeking() const noexcept
{
	return true;
}

std::expected<void, int> SFB::DataInput::_SeekToOffset(int64_t offset, int whence) noexcept
{
	auto length = CFDataGetLength(data_);

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
			return std::unexpected(EINVAL);
	}

	if(offset < 0 || offset > length)
		return std::unexpected(EINVAL);

	pos_ = offset;
	return {};
}
