//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <expected>
#import <memory>

#import <os/log.h>

#import <CoreFoundation/CoreFoundation.h>

namespace SFB {

class InputSource
{
public:
	/// The shared log for all `InputSource` instances
	static const os_log_t _Nonnull sLog;

	using unique_ptr = std::unique_ptr<InputSource>;

	virtual ~InputSource() noexcept;

	// This class is non-copyable.
	InputSource(const InputSource& rhs) = delete;
	InputSource(InputSource&& rhs) = delete;

	// This class is non-assignable.
	InputSource& operator=(const InputSource& rhs) = delete;
	InputSource& operator=(InputSource&& rhs) = delete;

	CFURLRef _Nullable GetURL() const noexcept;

	// Opening and closing
	std::expected<void, int> Open() noexcept;
	std::expected<void, int> Close() noexcept;

	bool IsOpen() const noexcept;

	// Reading
	std::expected<int64_t, int> Read(void * _Nonnull buffer, int64_t count) noexcept;

	std::expected<bool, int> AtEOF() const noexcept;

	std::expected<int64_t, int> GetOffset() const noexcept;
	std::expected<int64_t, int> GetLength() const noexcept;

	// Seeking
	bool SupportsSeeking() const noexcept;
	std::expected<void, int> SeekToOffset(int64_t offset, int whence) noexcept;

protected:

	explicit InputSource() noexcept = default;
	explicit InputSource(CFURLRef _Nullable url) noexcept;

private:

	// Subclasses must implement the following methods
	virtual std::expected<void, int> _Open() noexcept = 0;
	virtual std::expected<void, int> _Close() noexcept = 0;
	virtual std::expected<int64_t, int> _Read(void * _Nonnull buffer, int64_t count) noexcept = 0;
	virtual std::expected<bool, int> _AtEOF() const noexcept = 0;
	virtual std::expected<int64_t, int> _GetOffset() const noexcept = 0;
	virtual std::expected<int64_t, int> _GetLength() const noexcept = 0;

	// Optional seeking support
	virtual bool _SupportsSeeking() const noexcept;
	virtual std::expected<void, int> _SeekToOffset(int64_t offset, int whence) noexcept;

	CFURLRef _Nullable url_ {nullptr};
	bool isOpen_ {false};
};

} /* namespace SFB */
