/*
 *  Copyright (C) 2014, 2015 Stephen F. Booth <me@sbooth.org>
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

/*! @file RingBuffer.h @brief A generic ring buffer */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*!
	 * @brief A generic ring buffer implementation
	 *
	 * This class is thread safe when used from one reader thread
	 * and one writer thread (single producer, single consumer model).
	 *
	 * The read and write routines are based on JACK's ringbuffer implementation
	 */
	class RingBuffer
	{
	public:
		// ========================================
		/*! @name Creation and Destruction */
		//@{

		/*! @brief A \c std::unique_ptr for \c RingBuffer objects */
		using unique_ptr = std::unique_ptr<RingBuffer>;

		/*!
		 * @brief Create a new \c RingBuffer
		 * @note Allocate() must be called before the object may be used.
		 */
		RingBuffer();

		/*! @brief Destroy the \c RingBuffer and release all associated resources. */
		~RingBuffer();

		/*! @cond */

		/*! @internal This class is non-copyable */
		RingBuffer(const RingBuffer& rhs) = delete;

		/*! @internal This class is non-assignable */
		RingBuffer& operator=(const RingBuffer& rhs) = delete;

		/*! @endcond */

		//@}


		// ========================================
		/*! @name Buffer management */
		//@{

		/*!
		 * @brief Allocate space for data.
		 * @note This method is not thread safe.
		 * @param byteCount The desired capacity, in bytes
		 * @return \c true on success, \c false on error
		 */
		bool Allocate(size_t byteCount);

		/*!
		 * @brief Free the resources used by this \c RingBuffer
		 * @note This method is not thread safe.
		 */
		void Deallocate();


		/*!
		 * @brief Reset this \c RingBuffer to its default state.
		 * @note This method is not thread safe.
		 */
		void Reset();


		/*! @brief Get the capacity of this RingBuffer in bytes */
		inline size_t GetCapacityBytes() const						{ return mCapacityBytes; }

		/*! @brief  Get the number of bytes available for reading */
		size_t GetBytesAvailableToRead() const;

		/*! @brief Get the free space available for writing in bytes */
		size_t GetBytesAvailableToWrite() const;

		//@}


		// ========================================
		/*! @name Reading and writing data */
		//@{

		/*!
		 * @brief Read data from the \c RingBuffer, advancing the read pointer.
		 * @param destinationBuffer An address to receive the data
		 * @param byteCount The desired number of bytes to read
		 * @return The number of bytes actually read
		 */
		size_t Read(void *destinationBuffer, size_t byteCount);

		/*!
		 * @brief Read data from the \c RingBuffer without advancing the read pointer.
		 * @param destinationBuffer An address to receive the data
		 * @param byteCount The desired number of bytes to read
		 * @return The number of bytes actually read
		 */
		size_t Peek(void *destinationBuffer, size_t byteCount) const;

		/*!
		 * @brief Write data to the \c RingBuffer, advancing the write pointer.
		 * @param bufferList An address containing the data to copy
		 * @param byteCount The desired number of frames to write
		 * @return The number of bytes actually written
		 */
		size_t Write(const void *sourceBuffer, size_t byteCount);


		/*! @brief Advance the read pointer by the specified number of bytes */
		void ReadAdvance(size_t byteCount);

		/*! @brief Advance the write pointer by the specified number of bytes */
		void WriteAdvance(size_t byteCount);

		
		/*! @brief A struct wrapping a memory buffer location and capacity */
		struct Buffer {
			uint8_t	*mBuffer;			/*!< The memory buffer location */
			size_t	mBufferCapacity;	/*!< The capacity of \c mBuffer in bytes */

			/*! @brief Construct an empty Buffer */
			Buffer()
				: Buffer(nullptr, 0) {}
			
			/*! 
			 * @brief Construct a Buffer for the specified location and capacity 
			 * @param buffer The memory buffer location
			 * @param bufferCapacity The capacity of \c buffer in bytes
			 */
			Buffer(uint8_t *buffer, size_t bufferCapacity)
				: mBuffer(buffer), mBufferCapacity(bufferCapacity) {}
		};

		/*! @brief A pair of \c Buffer objects */
		using BufferPair = std::pair<Buffer, Buffer>;

		/*! @brief Retrieve the read vector containing the current readable data */
		BufferPair GetReadVector() const;

		/*! @brief Retrieve the write vector containing the current writeable data */
		BufferPair GetWriteVector() const;

		//@}

	private:

		uint8_t				*mBuffer;				/*!< The memory buffer holding the data */

		size_t				mCapacityBytes;			/*!< The capacity of \c mBuffer in bytes */
		size_t				mCapacityBytesMask;		/*!< The capacity of \c mBuffer in bytes minus one */

		volatile size_t		mWritePointer;			/*!< The offset into \c mBuffer of the read location */
		volatile size_t		mReadPointer;			/*!< The offset into \c mBuffer of the write location */
	};

}
