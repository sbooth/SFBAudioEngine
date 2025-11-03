//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import "InputSource.hpp"
#import "DataInput.hpp"
#import "FileInput.hpp"

namespace SFB {

const os_log_t InputSource::sLog = os_log_create("org.sbooth.AudioEngine", "InputSource");

} /* namespace SFB */

SFB::InputSource::InputSource(CFURLRef url) noexcept
{
	if(url)
		url_ = (CFURLRef)CFRetain(url);
}

SFB::InputSource::~InputSource()
{
	if(isOpen_)
		Close();
	if(url_)
		CFRelease(url_);
}

std::expected<void, int> SFB::InputSource::Open() noexcept
{
	if(IsOpen()) {
		os_log_debug(sLog, "Open() called on an InputSource that is already open");
		return;
	}

	auto result = _Open();
	if(result)
		isOpen_ = true;
	return result;
}

std::expected<void, int> SFB::InputSource::Close() noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "Close() called on an InputSource that hasn't been opened");
		return;
	}

	auto result = _Close();
	isOpen_ = false;
	return result;
}

std::expected<int64_t, int> SFB::InputSource::Read(void *buffer, int64_t count) noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "Read() called on an InputSource that hasn't been opened");
		return std::unexpected(EINVAL);
	}

	if(!buffer || count < 0) {
		os_log_debug(sLog, "Read() called with null buffer or invalid count");
		return std::unexpected(EINVAL);
	}

	return _Read(buffer, count);
}

std::expected<bool, int> SFB::InputSource::AtEOF() const noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "AtEOF() called on an InputSource that hasn't been opened");
		return std::unexpected(EINVAL);
	}

	return _AtEOF();
}

std::expected<int64_t, int> SFB::InputSource::GetOffset() const noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "GetOffset() called on an InputSource that hasn't been opened");
		return std::unexpected(EINVAL);
	}

	return _GetOffset();
}

std::expected<int64_t, int> SFB::InputSource::GetLength() const noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "GetLength() called on an InputSource that hasn't been opened");
		return std::unexpected(EINVAL);
	}

	return _GetLength();
}

bool SFB::InputSource::SupportsSeeking() const noexcept
{
	return _SupportsSeeking();
}

std::expected<void, int> SFB::InputSource::SeekToOffset(int64_t offset, int whence) noexcept
{
	if(!IsOpen()) {
		os_log_debug(sLog, "SeekToOffset() called on an InputSource that hasn't been opened");
		return std::unexpected(EINVAL);
	}

	return _SeekToOffset(offset, whence);
}

// Seeking support is optional

bool SFB::InputSource::_SupportsSeeking() const noexcept
{
	return false;
}

std::expected<void, int> SFB::InputSource::_SeekToOffset(int64_t offset, int whence) noexcept
{
	return std::unexpected(EBADF);
}
