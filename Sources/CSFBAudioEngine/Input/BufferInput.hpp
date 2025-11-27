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
	/// Buffer adoption behaviors.
	enum class BufferAdoption { copy, noCopy, noCopyAndFree };
	BufferInput(const void * _Nonnull buf, int64_t len, BufferAdoption behavior = BufferAdoption::copy);
	~BufferInput() noexcept;

	// This class is non-copyable.
	BufferInput(const BufferInput&) = delete;
	BufferInput(BufferInput&&) = delete;

	// This class is non-assignable.
	BufferInput& operator=(const BufferInput&) = delete;
	BufferInput& operator=(BufferInput&&) = delete;

protected:
	explicit BufferInput() noexcept = default;

	/// The data buffer.
	void * _Nonnull buf_ {nullptr};
	/// Whether the buffer should be freed in the destructor.
	bool free_ {false};
	/// The length of the buffer in bytes.
	int64_t len_ {0};
	/// The current byte position in the buffer.
	int64_t pos_ {0};

private:
	void _Open() override 								{ pos_ = 0; }
	void _Close() override 								{}
	bool _AtEOF() const noexcept override  				{ return len_ == pos_; }
	int64_t _Offset() const noexcept override 			{ return pos_; }
	int64_t _Length() const noexcept override 			{ return len_; }
	bool _SupportsSeeking() const noexcept override 	{ return true; }
	void _SeekToOffset(int64_t offset) override 		{ pos_ = offset; }

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;
	CFStringRef _CopyDescription() const noexcept override;
};

} /* namespace SFB */
