/*
 *  Copyright (C) 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
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

#include <CoreAudio/CoreAudioTypes.h>
#include <memory>

#include "AudioFormat.h"

/*! @file AudioRingBuffer.h @brief An audio ring buffer */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief A ring buffer implementation supporting non-interleaved audio.
		 *
		 * This class is thread safe when used from one reader thread
		 * and one writer thread (single producer, single consumer model).
		 *
		 * The read and write routines are based on JACK's ringbuffer implementation
		 * but are modified for non-interleaved audio.
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
			 * @brief Allocate space for audio data.
			 * @note Only interleaved formats are supported.
			 * @note This method is not thread safe.
			 * @param format The format of the audio that will be written to and read from this buffer.
			 * @param capacityFrames The desired capacity, in frames
			 * @return \c true on success, \c false on error
			 */
			bool Allocate(const AudioFormat& format, size_t capacityFrames);

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


			/*! @brief Get the capacity of this RingBuffer in frames */
			inline size_t GetCapacityFrames() const						{ return mCapacityFrames; }

			/*! @brief Get the format of this \c BufferList */
			inline const AudioFormat& GetFormat() const					{ return mFormat; }

			/*! @brief  Get the number of frames available for reading */
			size_t GetFramesAvailableToRead() const;

			/*! @brief Get the free space available for writing in frames */
			size_t GetFramesAvailableToWrite() const;

			//@}


			// ========================================
			/*! @name Reading and writing audio */
			//@{

			/*!
			 * @brief Read audio from the \c RingBuffer, advancing the read pointer.
			 * @param bufferList An \c AudioBufferList to receive the audio
			 * @param frameCount The desired number of frames to read
			 * @return The number of frames actually read
			 */
			size_t ReadAudio(AudioBufferList *bufferList, size_t frameCount);

			/*!
			 * @brief Write audio to the \c RingBuffer, advancing the write pointer.
			 * @param bufferList An \c AudioBufferList containing the audio to copy
			 * @param frameCount The desired number of frames to write
			 * @return The number of frames actually written
			 */
			size_t WriteAudio(const AudioBufferList *bufferList, size_t frameCount);

			//@}

		private:

			AudioFormat			mFormat;				// The format of the audio

			unsigned char		**mBuffers;				// The channel pointers and buffers, allocated in one chunk of memory

			size_t				mCapacityFrames;		// Frame capacity per channel
			size_t				mCapacityFramesMask;
			
			volatile size_t		mWritePointer;			// In frames
			volatile size_t		mReadPointer;
		};
		
	}
}
