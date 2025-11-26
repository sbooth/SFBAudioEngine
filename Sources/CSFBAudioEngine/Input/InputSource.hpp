//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <memory>
#import <stdexcept>

#import <os/log.h>

#import <CoreFoundation/CoreFoundation.h>

namespace SFB {

class InputSource
{
public:
	using unique_ptr = std::unique_ptr<InputSource>;

	virtual ~InputSource() noexcept;

	// This class is non-copyable.
	InputSource(const InputSource&) = delete;
	InputSource(InputSource&&) = delete;

	// This class is non-assignable.
	InputSource& operator=(const InputSource&) = delete;
	InputSource& operator=(InputSource&&) = delete;

	CFURLRef _Nullable GetURL() const noexcept 	{ return url_; }

	// Opening and closing
	void Open();
	void Close();
	bool IsOpen() const noexcept 	{ return isOpen_; }

	// Reading
	int64_t Read(void * _Nonnull buffer, int64_t count);
	CFDataRef _Nullable CopyDataWithLength(int64_t length);

	// Position
	bool AtEOF() const;
	int64_t Offset() const;
	int64_t Length() const;

	// Seeking
	bool SupportsSeeking() const noexcept;
	void SeekToOffset(int64_t offset, int whence);

	CFStringRef CopyDescription() const noexcept;

protected:
	/// The shared log for all `InputSource` instances
	static const os_log_t _Nonnull sLog;

	explicit InputSource() noexcept = default;

	explicit InputSource(CFURLRef _Nullable url) noexcept
	{ if(url) url_ = static_cast<CFURLRef>(CFRetain(url)); }

private:
	// Subclasses must implement the following methods
	virtual void _Open() = 0;
	virtual void _Close() = 0;
	virtual int64_t _Read(void * _Nonnull buffer, int64_t count) = 0;
	virtual bool _AtEOF() const = 0;
	virtual int64_t _Offset() const = 0;
	virtual int64_t _Length() const = 0;
	// Optional seeking support
	virtual bool _SupportsSeeking() const noexcept			{ return false; }
	virtual void _SeekToOffset(int64_t offset, int whence) 	{ throw std::logic_error("Seeking not supported"); }
	// Optional description
	virtual CFStringRef _CopyDescription() const noexcept 	{ return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<InputSource: %p>"), this); }

	/// The location of the bytes to be read
	CFURLRef _Nullable url_ {nullptr};
	/// `true` if the input source is open
	bool isOpen_ {false};
};

} /* namespace SFB */
