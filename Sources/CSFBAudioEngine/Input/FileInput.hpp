//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <cstdio>

#import "InputSource.hpp"
#import "scope_exit.hpp"

namespace SFB {

class FileInput: public InputSource
{
public:
	explicit FileInput(CFURLRef _Nonnull url) noexcept
	: InputSource(url) {}

	~FileInput() noexcept
	{ if(file_) std::fclose(file_); }

	// This class is non-copyable.
	FileInput(const FileInput& rhs) = delete;
	FileInput(FileInput&& rhs) = delete;

	// This class is non-assignable.
	FileInput& operator=(const FileInput& rhs) = delete;
	FileInput& operator=(FileInput&& rhs) = delete;

private:
	std::expected<void, int> _Open() noexcept override;

	std::expected<void, int> _Close() noexcept override
	{
		const auto defer = scope_exit{[this] noexcept { file_ = nullptr; }};
		if(std::fclose(file_)) return std::unexpected{errno};
		return {};
	}

	std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept override;

	std::expected<bool, int> _AtEOF() const noexcept override
	{ return std::feof(file_) != 0; }

	std::expected<int64_t, int> _GetOffset() const noexcept override
	{
		const auto offset = ::ftello(file_);
		if(offset == -1) return std::unexpected{errno};
		return offset;
	}

	std::expected<int64_t, int> _GetLength() const noexcept override
	{ return len_; }

	bool _SupportsSeeking() const noexcept override
	{ return true; }

	std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept override
	{
		if(::fseeko(file_, static_cast<off_t>(offset), whence))
			return std::unexpected{errno};
		return {};
	}

	// Data members
	FILE * _Nullable file_ {nullptr};
	int64_t len_ {0};
};

} /* namespace SFB */
