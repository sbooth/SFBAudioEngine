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
	explicit DataInput(CFDataRef _Nonnull data);
	~DataInput() noexcept;

	// This class is non-copyable.
	DataInput(const DataInput&) = delete;
	DataInput(DataInput&&) = delete;

	// This class is non-assignable.
	DataInput& operator=(const DataInput&) = delete;
	DataInput& operator=(DataInput&&) = delete;

private:
	void _Open() noexcept override 						{ pos_ = 0; }
	void _Close() noexcept override 					{}
	bool _AtEOF() const noexcept override 				{ return CFDataGetLength(data_) == pos_; }
	int64_t _Offset() const noexcept override 			{ return pos_; }
	int64_t _Length() const noexcept override 			{ return CFDataGetLength(data_); }
	bool _SupportsSeeking() const noexcept override 	{ return true; }

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	void _SeekToOffset(int64_t offset, int whence) override;

	CFDataRef _Nonnull data_ {nullptr};
	CFIndex pos_ {0};
};

} /* namespace SFB */
