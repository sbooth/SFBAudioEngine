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

SFB::InputSource::~InputSource() noexcept
{
	if(url_)
		CFRelease(url_);
}

void SFB::InputSource::Open()
{
	if(IsOpen()) {
		os_log_debug(sLog, "Open() called on <InputSource: %p> that is already open", this);
		return;
	}

	_Open();
	isOpen_ = true;
}

void SFB::InputSource::Close()
{
	if(!IsOpen()) {
		os_log_debug(sLog, "Close() called on <InputSource: %p> that hasn't been opened", this);
		return;
	}

	const auto defer = scope_exit{[this]() noexcept { isOpen_ = false; }};
	_Close();
}

int64_t SFB::InputSource::Read(void *buffer, int64_t count)
{
	if(!IsOpen()) {
		os_log_error(sLog, "Read() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	if(!buffer || count < 0) {
		os_log_error(sLog, "Read() called on <InputSource: %p> with null buffer or invalid count", this);
		throw std::invalid_argument("Null buffer or negative count");
	}

	return _Read(buffer, count);
}

CFDataRef SFB::InputSource::CopyDataWithLength(int64_t length)
{
	if(!IsOpen()) {
		os_log_error(sLog, "CopyDataOfLength() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	if(length < 0 || length > LONG_MAX) {
		os_log_error(sLog, "CopyDataOfLength() called on <InputSource: %p> with invalid length", this);
		throw std::invalid_argument("Invalid length");
	}

	if(length == 0)
		return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, nullptr, 0, kCFAllocatorNull);

	void *buf = std::malloc(length);
	if(!buf)
		throw std::bad_alloc();

	try {
		const auto read = _Read(buf, length);
		auto data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, static_cast<UInt8 *>(buf), read, kCFAllocatorMalloc);
		if(!data)
			std::free(buf);
		return data;
	}
	catch(...) {
		std::free(buf);
		throw;
	}
}

bool SFB::InputSource::AtEOF() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "AtEOF() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _AtEOF();
}

int64_t SFB::InputSource::Offset() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "GetOffset() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _Offset();
}

int64_t SFB::InputSource::Length() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "GetLength() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _Length();
}

bool SFB::InputSource::SupportsSeeking() const noexcept
{
	return _SupportsSeeking();
}

void SFB::InputSource::SeekToOffset(int64_t offset, SeekAnchor whence)
{
	if(!IsOpen()) {
		os_log_error(sLog, "SeekToOffset() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	if(!_SupportsSeeking()) {
		os_log_error(sLog, "SeekToOffset() called on <InputSource: %p> that doesn't support seeking", this);
		throw std::logic_error("Seeking not supported");
	}

	return _SeekToOffset(offset, whence);
}

CFStringRef SFB::InputSource::CopyDescription() const noexcept
{
	return _CopyDescription();
}
