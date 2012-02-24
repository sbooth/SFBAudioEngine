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

#include <stdexcept>
#include <Accelerate/Accelerate.h>

#include "PCMConverter.h"

#pragma mark Static Methods

static inline void ScaleAndClip(double *buffer, UInt32 frameCount, double scale, double minSample, double maxSample)
{
	vDSP_vsmulD(buffer, 1, &scale, buffer, 1, frameCount);
	vDSP_vclipD(buffer, 1, &minSample, &maxSample, buffer, 1, frameCount);
}

static inline void ScaleAddAndClip(double *buffer, UInt32 frameCount, double scale, double delta, double minSample, double maxSample)
{
	vDSP_vsmsaD(buffer, 1, &scale, &delta, buffer, 1, frameCount);
	vDSP_vclipD(buffer, 1, &minSample, &maxSample, buffer, 1, frameCount);
}

#pragma mark Creation and Destruction

PCMConverter::PCMConverter(const AudioStreamBasicDescription& sourceFormat, const AudioStreamBasicDescription& destinationFormat)
	: AudioConverter(sourceFormat, destinationFormat)
{
	if(kAudioFormatLinearPCM != mSourceFormat.mFormatID || kAudioFormatLinearPCM != mDestinationFormat.mFormatID)
		throw std::runtime_error("Only PCM to PCM conversions are supported by PCMConverter");

	if(mSourceFormat.mSampleRate != mDestinationFormat.mSampleRate)
		throw std::runtime_error("Sample rate conversion is not supported by PCMConverter");

	if(!(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags) || !(kAudioFormatFlagIsPacked & mSourceFormat.mFormatFlags) || (kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) || (8 * sizeof(double) != mSourceFormat.mBitsPerChannel))
		throw std::runtime_error("Only 64 bit floating point source formats are supported by PCMConverter");
	
	if(1 < mSourceFormat.mChannelsPerFrame && !(kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags))
		throw std::runtime_error("Only deinterleaved source formats are supported by PCMConverter");

	// Set up the default channel map
	for(UInt32 i = 0; i < std::min(mSourceFormat.mChannelsPerFrame, mDestinationFormat.mChannelsPerFrame); ++i)
		mChannelMap[i] = i;
}

PCMConverter::~PCMConverter()
{}

UInt32 PCMConverter::Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	if(nullptr == inputBuffer || nullptr == outputBuffer)
		return 0;
	
	// Nothing to do
	if(0 == frameCount) {
		for(UInt32 outputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex)
			outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = 0;
		return 0;
	}
	
	UInt32 interleavedChannelCount = kAudioFormatFlagIsNonInterleaved & mDestinationFormat.mFormatFlags ? 1 : mDestinationFormat.mChannelsPerFrame;
	UInt32 sampleWidth = mDestinationFormat.mBytesPerFrame / interleavedChannelCount;
	
	// Float-to-float conversion
	if(kAudioFormatFlagIsFloat & mDestinationFormat.mFormatFlags) {
		switch(mDestinationFormat.mBitsPerChannel) {
			case 32:	return ConvertToFloat(inputBuffer, outputBuffer, frameCount);
			case 64:	return ConvertToDouble(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported floating point size");
		}
	}
	
	// Packed conversions
	else if(kAudioFormatFlagIsPacked & mDestinationFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertToPacked8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertToPacked16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertToPacked24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertToPacked32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported packed sample width");
		}
	}
	
	// High-aligned conversions
	else if(kAudioFormatFlagIsAlignedHigh & mDestinationFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertToHighAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertToHighAligned16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertToHighAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertToHighAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported high-aligned sample width");
		}
	}
	
	// Low-aligned conversions
	else {
		switch(sampleWidth) {
			case 1:		return ConvertToLowAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertToLowAligned16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertToLowAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertToLowAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported low-aligned sample width");
		}
	}
	
	return 0;	
}

#pragma mark Float Conversions

UInt32 
PCMConverter::ConvertToFloat(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				float *output = static_cast<float *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vdpsp(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(float));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				float *output = static_cast<float *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vdpsp(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Swapped floats
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					unsigned int *swappedOutput = static_cast<unsigned int *>(outputBuffer->mBuffers[outputBufferIndex].mData) + outputChannelIndex;
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*swappedOutput = OSSwapInt32(*swappedOutput);
						swappedOutput += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(float));
			}
		}
	}
	
	return frameCount;
}

