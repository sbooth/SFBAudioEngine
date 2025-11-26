//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <cstdio>
#import <system_error>

#import <sys/stat.h>

#import "FileInput.hpp"
#import "scope_exit.hpp"

SFB::FileInput::FileInput(CFURLRef url)
: InputSource(url)
{
	if(!url) {
		os_log_error(sLog, "Cannot create FileInput with null URL");
		throw std::invalid_argument("Null URL");
	}
}

SFB::FileInput::~FileInput() noexcept
{
	if(file_)
		std::fclose(file_);
}

void SFB::FileInput::_Open()
{
	UInt8 path [PATH_MAX];
	auto success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, path, PATH_MAX);
	if(!success)
		throw std::runtime_error("Unable to get URL file system representation");

	file_ = std::fopen(reinterpret_cast<const char *>(path), "r");
	if(!file_)
		throw std::system_error{errno, std::generic_category()};

	struct stat s;
	if(::fstat(::fileno(file_), &s)) {
		std::fclose(file_);
		file_ = nullptr;
		throw std::system_error{errno, std::generic_category()};
	}

	len_ = s.st_size;
}

void SFB::FileInput::_Close()
{
	const auto defer = scope_exit{[this]() noexcept { file_ = nullptr; }};
	if(std::fclose(file_))
		throw std::system_error{errno, std::generic_category()};
}

int64_t SFB::FileInput::_Read(void *buffer, int64_t count)
{
	const auto nitems = std::fread(buffer, 1, count, file_);
	if(nitems != count && std::ferror(file_))
		throw std::system_error{errno, std::generic_category()};
	return nitems;
}

int64_t SFB::FileInput::_Offset() const
{
	const auto offset = ::ftello(file_);
	if(offset == -1)
		throw std::system_error{errno, std::generic_category()};
	return offset;
}

void SFB::FileInput::_SeekToOffset(int64_t offset, SeekAnchor whence)
{
	int _whence;
	switch(whence) {
		case SeekAnchor::start: 	_whence = SEEK_SET; 	break;
		case SeekAnchor::current: 	_whence = SEEK_CUR; 	break;
		case SeekAnchor::end:		_whence = SEEK_END; 	break;
	}
	if(::fseeko(file_, static_cast<off_t>(offset), _whence))
		throw std::system_error{errno, std::generic_category()};
}

CFStringRef SFB::FileInput::_CopyDescription() const noexcept
{
	CFStringRef lastPathComponent = CFURLCopyLastPathComponent(GetURL());
	const auto guard = scope_exit{[&lastPathComponent]() noexcept { CFRelease(lastPathComponent); }};
	return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<FileInput %p: \"%@\">"), this, lastPathComponent);
}
