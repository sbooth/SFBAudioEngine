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

	if(!(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags) && !(1 == sourceFormat.mBytesPerFrame || 2 == sourceFormat.mBytesPerFrame || 4 == sourceFormat.mBytesPerFrame))
		throw std::runtime_error("Only 1, 2, and 4 byte unpacked frame sizes are supported by DeinterleavingFloatConverter");
	
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
		for(UInt32 outputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex)
			outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = 0;
		
		return 0;
	}

	// Float-to-float
	if(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				switch(mSourceFormat.mBitsPerChannel) {
					case 32:
					{
						// Native floats
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							float *input = static_cast<float *>(inputBuffer->mBuffers[inputBufferIndex].mData);
							double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
							
							vDSP_vspdp(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						}
						// Swapped floats
						else {
							unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
							double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
							
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<float>(OSSwapInt32(*input));
								input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						
						break;
					}
					
					// 64 bit floats not (yet) supported
				}

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;

			} // channel
		} // buffer
	} // float

	else {
		double maxSignedSampleValue = 1L << (mSourceFormat.mBitsPerChannel - 1);		
		if(kAudioFormatFlagIsAlignedHigh & mSourceFormat.mFormatFlags)
			maxSignedSampleValue *= 1L << ((8 * mSourceFormat.mBytesPerFrame) - mSourceFormat.mBitsPerChannel);

		double unsignedSampleDelta = -1 * maxSignedSampleValue;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {

				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				switch(kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags ? mSourceFormat.mBytesPerFrame : mSourceFormat.mBytesPerFrame / mSourceFormat.mChannelsPerFrame) {
					case 1:
					{
						if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
							char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
							vDSP_vflt8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						}
						else {
							unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
							vDSP_vfltu8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
							vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						}

						break;
					}

					case 2:
					{
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
								vDSP_vflt16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
							}
							else {
								unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
								vDSP_vfltu16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						// Swap bytes
						else {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<short>(OSSwapInt16(*input));
									input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<unsigned short>(OSSwapInt16(*input));
									input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}

								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}

						break;
					}

					case 3:
					{
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + 3 * inputChannelIndex;
						double *outputAlias = output;

						if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int value;
								for(UInt32 count = 0; count < frameCount; ++count) {
									value = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
									*outputAlias++ = static_cast<double>(value);
									input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned int value;
								for(UInt32 count = 0; count < frameCount; ++count) {
									value = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
									*outputAlias++ = static_cast<double>(value);
									input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
						}
						else {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int value;
								for(UInt32 count = 0; count < frameCount; ++count) {
									value = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
									*outputAlias++ = static_cast<double>(value);
									input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned int value;
								for(UInt32 count = 0; count < frameCount; ++count) {
									value = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
									*outputAlias++ = static_cast<double>(value);
									input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
						}

						double specialNormFactor = 256;
						vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);

						if(!(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags))
							vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);

						break;
					}
						
					case 4:
					{
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int *input = static_cast<int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
								vDSP_vflt32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
							}
							else {
								unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
								vDSP_vfltu32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						// Swap bytes
						else {
							if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
								int *input = static_cast<int *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<int>(OSSwapInt32(*input));
									input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
							}
							else {
								unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
								double *outputAlias = output;
								
								for(UInt32 count = 0; count < frameCount; ++count) {
									*outputAlias++ = static_cast<unsigned int>(OSSwapInt32(*input));
									input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
								}
								
								vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
							}
						}
						
						break;
					}
				}

				// Normalize to [-1, 1)
				vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;

			} // channel
		} // buffer
	}

	// We're finished!
	return frameCount;	
}
