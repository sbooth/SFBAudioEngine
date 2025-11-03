//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "InputSource.hpp"

namespace SFB {

class FileContentsInput: public InputSource
{
public:
	explicit FileContentsInput(CFURLRef _Nonnull url) noexcept;
	virtual ~FileContentsInput() = default;

	// This class is non-copyable.
	FileContentsInput(const FileContentsInput& rhs) = delete;
	FileContentsInput(FileContentsInput&& rhs) = delete;

	// This class is non-assignable.
	FileContentsInput& operator=(const FileContentsInput& rhs) = delete;
	FileContentsInput& operator=(FileContentsInput&& rhs) = delete;

private:
	virtual std::expected<void, int> _Open() noexcept;
	virtual std::expected<void, int> _Close() noexcept;
	virtual std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept;
	virtual std::expected<bool, int> _AtEOF() const noexcept;
	virtual std::expected<int64_t, int> _GetOffset() const noexcept;
	virtual std::expected<int64_t, int> _GetLength() const noexcept;
	virtual bool _SupportsSeeking() const noexcept;
	virtual std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept;

	void * _Nullable buf_ = nullptr;
	int64_t len_ = 0;
	int64_t pos_ = 0;
};

} /* namespace SFB */
