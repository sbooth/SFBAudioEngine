/*
 *  Copyright (C) 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include <AudioToolbox/AudioToolbox.h>

class AudioDecoder;

// ========================================
// Enums
// ========================================
enum {
	eDecoderStateDataFlagDecodingStarted	= 1u << 0,
	eDecoderStateDataFlagDecodingFinished	= 1u << 1,
	eDecoderStateDataFlagRenderingStarted	= 1u << 2,
	eDecoderStateDataFlagRenderingFinished	= 1u << 3,
	eDecoderStateDataFlagStopDecoding		= 1u << 4
};

// ========================================
// State data for decoders that are decoding and/or rendering
// ========================================
class DecoderStateData
{
	
public:	

	DecoderStateData(AudioDecoder *decoder);
	~DecoderStateData();
	
	DecoderStateData(const DecoderStateData& rhs) = delete;
	DecoderStateData& operator=(const DecoderStateData& rhs) = delete;

	void AllocateBufferList(UInt32 capacityFrames);
	void DeallocateBufferList();

	void ResetBufferList();
	
	UInt32 ReadAudio(UInt32 frameCount);
	
	AudioDecoder			*mDecoder;

	AudioBufferList			*mBufferList;
	UInt32					mBufferCapacityFrames;
	
	SInt64					mTimeStamp;
	
	SInt64					mTotalFrames;
	volatile SInt64			mFramesRendered;

	SInt64					mFrameToSeek;
	
	volatile uint32_t		mFlags;

private:

	DecoderStateData();

};
