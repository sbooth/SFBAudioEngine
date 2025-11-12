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
	explicit DataInput(CFDataRef _Nonnull data) noexcept
	: data_(data) { if(data_) CFRetain(data_); }

	~DataInput() noexcept
	{ if(data_) CFRelease(data_); }

	// This class is non-copyable.
	DataInput(const DataInput& rhs) = delete;
	DataInput(DataInput&& rhs) = delete;

	// This class is non-assignable.
	DataInput& operator=(const DataInput& rhs) = delete;
	DataInput& operator=(DataInput&& rhs) = delete;

private:
	std::expected<void, int> _Open() noexcept override
	{
		if(!data_)
			return std::unexpected{ENOENT};
		pos_ = 0;
		return {};
	}

	std::expected<void, int> _Close() noexcept override
	{ return {}; }

	std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept override;

	std::expected<bool, int> _AtEOF() const noexcept override
	{ return CFDataGetLength(data_) == pos_; }

	std::expected<int64_t, int> _GetOffset() const noexcept override
	{ return pos_; }

	std::expected<int64_t, int> _GetLength() const noexcept override
	{ return CFDataGetLength(data_); }

	bool _SupportsSeeking() const noexcept override
	{ return true; }

	std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept override;

	// Data members
	CFDataRef _Nullable data_ {nullptr};
	CFIndex pos_ {0};
};

} /* namespace SFB */
