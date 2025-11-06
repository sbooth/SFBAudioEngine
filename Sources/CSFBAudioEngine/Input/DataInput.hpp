//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "InputSource.hpp"

namespace SFB {

class DataInput: public InputSource
{
public:
	explicit DataInput(CFDataRef _Nonnull data) noexcept;
	~DataInput() noexcept;

	// This class is non-copyable.
	DataInput(const DataInput& rhs) = delete;
	DataInput(DataInput&& rhs) = delete;

	// This class is non-assignable.
	DataInput& operator=(const DataInput& rhs) = delete;
	DataInput& operator=(DataInput&& rhs) = delete;

private:
	std::expected<void, int> _Open() noexcept override;
	std::expected<void, int> _Close() noexcept override;
	std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept override;
	std::expected<bool, int> _AtEOF() const noexcept override;
	std::expected<int64_t, int> _GetOffset() const noexcept override;
	std::expected<int64_t, int> _GetLength() const noexcept override;
	bool _SupportsSeeking() const noexcept override;
	std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept override;

	CFDataRef _Nullable data_ {nullptr};
	CFIndex pos_ {0};
};

} /* namespace SFB */
