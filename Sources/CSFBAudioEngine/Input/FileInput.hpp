//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <cstdio>
#import <system_error>

#import "InputSource.hpp"
#import "scope_exit.hpp"

namespace SFB {

class FileInput: public InputSource
{
public:
	explicit FileInput(CFURLRef _Nonnull url)
	: InputSource(url)
	{ if(!url) throw std::invalid_argument("Null URL"); }

	~FileInput() noexcept
	{ if(file_) std::fclose(file_); }

	// This class is non-copyable.
	FileInput(const FileInput& rhs) = delete;
	FileInput(FileInput&& rhs) = delete;

	// This class is non-assignable.
	FileInput& operator=(const FileInput& rhs) = delete;
	FileInput& operator=(FileInput&& rhs) = delete;

private:
	void _Open() override;

	void _Close() override
	{
		const auto defer = scope_exit{[this]() noexcept { file_ = nullptr; }};
		if(std::fclose(file_))
			throw std::system_error{errno, std::generic_category()};
	}

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;

	bool _AtEOF() const noexcept override
	{ return std::feof(file_) != 0; }

	int64_t _GetOffset() const override
	{
		const auto offset = ::ftello(file_);
		if(offset == -1)
			throw std::system_error{errno, std::generic_category()};
		return offset;
	}

	int64_t _GetLength() const noexcept override
	{ return len_; }

	bool _SupportsSeeking() const noexcept override
	{ return true; }

	void _SeekToOffset(int64_t offset, int whence) override
	{
		if(::fseeko(file_, static_cast<off_t>(offset), whence))
			throw std::system_error{errno, std::generic_category()};
	}

	// Data members
	FILE * _Nullable file_ {nullptr};
	int64_t len_ {0};
};

} /* namespace SFB */
