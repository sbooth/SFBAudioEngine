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
	virtual ~DataInput();

	// This class is non-copyable.
	DataInput(const DataInput& rhs) = delete;
	DataInput(DataInput&& rhs) = delete;

	// This class is non-assignable.
	DataInput& operator=(const DataInput& rhs) = delete;
	DataInput& operator=(DataInput&& rhs) = delete;

private:
	virtual std::expected<void, int> _Open() noexcept;
	virtual std::expected<void, int> _Close() noexcept;
	virtual std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept;
	virtual std::expected<bool, int> _AtEOF() const noexcept;
	virtual std::expected<int64_t, int> _GetOffset() const noexcept;
	virtual std::expected<int64_t, int> _GetLength() const noexcept;
	virtual bool _SupportsSeeking() const noexcept;
	virtual std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept;

	CFDataRef _Nullable data_ = nullptr;
	CFIndex pos_ = 0;
};

} /* namespace SFB */
