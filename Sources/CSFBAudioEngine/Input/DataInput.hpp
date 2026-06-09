//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
	int64_t _Position() const noexcept override 		{ return pos_; }
	int64_t _Length() const noexcept override 			{ return CFDataGetLength(data_); }
	bool _SupportsSeeking() const noexcept override 	{ return true; }
	void _SeekToPosition(int64_t position) override 	{ pos_ = position; }

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	CFStringRef _Nonnull _CopyDescription() const noexcept override;

	CFDataRef _Nonnull data_ {nullptr};
	CFIndex pos_ {0};
};

} /* namespace SFB */
