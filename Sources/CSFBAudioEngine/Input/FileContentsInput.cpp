//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <cstdio>

#import <sys/stat.h>

#import "FileContentsInput.hpp"
#import "scope_exit.hpp"

std::expected<void, int> SFB::FileContentsInput::_Open() noexcept
{
	CFURLRef url = GetURL();
	if(!url)
		return std::unexpected{ENOENT};

	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(url, FALSE, path, PATH_MAX);
	if(!success)
		return std::unexpected{EIO};

	auto file = std::fopen(reinterpret_cast<const char *>(path), "r");
	if(!file)
		return std::unexpected{errno};

	// Ensure the file is closed
	const auto guard = scope_exit{[&file] noexcept { std::fclose(file); }};

	auto fd = ::fileno(file);

	struct stat s;
	if(::fstat(fd, &s))
		return std::unexpected{errno};

	buf_ = std::malloc(s.st_size);
	if(!buf_)
		return std::unexpected{ENOMEM};

	const auto nitems = std::fread(buf_, 1, s.st_size, file);
	if(nitems != s.st_size && std::ferror(file))
		return std::unexpected{errno};

	len_ = nitems;
	pos_ = 0;

	return {};
}

std::expected<int64_t, int> SFB::FileContentsInput::_Read(void *buffer, int64_t count) noexcept
{
	if(count > SIZE_T_MAX)
		return std::unexpected{EINVAL};
	const auto remaining = len_ - pos_;
	count = std::min(count, remaining);
	memcpy(buffer, reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(buf_) + pos_), count);
	pos_ += count;
	return count;
}

std::expected<void, int> SFB::FileContentsInput::_SeekToOffset(int64_t offset, int whence) noexcept
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
			return std::unexpected{EINVAL};
	}

	if(offset < 0 || offset > len_)
		return std::unexpected{EINVAL};

	pos_ = offset;
	return {};
}

