//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <sys/stat.h>

#import "FileInput.hpp"

std::expected<void, int> SFB::FileInput::_Open() noexcept
{
	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, path, PATH_MAX);
	if(!success)
		return std::unexpected{EIO};

	file_ = std::fopen(reinterpret_cast<const char *>(path), "r");
	if(!file_)
		return std::unexpected{errno};

	struct stat s;
	if(::fstat(::fileno(file_), &s)) {
		std::fclose(file_);
		file_ = nullptr;
		return std::unexpected{errno};
	}

	len_ = s.st_size;

	return {};
}

std::expected<int64_t, int> SFB::FileInput::_Read(void *buffer, int64_t count) noexcept
{
	if(count > SIZE_T_MAX)
		return std::unexpected{EINVAL};
	const auto nitems = std::fread(buffer, 1, count, file_);
	if(nitems != count && std::ferror(file_))
		return std::unexpected{errno};
	return nitems;
}
