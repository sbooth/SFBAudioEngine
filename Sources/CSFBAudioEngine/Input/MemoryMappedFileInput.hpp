//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "InputSource.hpp"

namespace SFB {

class MemoryMappedFileInput: public InputSource
{
public:
	explicit MemoryMappedFileInput(CFURLRef _Nonnull url) noexcept;
	~MemoryMappedFileInput() noexcept;

	// This class is non-copyable.
	MemoryMappedFileInput(const MemoryMappedFileInput& rhs) = delete;
	MemoryMappedFileInput(MemoryMappedFileInput&& rhs) = delete;

	// This class is non-assignable.
	MemoryMappedFileInput& operator=(const MemoryMappedFileInput& rhs) = delete;
	MemoryMappedFileInput& operator=(MemoryMappedFileInput&& rhs) = delete;

private:
	std::expected<void, int> _Open() noexcept override;
	std::expected<void, int> _Close() noexcept override;
	std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept override;
	std::expected<bool, int> _AtEOF() const noexcept override;
	std::expected<int64_t, int> _GetOffset() const noexcept override;
	std::expected<int64_t, int> _GetLength() const noexcept override;
	bool _SupportsSeeking() const noexcept override;
	std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept override;

	void * _Nullable region_ = nullptr;
	int64_t len_ = 0;
	int64_t pos_ = 0;
};

} /* namespace SFB */
