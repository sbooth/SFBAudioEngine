//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "BufferInput.hpp"

namespace SFB {

class FileContentsInput: public BufferInput
{
public:
	explicit FileContentsInput(CFURLRef _Nonnull url);
	~FileContentsInput() noexcept = default;

	// This class is non-copyable.
	FileContentsInput(const FileContentsInput&) = delete;
	FileContentsInput(FileContentsInput&&) = delete;

	// This class is non-assignable.
	FileContentsInput& operator=(const FileContentsInput&) = delete;
	FileContentsInput& operator=(FileContentsInput&&) = delete;

private:
	void _Open() override;
	void _Close() noexcept override;
	CFStringRef _Nonnull _CopyDescription() const noexcept override;
};

} /* namespace SFB */
