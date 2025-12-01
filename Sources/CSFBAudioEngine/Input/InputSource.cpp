//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <limits>
#import <stdexcept>

#import "InputSource.hpp"
#import "scope_exit.hpp"

#import "BufferInput.hpp"
#import "DataInput.hpp"
#import "FileContentsInput.hpp"
#import "FileInput.hpp"
#import "MemoryMappedFileInput.hpp"

namespace SFB {

const os_log_t InputSource::sLog = os_log_create("org.sbooth.AudioEngine", "InputSource");

} /* namespace SFB */

SFB::InputSource::unique_ptr SFB::InputSource::CreateForURL(CFURLRef url, FileReadMode mode)
{
	switch(mode) {
		case FileReadMode::normal: 			return std::make_unique<FileInput>(url);
		case FileReadMode::memoryMap: 		return std::make_unique<MemoryMappedFileInput>(url);
		case FileReadMode::loadInMemory: 	return std::make_unique<FileContentsInput>(url);
	}
}

SFB::InputSource::unique_ptr SFB::InputSource::CreateWithData(CFDataRef data)
{
	return std::make_unique<DataInput>(data);
}

SFB::InputSource::unique_ptr SFB::InputSource::CreateWithBytes(const void *buf, int64_t len)
{
	return std::make_unique<BufferInput>(buf, len, BufferInput::BufferAdoption::copy);
}

SFB::InputSource::unique_ptr SFB::InputSource::CreateWithBytesNoCopy(const void *buf, int64_t len, bool free)
{
	return std::make_unique<BufferInput>(buf, len, free ? BufferInput::BufferAdoption::noCopyAndFree : BufferInput::BufferAdoption::noCopy);
}

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

CFDataRef SFB::InputSource::CopyData(int64_t count)
{
	if(!IsOpen()) {
		os_log_error(sLog, "CopyData() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	if(count < 0 || count > std::numeric_limits<CFIndex>::max()) {
		os_log_error(sLog, "CopyData() called on <InputSource: %p> with invalid count", this);
		throw std::invalid_argument("Invalid count");
	}

	if(count == 0)
		return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, nullptr, 0, kCFAllocatorNull);

	void *buf = std::malloc(count);
	if(!buf)
		throw std::bad_alloc();

	try {
		const auto read = _Read(buf, count);
		auto data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, static_cast<UInt8 *>(buf), read, kCFAllocatorMalloc);
		if(!data)
			std::free(buf);
		return data;
	} catch(...) {
		std::free(buf);
		throw;
	}
}

std::vector<uint8_t> SFB::InputSource::ReadBlock(std::vector<uint8_t>::size_type count)
{
	if(!IsOpen()) {
		os_log_error(sLog, "ReadBlock() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	if(count == 0)
		return {};

	std::vector<uint8_t> vec;
	vec.reserve(count);
	vec.resize(_Read(vec.data(), vec.capacity()));
	return vec;
}

bool SFB::InputSource::AtEOF() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "AtEOF() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _AtEOF();
}

int64_t SFB::InputSource::Position() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "Position() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _Position();
}

int64_t SFB::InputSource::Length() const
{
	if(!IsOpen()) {
		os_log_error(sLog, "Length() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

	return _Length();
}

bool SFB::InputSource::SupportsSeeking() const noexcept
{
	if(!IsOpen()) {
		os_log_error(sLog, "SupportsSeeking() called on <InputSource: %p> that hasn't been opened", this);
		throw std::logic_error("Input source not open");
	}

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

	const auto len = _Length();

	switch(whence) {
		case SeekAnchor::start:
			/* unchanged */
			break;

		case SeekAnchor::current:
			offset += _Position();
			break;

		case SeekAnchor::end:
			offset += len;
			break;
	}

	if(offset < 0 || offset > len) {
		os_log_error(sLog, "SeekToOffset() called on <InputSource: %p> with invalid position %lld", this, offset);
		throw std::out_of_range("Invalid seek position");
	}

	return _SeekToPosition(offset);
}

CFStringRef SFB::InputSource::CopyDescription() const noexcept
{
	return _CopyDescription();
}
