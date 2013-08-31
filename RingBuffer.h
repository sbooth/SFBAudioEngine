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

const UInt32 kGeneralRingTimeBoundsQueueSize = 32;
const UInt32 kGeneralRingTimeBoundsQueueMask = kGeneralRingTimeBoundsQueueSize - 1;

// ========================================
// Ring buffer class based off of Apple's CARingBuffer
// This class implements a ring buffer that is thread safe for the case of
// one reader and one writer (single producer, single consumer).
// ========================================
class RingBuffer
{
public:
	// ========================================
	// Typedefs
	// ========================================
	typedef SInt64 SampleTime;

	// ========================================
	// Creation/Destruction
	RingBuffer();
	~RingBuffer();

	// ========================================
	// Buffer management
	bool Allocate(const AudioStreamBasicDescription& format, UInt32 capacityFrames);
	bool Allocate(UInt32 channelCount, UInt32 bytesPerFrame, UInt32 capacityFrames);

	void Deallocate();

	inline UInt32 GetCapacityFrames() const						{ return mCapacityFrames; }

	// ========================================
	// Storing and fetching audio

	// Copies nFrames of data into the ring buffer at the specified sample time.
	// The sample time should normally increase sequentially, though gaps
	// are filled with zeroes. A sufficiently large gap effectively empties
	// the buffer before storing the new data.

	// If frameNumber is less than the previous frame number, the behavior is undefined.

	// Return false for failure (buffer not large enough).
	bool Store(const AudioBufferList *bufferList, UInt32 frameCount, SampleTime frameNumber);

	bool Fetch(AudioBufferList *bufferList, UInt32 frameCount, SampleTime frameNumber) const;

	// Get the time range contained in the buffer
	bool GetTimeBounds(SampleTime& startTime, SampleTime& endTime) const;
	
protected:

	inline UInt32 FrameOffset(SampleTime frameNumber) const		{ return (frameNumber & mCapacityFramesMask) * mBytesPerFrame; }

	// These should only be called from Store()
	inline SampleTime StartTime() const							{ return mTimeBoundsQueue[mTimeBoundsQueueCounter & kGeneralRingTimeBoundsQueueMask].mStartTime; }
	inline SampleTime EndTime() const							{ return mTimeBoundsQueue[mTimeBoundsQueueCounter & kGeneralRingTimeBoundsQueueMask].mEndTime; }

	bool ConstrainTimesToBounds(SampleTime& startRead, SampleTime& endRead) const;

	void SetTimeBounds(SampleTime startTime, SampleTime endTime);
	
	unsigned char			**mBuffers;				// allocated in one chunk of memory
	UInt32					mNumberChannels;
	UInt32					mBytesPerFrame;			// within one deinterleaved channel
	UInt32					mCapacityFrames;		// per channel, must be a power of 2
	UInt32					mCapacityFramesMask;
	UInt32					mCapacityBytes;			// per channel
	
	// Range of valid sample time in the buffer
	struct TimeBounds {
		volatile SampleTime		mStartTime;
		volatile SampleTime		mEndTime;
		volatile UInt32			mUpdateCounter;
	};
	
	RingBuffer::TimeBounds mTimeBoundsQueue[kGeneralRingTimeBoundsQueueSize];
	UInt32 mTimeBoundsQueueCounter;
};
