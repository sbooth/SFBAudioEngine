//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <system_error>

#import "InputSource.hpp"
#import "scope_exit.hpp"

namespace SFB {

class MemoryMappedFileInput: public InputSource
{
public:
	explicit MemoryMappedFileInput(CFURLRef _Nonnull url);

	~MemoryMappedFileInput() noexcept
	{ if(region_) munmap(region_, len_); }

	// This class is non-copyable.
	MemoryMappedFileInput(const MemoryMappedFileInput& rhs) = delete;
	MemoryMappedFileInput(MemoryMappedFileInput&& rhs) = delete;

	// This class is non-assignable.
	MemoryMappedFileInput& operator=(const MemoryMappedFileInput& rhs) = delete;
	MemoryMappedFileInput& operator=(MemoryMappedFileInput&& rhs) = delete;

private:
	void _Open() override;
	void _Close() override
	{
		const auto defer = scope_exit{[this]() noexcept { region_ = nullptr; }};
		if(munmap(region_, len_))
			throw std::system_error{errno, std::generic_category()};
	}

	int64_t _Read(void * _Nonnull buffer, int64_t count) override;

	bool _AtEOF() const noexcept override
	{ return len_ == pos_; }

	int64_t _Offset() const noexcept override
	{ return pos_; }

	int64_t _Length() const noexcept override
	{ return len_; }

	bool _SupportsSeeking() const noexcept override
	{ return true; }

	void _SeekToOffset(int64_t offset, int whence) override;

	// Data members
	void * _Nullable region_ {nullptr};
	int64_t len_ {0};
	int64_t pos_ {0};
};

} /* namespace SFB */
