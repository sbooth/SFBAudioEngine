//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <cstdio>
#import <system_error>

#import <sys/stat.h>

#import "FileContentsInput.hpp"
#import "scope_exit.hpp"

SFB::FileContentsInput::FileContentsInput(CFURLRef url)
: InputSource(url)
{
	if(!url) {
		os_log_error(sLog, "Cannot create FileContentsInput with null CFURL");
		throw std::invalid_argument("Null URL");
	}
}

void SFB::FileContentsInput::_Open()
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

	buf_ = std::malloc(s.st_size);
	if(!buf_)
		throw std::bad_alloc();

	const auto nitems = std::fread(buf_, 1, s.st_size, file);
	if(nitems != s.st_size && std::ferror(file))
		throw std::system_error{errno, std::generic_category()};

	len_ = nitems;
	pos_ = 0;
}

int64_t SFB::FileContentsInput::_Read(void *buffer, int64_t count)
{
	if(count > SIZE_T_MAX) {
		os_log_error(sLog, "_Read() called on <FileContentsInput: %p> with count greater than maximum allowable value", this);
		throw std::invalid_argument("Count greater than maximum allowable value");
	}
	const auto remaining = len_ - pos_;
	count = std::min(count, remaining);
	memcpy(buffer, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(buf_) + pos_), count);
	pos_ += count;
	return count;
}

void SFB::FileContentsInput::_SeekToOffset(int64_t offset, int whence)
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
			os_log_error(sLog, "_SeekToOffset() called on <FileContentsInput: %p> with unknown whence %d", this, whence);
			throw std::invalid_argument("Unknown whence");
	}

	if(offset < 0 || offset > len_) {
		os_log_error(sLog, "_SeekToOffset() called on <FileContentsInput: %p> with invalid seek offset %lld", this, offset);
		throw std::out_of_range("Invalid seek offset");
	}

	pos_ = offset;
}

