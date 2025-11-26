//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <cstdlib>

#import "InputSource.hpp"

namespace SFB {

class FileContentsInput: public InputSource
{
public:
	explicit FileContentsInput(CFURLRef _Nonnull url);
	~FileContentsInput() noexcept;

	// This class is non-copyable.
	FileContentsInput(const FileContentsInput&) = delete;
	FileContentsInput(FileContentsInput&&) = delete;

	// This class is non-assignable.
	FileContentsInput& operator=(const FileContentsInput&) = delete;
	FileContentsInput& operator=(FileContentsInput&&) = delete;

private:
	void _Open() override;
	void _Close() noexcept override;

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;

	bool _AtEOF() const noexcept override
	{ return len_ == pos_; }

	int64_t _Offset() const noexcept override
	{ return pos_; }

	int64_t _Length() const noexcept override
	{ return len_; }

	bool _SupportsSeeking() const noexcept override
	{ return true; }

	void _SeekToOffset(int64_t offset, int whence) override;

	// Data members
	void * _Nullable buf_ {nullptr};
	int64_t len_ {0};
	int64_t pos_ {0};
};

} /* namespace SFB */
