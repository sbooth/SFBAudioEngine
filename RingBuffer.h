/*
 *  Copyright (C) 2013 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

// ========================================
// A ring buffer implementation supporting non-interleaved audio
//
// This class is thread safe when used from one reader thread
// and one writer thread (single producer, single consumer model).
//
// The read and write routines are based on JACK's ringbuffer implementation,
// but modified for non-interleaved audio
// ========================================
class RingBuffer
{
public:
	// ========================================
	// Creation/Destruction

	/*!
	 * Create a new \c RingBuffer
	 * @note Allocate() must be called before the object may be used.
	 */
	RingBuffer();

	/*!
	 * Destroy the \c RingBuffer and release all associated resources.
	 */
	~RingBuffer();

	// ========================================
	// Buffer management

	/*!
	 * Allocate space for audio data.
	 * @note Only interleaved formats are supported.
	 * @note This method is not thread safe.
	 * @param format The format of the audio that will be written to and read from this buffer.
	 * @param capacityFrames The desired capacity, in frames
	 * @return \c true on success, \c false on error
	 */
	bool Allocate(const AudioStreamBasicDescription& format, size_t capacityFrames);

	/*!
	 * Allocate space for audio data.
	 * @note This method is not thread safe.
	 * @param channelCount The number of interleaved channels
	 * @param bytesPerFrame The number of bytes per audio frame
	 * @param capacityFrames The desired capacity, in frames
	 * @return \c true on success, \c false on error
	 */
	bool Allocate(UInt32 channelCount, UInt32 bytesPerFrame, size_t capacityFrames);

	/*!
	 * Free the resources used by this \c RingBuffer
	 * @note This method is not thread safe.
	 */
	void Deallocate();

	/*!
	 * Reset this \c RingBuffer to its default state.
	 * @note This method is not thread safe.
	 */
	void Reset();

	/*!
	 * Get the capacity of this RingBuffer in frames
	 * @return The capacity of this RingBuffer in frames
	 */
	inline size_t GetCapacityFrames() const						{ return mCapacityFrames; }

	/*!
	 * Get the number of frames available for reading
	 * @return The number of frames available for reading
	 */
	size_t GetFramesAvailableToRead() const;

	/*!
	 * Get the free space available for writing
	 * @return The number of frames available for writing
	 */
	size_t GetFramesAvailableToWrite() const;

	// ========================================
	// Reading and writing audio

	/*!
	 * Read audio from the \c RingBuffer, advancing the read pointer.
	 * @param bufferList An \c AudioBufferList to receive the audio
	 * @param frameCount The desired number of frames to read
	 * @return The number of frames actually read
	 */
	size_t ReadAudio(AudioBufferList *bufferList, size_t frameCount);

	/*!
	 * Write audio to the \c RingBuffer, advancing the write pointer.
	 * @param bufferList An \c AudioBufferList containing the audio to copy
	 * @param frameCount The desired number of frames to write
	 * @return The number of frames actually written
	 */
	size_t WriteAudio(const AudioBufferList *bufferList, size_t frameCount);

private:

	UInt32				mNumberChannels;		// The number of interleaved channels
	UInt32				mBytesPerFrame;			// The number of bytes per audio frames

	unsigned char		**mBuffers;				// The channel pointers and buffers, allocated in one chunk of memory

	size_t				mCapacityFrames;		// Frame capacity per channel
	size_t				mCapacityFramesMask;

	size_t				mCapacityBytes;			// Byte capacity per frame

	volatile size_t		mWritePointer;			// In frames
	volatile size_t		mReadPointer;
};
