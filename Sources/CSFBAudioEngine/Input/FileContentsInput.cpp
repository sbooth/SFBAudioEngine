//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <cstdio>
#import <system_error>

#import <sys/stat.h>

#import "FileContentsInput.hpp"
#import "scope_exit.hpp"

SFB::FileContentsInput::FileContentsInput(CFURLRef url)
{
	if(!url) {
		os_log_error(sLog, "Cannot create FileContentsInput with null URL");
		throw std::invalid_argument("Null URL");
	}
	url_ = static_cast<CFURLRef>(CFRetain(url));
	free_ = true;
}

void SFB::FileContentsInput::_Open()
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

	buf_ = std::malloc(s.st_size);
	if(!buf_)
		throw std::bad_alloc();

	const auto nitems = std::fread(buf_, 1, s.st_size, file);
	if(nitems != s.st_size && std::ferror(file))
		throw std::system_error{errno, std::generic_category()};

	len_ = nitems;
	pos_ = 0;
}

void SFB::FileContentsInput::_Close() noexcept
{
	std::free(buf_);
	buf_ = nullptr;
}

CFStringRef SFB::FileContentsInput::_CopyDescription() const noexcept
{
	CFStringRef lastPathComponent = CFURLCopyLastPathComponent(url_);
	const auto guard = scope_exit{[&lastPathComponent]() noexcept { CFRelease(lastPathComponent); }};
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<FileContentsInput %p: %lld bytes copied to %p from \"%@\">"), this, len_, buf_, lastPathComponent);
}
