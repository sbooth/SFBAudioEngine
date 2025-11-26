//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <system_error>

#import "InputSource.hpp"

namespace SFB {

class MemoryMappedFileInput: public InputSource
{
public:
	explicit MemoryMappedFileInput(CFURLRef _Nonnull url);
	~MemoryMappedFileInput() noexcept;

	// This class is non-copyable.
	MemoryMappedFileInput(const MemoryMappedFileInput&) = delete;
	MemoryMappedFileInput(MemoryMappedFileInput&&) = delete;

	// This class is non-assignable.
	MemoryMappedFileInput& operator=(const MemoryMappedFileInput&) = delete;
	MemoryMappedFileInput& operator=(MemoryMappedFileInput&&) = delete;

private:
	bool _AtEOF() const noexcept override 				{ return len_ == pos_; }
	int64_t _Offset() const noexcept override 			{ return pos_; }
	int64_t _Length() const noexcept override 			{ return len_; }
	bool _SupportsSeeking() const noexcept override 	{ return true; }

	void _Open() override;
	void _Close() override;
	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	void _SeekToOffset(int64_t offset, int whence) override;

	void * _Nullable region_ {nullptr};
	int64_t len_ {0};
	int64_t pos_ {0};
};

} /* namespace SFB */
