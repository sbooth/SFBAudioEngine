/*
 *  Copyright (C) 2006, 2007, 2008, 2009 Stephen F. Booth <me@sbooth.org>
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

#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/ExtendedAudioFile.h>

#include "AudioDecoder.h"

// ========================================
// A wrapper around an AudioDecoder that decodes a specific file region
// ========================================
class LoopableRegionDecoder : public AudioDecoder
{

	friend class AudioDecoder;
	
public:

	// ========================================
	// Destruction
	virtual ~LoopableRegionDecoder();
	
	// ========================================
	// File access
	virtual bool OpenFile(CFErrorRef *error = NULL);
	virtual bool CloseFile(CFErrorRef *error = NULL);

	virtual inline bool FileIsOpen()						{ return mDecoder->FileIsOpen(); }

	// ========================================
	// The native format of the source audio
	virtual inline CFStringRef CreateSourceFormatDescription() { return mDecoder->CreateSourceFormatDescription(); }

	// ========================================
	// The starting frame for this audio file region
	inline SInt64 GetStartingFrame()						{ return mStartingFrame; }
	inline void SetStartingFrame(SInt64 startingFrame)		{ mStartingFrame = startingFrame; }
	
	// ========================================
	// The number of frames to decode
	inline UInt32 GetFrameCount()							{ return mFrameCount; }
	inline void SetFrameCount(UInt32 frameCount)			{ mFrameCount = frameCount; }
	
	// ========================================
	// The number of times to repeat the audio
	inline UInt32 GetRepeatCount()							{ return mRepeatCount; }
	inline void SetRepeatCount(UInt32 repeatCount)			{ mRepeatCount = repeatCount; }
	
	inline UInt32 GetCompletedPasses()						{ return mCompletedPasses; }
	
	// ========================================
	// Reset to initial state
	void Reset();
	
	// ========================================
	// Attempt to read frameCount frames of audio, returning the actual number of frames read
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);
	
	// ========================================
	// Source audio information
	virtual inline SInt64 GetTotalFrames()					{ return ((mRepeatCount + 1) * mFrameCount);}
	virtual inline SInt64 GetCurrentFrame()					{ return mTotalFramesRead;}
	
	// ========================================
	// Seeking support
	virtual inline bool SupportsSeeking()					{ return mDecoder->SupportsSeeking(); }
	virtual SInt64 SeekToFrame(SInt64 frame);
	
protected:
	
	// ========================================
	// For these to work correctly decoder must be open already
	LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame);
	LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount);
	LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount);
	
private:
	
	AudioDecoder	*mDecoder;
	
	SInt64			mStartingFrame;
	UInt32			mFrameCount;
	UInt32			mRepeatCount;
	
	UInt32			mFramesReadInCurrentPass;
	SInt64			mTotalFramesRead;
	UInt32			mCompletedPasses;
};
