//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <cstdio>

#import "InputSource.hpp"

namespace SFB {

class FileInput: public InputSource
{
public:
	explicit FileInput(CFURLRef _Nonnull url) noexcept;
	virtual ~FileInput();

	// This class is non-copyable.
	FileInput(const FileInput& rhs) = delete;
	FileInput(FileInput&& rhs) = delete;

	// This class is non-assignable.
	FileInput& operator=(const FileInput& rhs) = delete;
	FileInput& operator=(FileInput&& rhs) = delete;

private:
	virtual std::expected<void, int> _Open() noexcept;
	virtual std::expected<void, int> _Close() noexcept;
	virtual std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept;
	virtual std::expected<bool, int> _AtEOF() const noexcept;
	virtual std::expected<int64_t, int> _GetOffset() const noexcept;
	virtual std::expected<int64_t, int> _GetLength() const noexcept;
	virtual bool _SupportsSeeking() const noexcept;
	virtual std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept;

	FILE * _Nullable file_ = nullptr;
	int64_t len_ = 0;
};

} /* namespace SFB */
