//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <cstdio>
#import <system_error>

#import <sys/mman.h>
#import <sys/stat.h>

#import "MemoryMappedFileInput.hpp"
#import "scope_exit.hpp"

SFB::MemoryMappedFileInput::MemoryMappedFileInput(CFURLRef url)
: InputSource(url)
{
	if(!url) {
		os_log_error(sLog, "Cannot create MemoryMappedFileInput with null URL");
		throw std::invalid_argument("Null URL");
	}
}

SFB::MemoryMappedFileInput::~MemoryMappedFileInput() noexcept
{
	if(region_)
		munmap(region_, len_);
}

void SFB::MemoryMappedFileInput::_Open()
{
	CFURLRef url = GetURL();

	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(url, FALSE, path, PATH_MAX);
	if(!success)
		throw std::runtime_error("Unable to get URL file system representation");

	auto file = std::fopen(reinterpret_cast<const char *>(path), "r");
	if(!file)
		throw std::system_error{errno, std::generic_category()};

	// Ensure the file is closed
	const auto guard = scope_exit{[&file]() noexcept { std::fclose(file); }};

	auto fd = ::fileno(file);

	struct stat s;
	if(::fstat(fd, &s))
		throw std::system_error{errno, std::generic_category()};

	// Only regular files can be mapped
	if(!S_ISREG(s.st_mode))
		throw std::system_error{ENOTSUP, std::generic_category()};

	// Map the file to memory
	auto region = mmap(nullptr, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(region == MAP_FAILED)
		throw std::system_error{errno, std::generic_category()};

	region_ = region;
	len_ = s.st_size;
	pos_ = 0;
}

void SFB::MemoryMappedFileInput::_Close()
{
	const auto defer = scope_exit{[this]() noexcept { region_ = nullptr; }};
	if(munmap(region_, len_))
		throw std::system_error{errno, std::generic_category()};
}

int64_t SFB::MemoryMappedFileInput::_Read(void *buffer, int64_t count)
{
	if(count > SIZE_T_MAX) {
		os_log_error(sLog, "_Read() called on <MemoryMappedFileInput: %p> with count greater than maximum allowable value", this);
		throw std::invalid_argument("Count greater than maximum allowable value");
	}
	const auto remaining = len_ - pos_;
	count = std::min(count, remaining);
	memcpy(buffer, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(region_) + pos_), count);
	pos_ += count;
	return count;
}

void SFB::MemoryMappedFileInput::_SeekToOffset(int64_t offset, int whence)
{
	switch(whence) {
		case SEEK_SET:
			break;
		case SEEK_CUR:
			offset += pos_;
			break;
		case SEEK_END:
			offset += len_;
			break;
		default:
			os_log_error(sLog, "_SeekToOffset() called on <MemoryMappedFileInput: %p> with unknown whence %d", this, whence);
			throw std::invalid_argument("Unknown whence");
	}

	if(offset < 0 || offset > len_) {
		os_log_error(sLog, "_SeekToOffset() called on <MemoryMappedFileInput: %p> with invalid seek offset %lld", this, offset);
		throw std::out_of_range("Invalid seek offset");
	}

	pos_ = offset;
}

CFStringRef SFB::MemoryMappedFileInput::_CopyDescription() const noexcept
{
	CFStringRef lastPathComponent = CFURLCopyLastPathComponent(GetURL());
	const auto guard = scope_exit{[&lastPathComponent]() noexcept { CFRelease(lastPathComponent); }};
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<MemoryMappedFileInput %p: %lld bytes mapped at %p from \"%{public}@\">"), this, len_, region_, lastPathComponent);
}
