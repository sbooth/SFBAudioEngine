//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <sys/stat.h>

#import "FileInput.hpp"
#import "scope_exit.hpp"

SFB::FileInput::FileInput(CFURLRef url) noexcept
: InputSource(url)
{}

std::expected<void, int> SFB::FileInput::_Open() noexcept
{
	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, path, PATH_MAX);
	if(!success)
		return std::unexpected(EIO);

	file_ = std::fopen(reinterpret_cast<const char *>(path), "r");
	if(!file_)
		return std::unexpected(errno);

	struct stat s;
	if(::fstat(::fileno(file_), &s)) {
		std::fclose(file_);
		file_ = nullptr;
		return std::unexpected(errno);
	}

	len_ = s.st_size;

	return {};
}

std::expected<void, int> SFB::FileInput::_Close() noexcept
{
	auto defer = scope_exit{[&] noexcept {
		file_ = nullptr;
		len_ = 0;
	}};

	if(std::fclose(file_))
		return std::unexpected(errno);
	return {};
}

std::expected<int64_t, int> SFB::FileInput::_Read(void *buffer, int64_t count) noexcept
{
	auto nitems = std::fread(buffer, 1, count, file_);
	if(nitems != count && std::ferror(file_))
		return std::unexpected(errno);
	return nitems;
}

std::expected<bool, int> SFB::FileInput::_AtEOF() const noexcept
{
	return std::feof(file_) != 0;
}

std::expected<int64_t, int> SFB::FileInput::_GetOffset() const noexcept
{
	auto offset = std::ftell(file_);
	if(offset == -1)
		return std::unexpected(errno);
	return offset;
}

std::expected<int64_t, int> SFB::FileInput::_GetLength() const noexcept
{
	return len_;
}

bool SFB::FileInput::_SupportsSeeking() const noexcept
{
	return true;
}

std::expected<void, int> SFB::FileInput::_SeekToOffset(int64_t offset, int whence) noexcept
{
	if(std::fseek(file_, offset, whence))
		return std::unexpected(errno);
	return {};
}
