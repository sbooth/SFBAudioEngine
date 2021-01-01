/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <algorithm>
#import <type_traits>

namespace SFB {

	/// A \c ByteStream provides heterogeneous typed access to an untyped buffer.
	class ByteStream {
	public:
		/// Initializes a \c ByteStream object with the specified buffer and length and sets the read position to \c 0
		/// @param buf The buffer providing the data
		/// @param len The length of \c buf in bytes
		ByteStream(const void * const buf, size_t len) noexcept
			: mBuffer(buf), mBufferLength(len), mReadPosition(0)
		{
			assert(mBuffer != nullptr);
		}

		/// Initializes a \c ByteStream object with the same buffer, length, and read position as \c rhs
		/// @param rhs The object to copy
		ByteStream(const ByteStream& rhs) noexcept
			: mBuffer(rhs.mBuffer), mBufferLength(rhs.mBufferLength), mReadPosition(rhs.mReadPosition)
		{}

		/// Sets the buffer, length, and read position to those of \c rhs
		/// @param rhs The object to copy
		/// @return A reference to \c this
		ByteStream& operator=(const ByteStream& rhs) noexcept
		{
			mBuffer = rhs.mBuffer;
			mBufferLength = rhs.mBufferLength;
			mReadPosition = rhs.mReadPosition;
			return *this;
		}

		/// Compares to \c ByteStream objects for equality
		/// Two \c ByteStream objects are equal if they have the same buffer, length, and read position
		/// @param rhs The object to compare
		/// @return \c true if the objects are equal, \c false otherwise
		bool operator==(const ByteStream& rhs) noexcept
		{
			return mBuffer == rhs.mBuffer && mBufferLength == rhs.mBufferLength && mReadPosition == rhs.mReadPosition;
		}

		/// Compares to \c ByteStream objects for inequality
		/// Two \c ByteStream objects are equal if they have the same buffer, length, and read position
		/// @param rhs The object to compare
		/// @return \c true if the objects are not equal, \c false otherwise
		bool operator!=(const ByteStream& rhs) noexcept
		{
			return mBuffer != rhs.mBuffer || mBufferLength != rhs.mBufferLength || mReadPosition != rhs.mReadPosition;
		}

		/// Reads an integral type and advances the read position
		/// @tparam T The integral type to read
		/// @param value The destination value
		/// @return \c true on success, \c false otherwise
		template <typename T>
		typename std::enable_if<std::is_integral<T>::value, bool>::type Read(T& value) noexcept
		{
			auto valueSize = sizeof(value);
			if(valueSize > Remaining())
				return false;
			auto bytesRead = Read(&value, valueSize);
			return bytesRead == valueSize;
		}

		/// Reads an unsigned little endian integral type converted to host byte ordering and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @param value The destination value
		/// @return \c true on success, \c false otherwise
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadLE(T& value) noexcept
		{
			auto valueSize = sizeof(value);
			if(valueSize > Remaining())
				return false;
			auto bytesRead = Read(&value, valueSize);
			if(valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2:	value = static_cast<T>(OSSwapLittleToHostInt16(value)); break;
				case 4:	value = static_cast<T>(OSSwapLittleToHostInt32(value)); break;
				case 8:	value = static_cast<T>(OSSwapLittleToHostInt64(value)); break;
			}

			return true;
		}

		/// Reads an unsigned big endian integral type converted to host byte ordering and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @param value The destination value
		/// @return \c true on success, \c false otherwise
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadBE(T& value) noexcept
		{
			auto valueSize = sizeof(value);
			if(valueSize > Remaining())
				return false;
			auto bytesRead = Read(&value, valueSize);
			if(valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2:	value = static_cast<T>(OSSwapBigToHostInt16(value)); break;
				case 4:	value = static_cast<T>(OSSwapBigToHostInt32(value)); break;
				case 8:	value = static_cast<T>(OSSwapBigToHostInt64(value)); break;
			}

			return true;
		}

		/// Reads an unsigned integral type, swaps its byte ordering, and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @param value The destination value
		/// @return \c true on success, \c false otherwise
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadSwapped(T& value) noexcept
		{
			auto valueSize = sizeof(value);
			if(valueSize > Remaining())
				return false;
			auto bytesRead = Read(&value, valueSize);
			if(valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2: value = static_cast<T>(OSSwapInt16(value)); break;
				case 4: value = static_cast<T>(OSSwapInt32(value)); break;
				case 8: value = static_cast<T>(OSSwapInt64(value)); break;
			}

			return true;
		}

		/// Reads an integral type and advances the read position
		/// @tparam T The integral type to read
		/// @return The value read or \c 0 on failure
		template <typename T>
		typename std::enable_if<std::is_integral<T>::value, T>::type Read() noexcept
		{
			T value;
			return Read(value) ? value : 0;
		}

		/// Reads an unsigned little endian integral type converted to host byte ordering and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @return The value read or \c 0 on failure
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, T>::type ReadLE() noexcept
		{
			T value;
			return ReadLE(value) ? value : 0;
		}

		/// Reads an unsigned big endian integral type converted to host byte ordering and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @return The value read or \c 0 on failure
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, T>::type ReadBE() noexcept
		{
			T value;
			return ReadBE(value) ? value : 0;
		}

		/// Reads an unsigned integral type, swaps its byte ordering, and advances the read position
		/// @tparam T The unsigned integral type to read
		/// @return The value read or \c 0 on failure
		template <typename T>
		typename std::enable_if<std::is_unsigned<T>::value, T>::type ReadSwapped() noexcept
		{
			T value;
			return ReadSwapped(value) ? value : 0;
		}

		/// Reads bytes and advances the read position
		/// @param buf The destination buffer or \c nullptr to discard the bytes
		/// @param count The number of bytes to read
		/// @return The number of bytes actually read
		size_t Read(void * const buf, size_t count) noexcept
		{
			auto bytesToCopy = std::min(count, mBufferLength - mReadPosition);
			if(buf)
				memcpy(buf, static_cast<const uint8_t *>(mBuffer) + mReadPosition, bytesToCopy);
			mReadPosition += bytesToCopy;
			return bytesToCopy;
		}

		/// Advances the read position
		/// @param count The number of bytes to skip
		/// @return The number of bytes actually skipped
		size_t Skip(size_t count) noexcept
		{
			mReadPosition += std::min(count, mBufferLength - mReadPosition);
			return mReadPosition;
		}

		/// Rewinds the read position
		/// @param count The number of bytes to rewind
		/// @return The number of bytes actually skipped
		size_t Rewind(size_t count) noexcept
		{
			auto bytesToSkip = std::min(count, mReadPosition);
			mReadPosition -= bytesToSkip;
			return bytesToSkip;
		}

		/// Returns the number of bytes in the buffer
		/// @return The number of bytes in the buffer
		inline size_t Length() const noexcept
		{
			return mBufferLength;
		}

		/// Returns the number of bytes remaining
		inline size_t Remaining() const noexcept
		{
			return mBufferLength - mReadPosition;
		}

		/// Returns the read position
		/// @return The read posiiton
		inline size_t Position() const noexcept
		{
			return mReadPosition;
		}

		/// Sets the read position
		/// @param pos The desired read position
		/// @return The new read posiiton
		inline size_t SetPosition(size_t pos) noexcept
		{
			mReadPosition = std::min(pos, mBufferLength);
			return mReadPosition;
		}

	private:
		
		/// The wrapped buffer
		const void *mBuffer;
		/// The number of bytes in \c mBuffer
		size_t mBufferLength;
		/// The current read position
		size_t mReadPosition;
	};
	
}
