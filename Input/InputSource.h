/*
 *  Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <memory>

#include <CoreFoundation/CoreFoundation.h>

#include "CFWrapper.h"

/*! @file InputSource.h @brief Support for arbitrary bytestream input to \c AudioDecoder */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief An abstract class allowing access to a stream of bytes */
	class InputSource
	{

	public:

		/*! @brief The \c CFErrorRef error domain used by \c InputSource and subclasses */
		static const CFStringRef ErrorDomain;

		/*! @brief Possible \c CFErrorRef error codes used by \c InputSource */
		enum ErrorCode {
			FileNotFoundError		= 0,		/*!< File not found */
			InputOutputError		= 1			/*!< Input/output error */
		};

		/*! Flags used in \c InputSource::CreateInputSourceForURL */
		enum InputSourceFlags {
			MemoryMapFiles			= 1 << 0,	/*!< Files should be mapped in memory using \c mmap() */
			LoadFilesInMemory		= 1 << 1	/*!< Files should be fully loaded in memory */
		};
		

		// ========================================
		/*! @name Factory Methods */
		//@{

		/*! @brief A \c std::unique_ptr for \c InputSource objects */
		using unique_ptr = std::unique_ptr<InputSource>;

		/*!
		 * Create a new \c InputSource for the given URL
		 * @param url The URL
		 * @param flags Optional flags affecting how \c url is handled
		 * @param error An optional pointer to a \c CFErrorRef to receive error information
		 * @return An \c InputSource for the specified URL, or \c nullptr on failure
		 * @see InputSourceFlags
		 */
		static unique_ptr CreateInputSourceForURL(CFURLRef url, int flags = 0, CFErrorRef *error = nullptr);

		//@}


		// ========================================
		/*! @name Creation and Destruction */
		// @{

		/*! @brief Destroy this \c InputSource */
		inline virtual ~InputSource() = default;

		/*! @cond */

		/*! @internal This class is non-copyable */
		InputSource(const InputSource& rhs) = delete;

		/*! @internal This class is non-assignable */
		InputSource& operator=(const InputSource& rhs) = delete;

		/*! @endcond */

		//@}


		// ========================================
		/*! @name URL access */
		//@{

		/*! @brief Get the URL for this \c InputSource */
		inline CFURLRef GetURL() const							{ return mURL; }

		//@}


		// ========================================
		/*! @name Opening and Closing */
		//@{

		/*!
		 * @brief Open the input for reading
		 * @param error An optional pointer to a \c CFErrorRef to receive error information
		 * @return \c true on success, \c false otherwise
		 */
		bool Open(CFErrorRef *error = nullptr);

		/*!
		 * @brief Close the input
		 * @param error An optional pointer to a \c CFErrorRef to receive error information
		 * @return \c true on success, \c false otherwise
		 */
		bool Close(CFErrorRef *error = nullptr);


		/*! @brief Query whether this \c InputSource is open */
		inline bool IsOpen() const								{ return mIsOpen; }


		// ========================================
		/*! @name Bytestream access */
		//@{

		/*!
		 * @brief Read bytes from the input
		 * @param buffer The destination buffer
		 * @param byteCount The maximum number of bytes to read
		 * @return The number of bytes read
		 */
		SInt64 Read(void *buffer, SInt64 byteCount);

		/*!
		 * @brief Read an integral type from the input
		 * @tparam The integral type to read
		 * @param value The destination value
		 * @return \c true on success, \c false otherwise
		 */
		template <typename T> typename std::enable_if<std::is_integral<T>::value, bool>::type Read(T& value)
		{
			auto valueSize = sizeof(value);
			auto bytesRead = Read(&value, (SInt64)valueSize);
			if((SInt64)valueSize != bytesRead)
				return false;
			return true;
		}

		/*!
		 * @brief Read an unsigned little endian integral type from the input and convert to host byte ordering
		 * @tparam The unsigned integral type to read
		 * @param value The destination value
		 * @return \c true on success, \c false otherwise
		 */
		template <typename T> typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadLE(T& value)
		{
			auto valueSize = sizeof(value);
			auto bytesRead = Read(&value, (SInt64)valueSize);
			if((SInt64)valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2:	value = (T)OSSwapLittleToHostInt16(value);	break;
				case 4:	value = (T)OSSwapLittleToHostInt32(value);	break;
				case 8:	value = (T)OSSwapLittleToHostInt64(value);	break;
			}

			return true;
		}

		/*!
		 * @brief Read an unsigned big endian integral type from the input and convert to host byte ordering
		 * @tparam The unsigned integral type to read
		 * @param value The destination value
		 * @return \c true on success, \c false otherwise
		 */
		template <typename T> typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadBE(T& value)
		{
			auto valueSize = sizeof(value);
			auto bytesRead = Read(&value, (SInt64)valueSize);
			if((SInt64)valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2:	value = (T)OSSwapBigToHostInt16(value); break;
				case 4:	value = (T)OSSwapBigToHostInt32(value); break;
				case 8:	value = (T)OSSwapBigToHostInt64(value); break;
			}

			return true;
		}

		/*!
		 * @brief Read an unsigned integral type from the input and swap its byte ordering
		 * @tparam The unsigned integral type to read
		 * @param value The destination value
		 * @return \c true on success, \c false otherwise
		 */
		template <typename T> typename std::enable_if<std::is_unsigned<T>::value, bool>::type ReadSwapped(T& value)
		{
			auto valueSize = sizeof(value);
			auto bytesRead = Read(&value, (SInt64)valueSize);
			if((SInt64)valueSize != bytesRead)
				return false;

			switch(valueSize) {
				case 2: value = (T)OSSwapInt16(value); break;
				case 4: value = (T)OSSwapInt32(value); break;
				case 8: value = (T)OSSwapInt64(value); break;
			}

			return true;
		}


		/*! @brief Determine whether the end of input has been reached */
		bool AtEOF() const;


		/*! @brief Get the current offset in the input, in bytes */
		SInt64 GetOffset() const;

		/*! @brief Get the length of the input, in bytes */
		SInt64 GetLength() const;


		/*! @brief Query whether this \c InputSource is seekable */
		bool SupportsSeeking() const;

		/*!
		 * Seek to the specified byte offset
		 * @param offset The desired byte offset
		 * @return \c true on success, \c false otherwise
		 */
		bool SeekToOffset(SInt64 offset);

		//@}

	protected:

		/*! @brief Create a new \c InputSource and initialize \c InputSource::mURL to \c nullptr */
		InputSource();
		
		/*! @brief Create a new \c InputSource and initialize \c InputSource::mURL to \c url */
		InputSource(CFURLRef url);

	private:

		// Subclasses must implement the following methods
		virtual bool _Open(CFErrorRef *error) = 0;
		virtual bool _Close(CFErrorRef *error) = 0;
		virtual SInt64 _Read(void *buffer, SInt64 byteCount) = 0;
		virtual bool _AtEOF() const = 0;
		virtual SInt64 _GetOffset() const = 0;
		virtual SInt64 _GetLength() const = 0;

		// Optional seeking support
		virtual bool _SupportsSeeking() const					{ return false; }
		virtual bool _SeekToOffset(SInt64 /*offset*/)			{ return false; }

		// Data members
		SFB::CFURL mURL;	/*!< @brief The location of the bytes to be read */
		bool mIsOpen;		/*!< @brief Indicates if input is open */

	};

}