UInt32 
PCMConverter::ConvertToDouble(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	double zero = 0;

	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsaddD(input, 1, &zero, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(double));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsaddD(input, 1, &zero, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Swapped floats
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					unsigned int *swappedOutput = static_cast<unsigned int *>(outputBuffer->mBuffers[outputBufferIndex].mData) + outputChannelIndex;
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*swappedOutput = OSSwapInt32(*swappedOutput);
						swappedOutput += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(double));
			}
		}
	}
	
	return frameCount;
}

#pragma mark Packed Conversions

UInt32 
PCMConverter::ConvertToPacked8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
				vDSP_vfix8D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(char));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
				vDSP_vfixu8D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(unsigned char));
			}
		}
	}
	
	return frameCount;
}

UInt32 
PCMConverter::ConvertToPacked16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				short *output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
				vDSP_vfix16D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Byte swap if required
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					output += outputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output = static_cast<short>(OSSwapInt16(*output));
						output += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(short));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				unsigned short *output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
				vDSP_vfixu16D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Byte swap if required
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					output += outputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output = static_cast<unsigned short>(OSSwapInt16(*output));
						output += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned short));
			}
		}
	}
	
	return frameCount;
}

UInt32 
PCMConverter::ConvertToPacked24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);

				ScaleAndClip(input, frameCount, scale, -scale, scale - 1);

				int sample;
				if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
					for(UInt32 count = 0; count < frameCount; ++count) {
						sample = static_cast<int>(*input++);
						output[0] = static_cast<unsigned char>((sample >> 16) & 0xff);
						output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
						output[2] = static_cast<unsigned char>(sample & 0xff);
						output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				else {
					for(UInt32 count = 0; count < frameCount; ++count) {
						sample = static_cast<int>(*input++);
						output[0] = static_cast<unsigned char>(sample & 0xff);
						output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
						output[2] = static_cast<unsigned char>((sample >> 16) & 0xff);
						output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);
				
				ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
				
				unsigned int sample;
				if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
					for(UInt32 count = 0; count < frameCount; ++count) {
						sample = static_cast<unsigned int>(*input++);
						output[0] = static_cast<unsigned char>((sample >> 16) & 0xff);
						output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
						output[2] = static_cast<unsigned char>(sample & 0xff);
						output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				else {
					for(UInt32 count = 0; count < frameCount; ++count) {
						sample = static_cast<unsigned int>(*input++);
						output[0] = static_cast<unsigned char>(sample & 0xff);
						output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
						output[2] = static_cast<unsigned char>((sample >> 16) & 0xff);
						output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
			}
		}
	}
	
	return frameCount;
}

UInt32 
PCMConverter::ConvertToPacked32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{	
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				int *output = static_cast<int *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
				vDSP_vfix32D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Byte swap if required
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					output += outputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output = static_cast<int>(OSSwapInt32(*output));
						output += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(int));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
				if(mChannelMap.end() == it)
					continue;
				
				double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
				unsigned int *output = static_cast<unsigned int *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
				vDSP_vfixu32D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				// Byte swap if required
				if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
					output += outputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output = static_cast<unsigned int>(OSSwapInt32(*output));
						output += outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
					}
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned int));
			}
		}
	}
	
	return frameCount;
}

#pragma mark High-Aligned Conversions

UInt32 
PCMConverter::ConvertToHighAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 7:
		{
			UInt32 unusedBits = 8 - mDestinationFormat.mBitsPerChannel;
			double scale = 1u << (7 - unusedBits);

			return ConvertToPacked8(inputBuffer, outputBuffer, frameCount, scale);
		}
			
		default:
			throw std::runtime_error("Unsupported 8-bit high-aligned bit depth");
	}
	
	return 0;
}

UInt32 
PCMConverter::ConvertToHighAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, 
#if __BIG_ENDIAN__
									output + (2 * outputChannelIndex), 
#else
									output + (2 * outputChannelIndex) + 1, 
#endif
									2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(short));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, 
#if __BIG_ENDIAN__
									 output + (2 * outputChannelIndex), 
#else
									 output + (2 * outputChannelIndex) + 1, 
#endif
									 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned short));
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 15:
		{
			UInt32 unusedBits = 16 - mDestinationFormat.mBitsPerChannel;
			double scale = 1u << (15 - unusedBits);
			
			return ConvertToPacked16(inputBuffer, outputBuffer, frameCount, scale);
		}
			
		default:
			throw std::runtime_error("Unsupported 16-bit high-aligned bit depth");
	}
	
	return 0;
}

