//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "BufferInput.hpp"

namespace SFB {

class MemoryMappedFileInput: public BufferInput
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
	void _Open() override;
	void _Close() override;
	CFStringRef _Nonnull _CopyDescription() const noexcept override;
};

} /* namespace SFB */
