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

#include "PCMConverter.h"


PCMConverter::PCMConverter(const AudioStreamBasicDescription& sourceFormat, const AudioStreamBasicDescription& destinationFormat)
	: AudioConverter(sourceFormat, destinationFormat)
{
	if(kAudioFormatLinearPCM != mSourceFormat.mFormatID || kAudioFormatLinearPCM != mDestinationFormat.mFormatID)
		throw std::runtime_error("Only PCM to PCM conversions are supported by PCMConverter");

	if(mSourceFormat.mSampleRate != mDestinationFormat.mSampleRate)
		throw std::runtime_error("Sample rate conversion is not supported by PCMConverter");

	if(!(kAudioFormatFlagsNativeFloatPacked & mSourceFormat.mFormatFlags) || (8 * sizeof(double) != mSourceFormat.mBitsPerChannel))
		throw std::runtime_error("Only 64 bit floating point source formats are supported by PCMConverter");
	
	if(1 < mSourceFormat.mChannelsPerFrame && !(kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags))
		throw std::runtime_error("Only deinterleaved source formats are supported by PCMConverter");

//	if(mSourceFormat.mChannelsPerFrame != mDestinationFormat.mChannelsPerFrame)
//		throw std::runtime_error("Channel mapping is not supported by PCMConverter");
}

PCMConverter::~PCMConverter()
{}

UInt32 PCMConverter::Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);
	
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
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				float *output = static_cast<float *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vdpsp(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(float));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
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
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsaddD(input, 1, &zero, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(double));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
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
	double maxSignedSampleValue = scale;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				char *output = static_cast<char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vfixr8D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * outputBuffer->mBuffers[outputBufferIndex].mNumberChannels * sizeof(char));
			}
		}
	}
	else {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vsaddD(input, 1, &unsignedSampleDelta, input, 1, frameCount);
				vDSP_vfixru8D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(unsigned char));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

UInt32 
PCMConverter::ConvertToPacked16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	double maxSignedSampleValue = scale;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				short *output = static_cast<short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vfixr16D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
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
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				unsigned short *output = static_cast<unsigned short *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vsaddD(input, 1, &unsignedSampleDelta, input, 1, frameCount);
				vDSP_vfixru16D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
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
	double maxSignedSampleValue = scale;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + 3 * outputChannelIndex;

				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);

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
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				unsigned char *output = static_cast<unsigned char *>(outputBuffer->mBuffers[outputBufferIndex].mData) + 3 * outputChannelIndex;
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vsaddD(input, 1, &unsignedSampleDelta, input, 1, frameCount);
				
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
	double maxSignedSampleValue = scale;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagIsSignedInteger & mDestinationFormat.mFormatFlags) {
		for(UInt32 outputBufferIndex = 0, inputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex) {
			for(UInt32 outputChannelIndex = 0; outputChannelIndex < outputBuffer->mBuffers[outputBufferIndex].mNumberChannels; ++outputChannelIndex, ++inputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				int *output = static_cast<int *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vfixr32D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
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
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				unsigned int *output = static_cast<unsigned int *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsmulD(input, 1, &maxSignedSampleValue, input, 1, frameCount);
				vDSP_vsaddD(input, 1, &unsignedSampleDelta, input, 1, frameCount);
				vDSP_vfixru32D(input, 1, output + outputChannelIndex, outputBuffer->mBuffers[outputBufferIndex].mNumberChannels, frameCount);
				
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
	return ConvertToPacked8(inputBuffer, outputBuffer, frameCount);
}

UInt32 
PCMConverter::ConvertToHighAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked16(inputBuffer, outputBuffer, frameCount);
}

UInt32 
PCMConverter::ConvertToHighAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked24(inputBuffer, outputBuffer, frameCount);
}

UInt32 
PCMConverter::ConvertToHighAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked32(inputBuffer, outputBuffer, frameCount);
}

#pragma mark Low-Aligned Conversions

UInt32 
PCMConverter::ConvertToLowAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked8(inputBuffer, outputBuffer, frameCount, 1u << mDestinationFormat.mBitsPerChannel);
}

UInt32 
PCMConverter::ConvertToLowAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked16(inputBuffer, outputBuffer, frameCount, 1u << mDestinationFormat.mBitsPerChannel);
}

UInt32 
PCMConverter::ConvertToLowAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked24(inputBuffer, outputBuffer, frameCount, 1u << mDestinationFormat.mBitsPerChannel);
}

UInt32 
PCMConverter::ConvertToLowAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertToPacked32(inputBuffer, outputBuffer, frameCount, 1u << mDestinationFormat.mBitsPerChannel);
}