UInt32 
PCMConverter::ConvertToHighAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, output + (3 * outputChannelIndex), 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);

						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, output + (3 * outputChannelIndex), 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}
			
		case 9 ... 16:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						
						short sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<short>(*input++);
								output[0] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = 0;
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<short>(*input++);
								output[0] = static_cast<unsigned char>(sample & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = 0;
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						
						unsigned short sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned short>(*input++);
								output[0] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = 0;
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned short>(*input++);
								output[0] = static_cast<unsigned char>(sample & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = 0;
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}

		case 17 ... 23:
		{
			UInt32 unusedBits = 24 - mDestinationFormat.mBitsPerChannel;
			double scale = 1u << (23 - unusedBits);
			
			return ConvertToPacked24(inputBuffer, outputBuffer, frameCount, scale);
		}
			
		default:
			throw std::runtime_error("Unsupported 24-bit high-aligned bit depth");
	}
	
	return 0;
}

UInt32 
PCMConverter::ConvertToHighAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, 
#if __BIG_ENDIAN__
									output + (4 * outputChannelIndex), 
#else
									output + (4 * outputChannelIndex) + 3, 
#endif
									4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(int));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, 
#if __BIG_ENDIAN__
									 output + (4 * outputChannelIndex), 
#else
									 output + (4 * outputChannelIndex) + 3, 
#endif
									 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned int));
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 16:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;

						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						short *output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData);

						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix16D(input, 1, 
#if __BIG_ENDIAN__
									 output + (2 * outputChannelIndex), 
#else
									 output + (2 * outputChannelIndex) + 1, 
#endif
									 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);

						// Byte swap if required
						if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
#if __BIG_ENDIAN__
							output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex);
#else
							output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex) + 1;
#endif
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output = static_cast<short>(OSSwapInt16(*output));
								output += 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}

						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(int));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned short *output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu16D(input, 1, 
#if __BIG_ENDIAN__
									  output + (2 * outputChannelIndex), 
#else
									  output + (2 * outputChannelIndex) + 1, 
#endif
									  2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);

						// Byte swap if required
						if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
#if __BIG_ENDIAN__
							output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex);
#else
							output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex) + 1;
#endif
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output = static_cast<unsigned short>(OSSwapInt16(*output));
								output += 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}

						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned int));
					}
				}
			}
			
			return frameCount;
		}

		case 17 ... 24:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (4 * outputChannelIndex);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						
						int sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<int>(*input++);
								output[0] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>(sample & 0xff);
								output[3] = 0;
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<int>(*input++);
								output[0] = static_cast<unsigned char>(sample & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[3] = 0;
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 4 * sizeof(unsigned char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (4 * outputChannelIndex);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						
						unsigned int sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned int>(*input++);
								output[0] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>(sample & 0xff);
								output[3] = 0;
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned int>(*input++);
								output[0] = static_cast<unsigned char>(sample & 0xff);
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[3] = 0;
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 4 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}

		case 25 ... 31:
		{
			UInt32 unusedBits = 32 - mDestinationFormat.mBitsPerChannel;
			double scale = 1u << (31 - unusedBits);
			
			return ConvertToPacked32(inputBuffer, outputBuffer, frameCount, scale);
		}

		default:
			throw std::runtime_error("Unsupported 32-bit high-aligned bit depth");
	}

	return 0;
}

#pragma mark Low-Aligned Conversions

UInt32 
PCMConverter::ConvertToLowAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 7:
			return ConvertToPacked8(inputBuffer, outputBuffer, frameCount, 1u << (mDestinationFormat.mBitsPerChannel - 1));
			
		default:
			throw std::runtime_error("Unsupported 8-bit low-aligned bit depth");
	}
	
	return 0;
}

UInt32 
PCMConverter::ConvertToLowAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, 
#if __BIG_ENDIAN__
									output + (2 * outputChannelIndex) + 1, 
#else
									output + (2 * outputChannelIndex), 
#endif
									2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(short));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, 
#if __BIG_ENDIAN__
									 output + (2 * outputChannelIndex) + 1, 
#else
									 output + (2 * outputChannelIndex), 
