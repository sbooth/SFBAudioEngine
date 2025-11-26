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
	bool _SupportsSeeking() const noexcept override 	{ return true; }

	void _Open() override;
	void _Close() override;
	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	int64_t _Offset() const override;
	void _SeekToOffset(int64_t offset, int whence) override;

	FILE * _Nullable file_ {nullptr};
	int64_t len_ {0};
};

} /* namespace SFB */
