//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <cstdio>
#import <system_error>

#import "InputSource.hpp"

namespace SFB {

class FileInput: public InputSource
{
public:
	explicit FileInput(CFURLRef _Nonnull url);
	~FileInput() noexcept;

	// This class is non-copyable.
	FileInput(const FileInput&) = delete;
	FileInput(FileInput&&) = delete;

	// This class is non-assignable.
	FileInput& operator=(const FileInput&) = delete;
	FileInput& operator=(FileInput&&) = delete;

private:
	bool _AtEOF() const noexcept override 				{ return std::feof(file_) != 0; }
	int64_t _Length() const noexcept override 			{ return len_; }
	bool _SupportsSeeking() const noexcept override 	{ return seekable_; }
	void _SeekToPosition(int64_t position) override 	{ if(::fseeko(file_, static_cast<off_t>(position), SEEK_SET)) throw std::system_error{errno, std::generic_category()}; }

	void _Open() override;
	void _Close() override;
	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	int64_t _Position() const override;
	CFStringRef _Nonnull _CopyDescription() const noexcept override;

	FILE * _Nullable file_ {nullptr};
	int64_t len_ {0};
	bool seekable_{false};
};

} /* namespace SFB */
