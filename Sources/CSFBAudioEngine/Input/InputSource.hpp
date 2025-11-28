//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <memory>
#import <stdexcept>
#import <type_traits>
#import <vector>

#import <libkern/OSByteOrder.h>
#import <os/log.h>

#import <CoreFoundation/CoreFoundation.h>

namespace SFB {

/// An input source.
class InputSource
{
public:
	using unique_ptr = std::unique_ptr<InputSource>;

	enum class FileReadMode { normal, memoryMap, loadInMemory, };
	static unique_ptr CreateForURL(CFURLRef url, FileReadMode mode = FileReadMode::normal);
	static unique_ptr CreateWithData(CFDataRef _Nonnull data);
	static unique_ptr CreateWithBytes(const void * _Nonnull buf, int64_t len);
	static unique_ptr CreateWithBytesNoCopy(const void * _Nonnull buf, int64_t len, bool free = true);

	virtual ~InputSource() noexcept;

	// This class is non-copyable.
	InputSource(const InputSource&) = delete;
	InputSource(InputSource&&) = delete;

	// This class is non-assignable.
	InputSource& operator=(const InputSource&) = delete;
	InputSource& operator=(InputSource&&) = delete;

	/// Returns the URL, if any, of the input source.
	CFURLRef _Nullable GetURL() const noexcept 	{ return url_; }

	// Opening and closing
	/// Opens the input source.
	void Open();
	/// Closes the input source.
	void Close();
	/// Returns `true` if the input source is open.
	bool IsOpen() const noexcept 	{ return isOpen_; }

	// Reading
	/// Reads up to `count` bytes from the input source into `buffer` and returns the number of bytes read.
	int64_t Read(void * _Nonnull buffer, int64_t count);
	/// Reads and returns up to `count` bytes from the input source in a `CFData` object.
	CFDataRef _Nullable CopyData(int64_t count);
	/// Reads and returns up to `count` bytes from the input source in a `std::vector` object.
	std::vector<uint8_t> ReadBlock(std::vector<uint8_t>::size_type count);

	// Position
	/// Returns `true` if the input source is at the end of input.
	bool AtEOF() const;
	/// Returns the current read position of the input source in bytes.
	int64_t Position() const;
	/// Returns the number of bytes in the input source.
	int64_t Length() const;

	// Seeking
	/// Returns `true` if the input source is seekable.
	bool SupportsSeeking() const noexcept;
	/// Possible seek anchor points.
	enum class SeekAnchor { start, current, end, };
	/// Seeks to `offset` bytes relative to `whence`.
	void SeekToOffset(int64_t offset, SeekAnchor whence = SeekAnchor::start);

	/// Returns a description of the input source.
	CFStringRef CopyDescription() const noexcept;

	// Helpers
	/// Reads and returns a value from the input source.
	template <typename V, typename = std::enable_if_t<std::is_trivially_copyable_v<V> && std::is_trivially_default_constructible_v<V>>>
	V ReadValue()
	{
		if(!IsOpen()) {
			os_log_error(sLog, "ReadValue() called on <InputSource: %p> that hasn't been opened", this);
			throw std::logic_error("Input source not open");
		}

		V value;
		if(_Read(&value, sizeof(V)) != sizeof(V))
			throw std::runtime_error("Insufficient data");
		return value;
	}

	/// Possible byte orders.
	enum class ByteOrder { little, big, host, swapped, };

	/// Reads and returns an unsigned integer value in the specified byte order.
	template <typename U, typename = std::enable_if_t<std::is_same_v<U, std::uint16_t> || std::is_same_v<U, std::uint32_t> || std::is_same_v<U, std::uint64_t>>>
	U ReadUnsigned(ByteOrder order = ByteOrder::host)
	{
		if(!IsOpen()) {
			os_log_error(sLog, "ReadUnsigned() called on <InputSource: %p> that hasn't been opened", this);
			throw std::logic_error("Input source not open");
		}

		U value;
		if(_Read(&value, sizeof(U)) != sizeof(U))
			throw std::runtime_error("Insufficient data");

		if constexpr (std::is_same_v<U, std::uint16_t>) {
			switch(order) {
				case ByteOrder::little: 	return OSSwapLittleToHostInt16(value);
				case ByteOrder::big: 		return OSSwapBigToHostInt16(value);
				case ByteOrder::host: 		return value;
				case ByteOrder::swapped: 	return OSSwapInt16(value);
			}
		}
		else if constexpr (std::is_same_v<U, std::uint32_t>) {
			switch(order) {
				case ByteOrder::little: 	return OSSwapLittleToHostInt32(value);
				case ByteOrder::big: 		return OSSwapBigToHostInt32(value);
				case ByteOrder::host: 		return value;
				case ByteOrder::swapped: 	return OSSwapInt32(value);
			}
		}
		else if constexpr (std::is_same_v<U, std::uint64_t>) {
			switch(order) {
				case ByteOrder::little: 	return OSSwapLittleToHostInt64(value);
				case ByteOrder::big: 		return OSSwapBigToHostInt64(value);
				case ByteOrder::host: 		return value;
				case ByteOrder::swapped: 	return OSSwapInt64(value);
			}
		}
		else
			static_assert(false, "Unsupported unsigned integer type");
	}

	/// Reads and returns a signed integer value in the specified byte order.
	template <typename S, typename = std::enable_if_t<std::is_same_v<S, std::int16_t> || std::is_same_v<S, std::int32_t> || std::is_same_v<S, std::int64_t>>>
	S ReadSigned(ByteOrder order = ByteOrder::host)
	{
		return std::make_signed(ReadUnsigned<std::make_unsigned<S>>(order));
	}

protected:
	/// The shared log for all `InputSource` instances.
	static const os_log_t _Nonnull sLog;

	explicit InputSource() noexcept = default;

	/// The location of the input.
	CFURLRef _Nullable url_ {nullptr};

private:
	// Subclasses must implement the following methods
	virtual void _Open() = 0;
	virtual void _Close() = 0;
	virtual int64_t _Read(void * _Nonnull buffer, int64_t count) = 0;
	virtual bool _AtEOF() const = 0;
	virtual int64_t _Position() const = 0;
	virtual int64_t _Length() const = 0;
	// Optional seeking support
	virtual bool _SupportsSeeking() const noexcept 			{ return false; }
	virtual void _SeekToPosition(int64_t position) 			{ throw std::logic_error("Seeking not supported"); }
	// Optional description
	virtual CFStringRef _CopyDescription() const noexcept 	{ return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("<InputSource: %p>"), this); }

	/// `true` if the input source is open.
	bool isOpen_ {false};
};

} /* namespace SFB */
