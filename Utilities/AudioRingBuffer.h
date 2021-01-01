/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <atomic>
#include <memory>

#include <CoreAudio/CoreAudioTypes.h>

#include "AudioFormat.h"

/*! @file AudioRingBuffer.h @brief An audio ring buffer */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief A ring buffer supporting non-interleaved audio.
		 *
		 * This class is thread safe when used from one reader thread and one writer thread (single producer, single consumer model).
		 *
		 * The read and write routines were originally based on JACK's ringbuffer implementation.
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
			RingBuffer() noexcept;

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
			 * @note Capacities from 2 to 2,147,483,648 (0x80000000) frames are supported
			 * @param format The format of the audio that will be written to and read from this buffer.
			 * @param capacityFrames The desired capacity, in frames
			 * @return \c true on success, \c false on error
			 */
			bool Allocate(const class Format& format, size_t capacityFrames) noexcept;

			/*!
			 * @brief Free the resources used by this \c RingBuffer
			 * @note This method is not thread safe.
			 */
			void Deallocate() noexcept;


			/*!
			 * @brief Reset this \c RingBuffer to its default state.
			 * @note This method is not thread safe.
			 */
			void Reset() noexcept;


			/*! @brief Returns the capacity of this RingBuffer in frames */
			inline size_t CapacityFrames() const noexcept				{ return mCapacityFrames; }

			/*! @brief Returns the format of this \c BufferList */
			inline const Format& Format() const noexcept				{ return mFormat; }

			/*! @brief Returns the number of frames available for reading */
			size_t FramesAvailableToRead() const noexcept;

			/*! @brief Returns the free space available for writing in frames */
			size_t FramesAvailableToWrite() const noexcept;

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
			size_t Read(AudioBufferList * const bufferList, size_t frameCount) noexcept;

			/*!
			 * @brief Write audio to the \c RingBuffer, advancing the write pointer.
			 * @param bufferList An \c AudioBufferList containing the audio to copy
			 * @param frameCount The desired number of frames to write
			 * @return The number of frames actually written
			 */
			size_t Write(const AudioBufferList * const bufferList, size_t frameCount) noexcept;

			//@}

		private:

			class Format		mFormat;				// The format of the audio

			uint8_t				**mBuffers;				// The channel pointers and buffers, allocated in one chunk of memory

			size_t				mCapacityFrames;		// Frame capacity per channel
			size_t				mCapacityFramesMask;

			std::atomic_size_t	mWritePointer;			// In frames
			std::atomic_size_t	mReadPointer;
		};

	}
}
