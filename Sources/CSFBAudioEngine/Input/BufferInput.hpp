//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "InputSource.hpp"

namespace SFB {

class BufferInput: public InputSource
{
public:
	enum class BufferAdoption { copy, noCopy, noCopyAndFree };
	BufferInput(const void * _Nonnull buf, int64_t len, BufferAdoption owned = BufferAdoption::copy);
	~BufferInput() noexcept;

	// This class is non-copyable.
	BufferInput(const BufferInput&) = delete;
	BufferInput(BufferInput&&) = delete;

	// This class is non-assignable.
	BufferInput& operator=(const BufferInput&) = delete;
	BufferInput& operator=(BufferInput&&) = delete;

private:
	void _Open() noexcept override 						{ pos_ = 0; }
	void _Close() noexcept override 					{}
	bool _AtEOF() const noexcept override  				{ return len_ == pos_; }
	int64_t _Offset() const noexcept override 			{ return pos_; }
	int64_t _Length() const noexcept override 			{ return len_; }
	bool _SupportsSeeking() const noexcept override 	{ return true; }

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	void _SeekToOffset(int64_t offset, int whence) override;

	void * _Nonnull buf_ {nullptr};
	bool free_ {false};
	int64_t len_ {0};
	int64_t pos_ {0};
};

} /* namespace SFB */
