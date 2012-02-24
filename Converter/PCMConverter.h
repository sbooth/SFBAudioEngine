/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <map>

#include "AudioConverter.h"

// ========================================
// A PCM converter
// ========================================
class PCMConverter : public AudioConverter
{
public:
	// ========================================
	PCMConverter(const AudioStreamBasicDescription& sourceFormat, const AudioStreamBasicDescription& destinationFormat);
	virtual ~PCMConverter();

	// The mapping which specifies how input channels should map to output channels
	// The key is the output channel number, and the value is the input channel number (zero indexed)
	inline std::map<int, int> GetChannelMap()							{ return mChannelMap; }
	inline void SetChannelMap(const std::map<int, int>& channelMap)		{ mChannelMap = channelMap; }

	// ========================================
	virtual UInt32 Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);

private:
	UInt32 ConvertToFloat(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToDouble(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	
	UInt32 ConvertToPacked8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale = 1u << 7);
	UInt32 ConvertToPacked16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale = 1u << 15);
	UInt32 ConvertToPacked24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale = 1u << 23);
	UInt32 ConvertToPacked32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale = 1u << 31);
	
	UInt32 ConvertToHighAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToHighAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToHighAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToHighAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	
	UInt32 ConvertToLowAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToLowAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToLowAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);
	UInt32 ConvertToLowAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount);	

	std::map<int, int> mChannelMap;
};
