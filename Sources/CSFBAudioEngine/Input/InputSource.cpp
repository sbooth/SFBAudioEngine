//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <stdexcept>

#import "InputSource.hpp"
#import "scope_exit.hpp"

namespace SFB {

const os_log_t InputSource::sLog = os_log_create("org.sbooth.AudioEngine", "InputSource");

} /* namespace SFB */

void SFB::InputSource::Open()
{
	if(IsOpen()) {
		os_log_debug(sLog, "Open() called on an InputSource that is already open");
		return;
	}

	_Open();
	isOpen_ = true;
}

void SFB::InputSource::Close()
{
	if(!IsOpen()) {
		os_log_debug(sLog, "Close() called on an InputSource that hasn't been opened");
		return;
	}

	const auto defer = scope_exit{[this] noexcept { isOpen_ = false; }};
	_Close();
}

int64_t SFB::InputSource::Read(void *buffer, int64_t count)
{
	if(!IsOpen()) {
		os_log_debug(sLog, "Read() called on an InputSource that hasn't been opened");
		throw std::logic_error("Input source not open");
	}

	if(!buffer || count < 0) {
		os_log_debug(sLog, "Read() called with null buffer or invalid count");
		throw std::out_of_range("Null buffer or negative count");
	}

	return _Read(buffer, count);
}

bool SFB::InputSource::AtEOF() const
{
	if(!IsOpen()) {
		os_log_debug(sLog, "AtEOF() called on an InputSource that hasn't been opened");
		throw std::logic_error("Input source not open");
	}

	return _AtEOF();
}

int64_t SFB::InputSource::GetOffset() const
{
	if(!IsOpen()) {
		os_log_debug(sLog, "GetOffset() called on an InputSource that hasn't been opened");
		throw std::logic_error("Input source not open");
	}

	return _GetOffset();
}

int64_t SFB::InputSource::GetLength() const
{
	if(!IsOpen()) {
		os_log_debug(sLog, "GetLength() called on an InputSource that hasn't been opened");
		throw std::logic_error("Input source not open");
	}

	return _GetLength();
}

bool SFB::InputSource::SupportsSeeking() const noexcept
{
	return _SupportsSeeking();
}

void SFB::InputSource::SeekToOffset(int64_t offset, int whence)
{
	if(!IsOpen()) {
		os_log_debug(sLog, "SeekToOffset() called on an InputSource that hasn't been opened");
		throw std::logic_error("Input source not open");
	}

	if(!_SupportsSeeking()) {
		os_log_debug(sLog, "SeekToOffset() called on an InputSource that doesn't support seeking");
		throw std::logic_error("Seeking not supported");
	}

	return _SeekToOffset(offset, whence);
}
