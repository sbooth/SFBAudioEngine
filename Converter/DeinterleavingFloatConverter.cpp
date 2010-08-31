/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include <stdexcept>
#include <Accelerate/Accelerate.h>

#include "DeinterleavingFloatConverter.h"


DeinterleavingFloatConverter::DeinterleavingFloatConverter(const AudioStreamBasicDescription& sourceFormat)
{
	if(kAudioFormatLinearPCM != sourceFormat.mFormatID)
		throw std::runtime_error("Only PCM input formats are supported by DeinterleavingFloatConverter");
	
	if(kAudioFormatFlagIsFloat & sourceFormat.mFormatFlags && 32 != sourceFormat.mBitsPerChannel)
		throw std::runtime_error("Only 32 bit float sample size is supported by DeinterleavingFloatConverter");

	if(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags && !(8 == sourceFormat.mBitsPerChannel || 16 == sourceFormat.mBitsPerChannel || 24 == sourceFormat.mBitsPerChannel || 32 == sourceFormat.mBitsPerChannel))
		throw std::runtime_error("Only 8, 16, 24, and 32 bit packed sample sizes are supported by DeinterleavingFloatConverter");

	if(!(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags) && !(8 == sourceFormat.mBytesPerFrame || 16 == sourceFormat.mBytesPerFrame || 32 == sourceFormat.mBytesPerFrame))
		throw std::runtime_error("Only 8, 16, and 32 bit unpacked frame sizes are supported by DeinterleavingFloatConverter");
	
	mSourceFormat = sourceFormat;
	
	// This converter always produces 64-bit deinterleaved float output
	mDestinationFormat.mFormatID			= kAudioFormatLinearPCM;
	mDestinationFormat.mFormatFlags			= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mDestinationFormat.mSampleRate			= sourceFormat.mSampleRate;
	mDestinationFormat.mChannelsPerFrame	= sourceFormat.mChannelsPerFrame;
	mDestinationFormat.mBitsPerChannel		= 8 * sizeof(double);
	
	mDestinationFormat.mBytesPerPacket		= (mDestinationFormat.mBitsPerChannel / 8);
	mDestinationFormat.mFramesPerPacket		= 1;
	mDestinationFormat.mBytesPerFrame		= mDestinationFormat.mBytesPerPacket * mDestinationFormat.mFramesPerPacket;
	
	mDestinationFormat.mReserved			= 0;
}

DeinterleavingFloatConverter::~DeinterleavingFloatConverter()
{}

UInt32
DeinterleavingFloatConverter::Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);
	
	// Nothing to do
	if(0 == frameCount) {
		for(UInt32 bufferIndex = 0; bufferIndex < outputBuffer->mNumberBuffers; ++bufferIndex)
			outputBuffer->mBuffers[bufferIndex].mDataByteSize = 0;
		
		return 0;
	}

	// Float-to-float
	if(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags) {
		for(UInt32 bufferIndex = 0; bufferIndex < inputBuffer->mNumberBuffers; ++bufferIndex) {
			for(UInt32 channelIndex = 0; channelIndex < inputBuffer->mBuffers[bufferIndex].mNumberChannels; ++channelIndex) {
				switch(mSourceFormat.mBitsPerChannel) {
					case 32:
					{
						// Native floats
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							float *input = static_cast<float *>(inputBuffer->mBuffers[bufferIndex].mData);
							double *output = static_cast<double *>(outputBuffer->mBuffers[bufferIndex + channelIndex].mData);
							
							vDSP_vspdp(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
						}
						// Swapped floats
						else {
							int *input = static_cast<int *>(inputBuffer->mBuffers[bufferIndex].mData);
							double *output = static_cast<double *>(outputBuffer->mBuffers[bufferIndex + channelIndex].mData);
							
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<float>(OSSwapInt32(*input));
								input += inputBuffer->mBuffers[bufferIndex].mNumberChannels;
							}
						}
						
						break;
					}
					
					// 64 bit floats not (yet) supported
				}
			} // channel
		} // buffer
	} // float

	else {

		double maxSignedSampleValue = 1L << (mSourceFormat.mBitsPerChannel - 1);		
		if(kAudioFormatFlagIsAlignedHigh & mSourceFormat.mFormatFlags)
			maxSignedSampleValue *= 1L << ((8 * mSourceFormat.mBytesPerFrame) - mSourceFormat.mBitsPerChannel);

		double unsignedSampleDelta = -1 * maxSignedSampleValue;

		for(UInt32 bufferIndex = 0; bufferIndex < inputBuffer->mNumberBuffers; ++bufferIndex) {
			for(UInt32 channelIndex = 0; channelIndex < inputBuffer->mBuffers[bufferIndex].mNumberChannels; ++channelIndex) {

				double *output = static_cast<double *>(outputBuffer->mBuffers[bufferIndex + channelIndex].mData);

				switch(mSourceFormat.mBytesPerFrame) {
					case 1:
					{
						if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
							char *input = static_cast<char *>(inputBuffer->mBuffers[bufferIndex].mData);
							vDSP_vflt8D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
						}
						else {
							unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[bufferIndex].mData);
							vDSP_vfltu8D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
							vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						}

						break;
					}

					case 2:
					{
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								short *input = static_cast<short *>(inputBuffer->mBuffers[bufferIndex].mData);
								vDSP_vflt16D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
							}
							else {
								unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[bufferIndex].mData);
								vDSP_vfltu16D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						// Swap bytes
						else {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								short *input = static_cast<short *>(inputBuffer->mBuffers[bufferIndex].mData) + channelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<short>(OSSwapInt16(*input));
									input += inputBuffer->mBuffers[bufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[bufferIndex].mData) + channelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<unsigned short>(OSSwapInt16(*input));
									input += inputBuffer->mBuffers[bufferIndex].mNumberChannels;
								}

								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}

						break;
					}

					case 3:
					{
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[bufferIndex].mData) + 3 * channelIndex;
						double *outputAlias = output;

						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								*outputAlias++ = input[0] | (input[1] << 8) | (input[2] << 16);
								input += 3 * inputBuffer->mBuffers[bufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								*outputAlias++ = (input[0] << 16) | (input[1] << 8) | input[2];
								input += 3 * inputBuffer->mBuffers[bufferIndex].mNumberChannels;
							}
						}
						
						break;
					}
						
					case 4:
					{
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int *input = static_cast<int *>(inputBuffer->mBuffers[bufferIndex].mData);
								vDSP_vflt32D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
							}
							else {
								unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[bufferIndex].mData);
								vDSP_vfltu32D(input + channelIndex, 1 + channelIndex, output, 1, frameCount);
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						// Swap bytes
						else {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int *input = static_cast<int *>(inputBuffer->mBuffers[bufferIndex].mData) + channelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<int>(OSSwapInt32(*input));
									input += inputBuffer->mBuffers[bufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[bufferIndex].mData) + channelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<unsigned int>(OSSwapInt32(*input));
									input += inputBuffer->mBuffers[bufferIndex].mNumberChannels;
								}
								
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						
						break;
					}
				}

				// Normalize to [-1, 1)
				vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

			} // channel
		} // buffer
	}

	// We're finished!
	return frameCount;	
}