#endif
									 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned short));
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 15:
			return ConvertToPacked16(inputBuffer, outputBuffer, frameCount, 1u << (mDestinationFormat.mBitsPerChannel - 1));

		default:
			throw std::runtime_error("Unsupported 16-bit low-aligned bit depth");
	}

	return 0;
}

UInt32 
PCMConverter::ConvertToLowAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, output + outputChannelIndex, 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, output + outputChannelIndex, 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}
			
		case 9 ... 16:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						
						short sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<short>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>(sample & 0xff);
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<short>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (3 * outputChannelIndex);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						
						unsigned short sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned short>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[2] = static_cast<unsigned char>(sample & 0xff);
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned short>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output += 3 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 3 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}

		case 17 ... 23:
			return ConvertToPacked24(inputBuffer, outputBuffer, frameCount, 1u << (mDestinationFormat.mBitsPerChannel - 1));
			
		default:
			throw std::runtime_error("Unsupported 24-bit low-aligned bit depth");
	}
	
	return 0;
}

UInt32 
PCMConverter::ConvertToLowAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mDestinationFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix8D(input, 1, 
#if __BIG_ENDIAN__
									output + (4 * outputChannelIndex) + 3, 
#else
									output + (4 * outputChannelIndex), 
#endif
									4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(int));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu8D(input, 1, 
#if __BIG_ENDIAN__
									 output + (4 * outputChannelIndex) + 3, 
#else
									 output + (4 * outputChannelIndex), 
#endif
									 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned int));
					}
				}
			}
			
			return frameCount;
		}
			
		case 9 ... 16:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						short *output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						vDSP_vfix16D(input, 1, 
#if __BIG_ENDIAN__
									 output + (2 * outputChannelIndex) + 1, 
#else
									 output + (2 * outputChannelIndex), 
#endif
									 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						// Byte swap if required
						if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
#if __BIG_ENDIAN__
							output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex);
#else
							output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex) + 1;
#endif
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output = static_cast<short>(OSSwapInt16(*output));
								output += 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(int));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned short *output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						vDSP_vfixu16D(input, 1, 
#if __BIG_ENDIAN__
									  output + (2 * outputChannelIndex) + 1, 
#else
									  output + (2 * outputChannelIndex), 
#endif
									  2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
						
						// Byte swap if required
						if(kAudioFormatFlagsNativeEndian != (kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags)) {
#if __BIG_ENDIAN__
							output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex);
#else
							output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (2 * outputChannelIndex) + 1;
#endif
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output = static_cast<unsigned short>(OSSwapInt16(*output));
								output += 2 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(unsigned int));
					}
				}
			}
			
			return frameCount;
		}
			
		case 17 ... 24:
		{
			double scale = 1u << (mDestinationFormat.mBitsPerChannel - 1);
			
			if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (4 * outputChannelIndex);
						
						ScaleAndClip(input, frameCount, scale, -scale, scale - 1);
						
						int sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<int>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[3] = static_cast<unsigned char>(sample & 0xff);
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<int>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[3] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 4 * sizeof(unsigned char));
					}
				}
			}
			else {
				for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
					for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
						std::map<int, int>::iterator it = mChannelMap.find(inputBufferIndex);
						if(mChannelMap.end() == it)
							continue;
						
						double *input = static_cast<double *>(inputBuffer->mBuffers[it->second].mData);
						unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + (4 * outputChannelIndex);
						
						ScaleAddAndClip(input, frameCount, scale, scale, 0, scale);
						
						unsigned int sample;
						if(kAudioFormatFlagIsBigEndian & mDestinationFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned int>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[3] = static_cast<unsigned char>(sample & 0xff);
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
								sample = static_cast<unsigned int>(*input++);
								output[0] = 0;
								output[1] = static_cast<unsigned char>(sample & 0xff);
								output[2] = static_cast<unsigned char>((sample >> 8) & 0xff);
								output[3] = static_cast<unsigned char>((sample >> 16) & 0xff);
								output += 4 * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels;
							}
						}
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * 4 * sizeof(unsigned char));
					}
				}
			}
			
			return frameCount;
		}

		case 25 ... 31:
			return ConvertToPacked32(inputBuffer, outputBuffer, frameCount, 1u << (mDestinationFormat.mBitsPerChannel - 1));
			
		default:
			throw std::runtime_error("Unsupported 32-bit low-aligned bit depth");
	}
	
	return 0;
}
