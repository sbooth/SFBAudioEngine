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
{
	if(!url) {
		os_log_error(sLog, "Cannot create MemoryMappedFileInput with null URL");
		throw std::invalid_argument("Null URL");
	}
	url_ = static_cast<CFURLRef>(CFRetain(url));
	free_ = false;
}

SFB::MemoryMappedFileInput::~MemoryMappedFileInput() noexcept
{
	if(buf_)
		munmap(buf_, len_);
}

void SFB::MemoryMappedFileInput::_Open()
{
	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(url_, FALSE, path, PATH_MAX);
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

	buf_ = region;
	len_ = s.st_size;
	pos_ = 0;
}

void SFB::MemoryMappedFileInput::_Close()
{
	const auto defer = scope_exit{[this]() noexcept { buf_ = nullptr; }};
	if(munmap(buf_, len_))
		throw std::system_error{errno, std::generic_category()};
}

CFStringRef SFB::MemoryMappedFileInput::_CopyDescription() const noexcept
{
	CFStringRef lastPathComponent = CFURLCopyLastPathComponent(url_);
	const auto guard = scope_exit{[&lastPathComponent]() noexcept { CFRelease(lastPathComponent); }};
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<MemoryMappedFileInput %p: %lld bytes mapped at %p from \"%@\">"), this, len_, buf_, lastPathComponent);
}
