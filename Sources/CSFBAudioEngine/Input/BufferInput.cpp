//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <cstdio>
#import <cstdlib>
#import <system_error>

#import <sys/stat.h>

#import "BufferInput.hpp"

SFB::BufferInput::BufferInput(const void *buf, int64_t len, BufferAdoption behavior)
: buf_{const_cast<void *>(buf)}, free_{behavior == BufferAdoption::copy || behavior == BufferAdoption::noCopyAndFree}, len_{len}
{
	if(!buf || len < 0) {
		os_log_error(sLog, "Cannot create BufferInput with null buffer or negative length");
		throw std::invalid_argument("Null buffer or negative length");
	}

	if(behavior == BufferAdoption::copy) {
		buf_ = std::malloc(len_);
		if(!buf_)
			throw std::bad_alloc();
		std::memcpy(buf_, buf, len_);
	}
}

SFB::BufferInput::~BufferInput() noexcept
{
	if(free_)
		std::free(buf_);
}

int64_t SFB::BufferInput::_Read(void *buffer, int64_t count)
{
	const auto remaining = len_ - pos_;
	count = std::min(count, remaining);
	memcpy(buffer, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(buf_) + pos_), count);
	pos_ += count;
	return count;
}

void SFB::BufferInput::_SeekToOffset(int64_t offset, SeekAnchor whence)
{
	switch(whence) {
#if false
		case SeekAnchor::start: 	/* unchanged */		break;
#endif
		case SeekAnchor::current: 	offset += pos_; 	break;
		case SeekAnchor::end:		offset += len_; 	break;
	}

	if(offset < 0 || offset > len_) {
		os_log_error(sLog, "_SeekToOffset() called on <BufferInput: %p> with invalid seek offset %lld", this, offset);
		throw std::out_of_range("Invalid seek offset");
	}

	pos_ = offset;
}

CFStringRef SFB::BufferInput::_CopyDescription() const noexcept
{
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<BufferInput %p: %lld bytes at %p>"), this, len_, buf_);
}
