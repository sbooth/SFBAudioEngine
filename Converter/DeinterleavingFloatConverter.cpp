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

#include "DeinterleavingFloatConverter.h"

DeinterleavingFloatConverter::DeinterleavingFloatConverter(const AudioStreamBasicDescription& sourceFormat)
{
	if(kAudioFormatLinearPCM != sourceFormat.mFormatID)
		throw std::runtime_error("Only PCM input formats are supported by DeinterleavingFloatConverter");

	// Validate the input format is supported (a crash is better than no audio data)

	// Floating point input
	if(kAudioFormatFlagIsFloat & sourceFormat.mFormatFlags) {
		// Supported floating point sample sizes are 32 (float) and 64 (double)
		if(!(32 == sourceFormat.mBitsPerChannel || 64 == sourceFormat.mBitsPerChannel))
			throw std::runtime_error("Only 32 and 64 bit floating point sample sizes are supported by DeinterleavingFloatConverter");
	}
	// Integer input
	else {
		// Supported packed integer sample sizes are 8, 16, 24, and 32
		if((kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags) && !(8 == sourceFormat.mBitsPerChannel || 16 == sourceFormat.mBitsPerChannel || 24 == sourceFormat.mBitsPerChannel || 32 == sourceFormat.mBitsPerChannel))
			throw std::runtime_error("Only 8, 16, 24, and 32 bit packed integer sample sizes are supported by DeinterleavingFloatConverter");
		
		UInt32 interleavedChannelCount = kAudioFormatFlagIsNonInterleaved & sourceFormat.mFormatFlags ? 1 : sourceFormat.mChannelsPerFrame;
		UInt32 sampleWidth = sourceFormat.mBytesPerFrame / interleavedChannelCount;
		
		// High- and low- alignment is supported for 1, 2, 3, and 4 byte sample sizes
		if(!(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags) && !(1 == sampleWidth || 2 == sampleWidth || 3 == sampleWidth || 4 == sampleWidth))
			throw std::runtime_error("Only 1, 2, 3, or 4 byte unpacked frame sizes are supported by DeinterleavingFloatConverter");
	}

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
	assert(nullptr != inputBuffer);
	assert(nullptr != outputBuffer);
	
	// Nothing to do
	if(0 == frameCount) {
		for(UInt32 outputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex)
			outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = 0;
		return 0;
	}

	UInt32 interleavedChannelCount = kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags ? 1 : mSourceFormat.mChannelsPerFrame;
	UInt32 sampleWidth = mSourceFormat.mBytesPerFrame / interleavedChannelCount;
	
	// Float-to-float conversion
	if(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags) {
		switch(mSourceFormat.mBitsPerChannel) {
			case 32:	return ConvertFromFloat(inputBuffer, outputBuffer, frameCount);
			case 64:	return ConvertFromDouble(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported floating point size");
		}
	}

	// Packed conversions
	else if(kAudioFormatFlagIsPacked & mSourceFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromPacked16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertFromPacked24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported packed sample width");
		}
	}
	
	// High-aligned conversions
	else if(kAudioFormatFlagIsAlignedHigh & mSourceFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertFromHighAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromHighAligned16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertFromHighAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromHighAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported high-aligned sample width");
		}
	}

	// Low-aligned conversions
	else {
		switch(sampleWidth) {
			case 1:		return ConvertFromLowAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromLowAligned16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertFromLowAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromLowAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported low-aligned sample width");
		}
	}
	
	return 0;
}

#pragma mark Float Conversions

UInt32
DeinterleavingFloatConverter::ConvertFromFloat(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				float *input = static_cast<float *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				vDSP_vspdp(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				CFSwappedFloat32 *input = static_cast<CFSwappedFloat32 *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				for(UInt32 count = 0; count < frameCount; ++count) {
					*output++ = static_cast<double>(CFConvertFloatSwappedToHost(*input));
					input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
				}

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromDouble(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		double zero = 0;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				// It is faster to add 0 than it is to loop through the samples
				vDSP_vsaddD(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, &zero, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				CFSwappedFloat64 *input = static_cast<CFSwappedFloat64 *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				for(UInt32 count = 0; count < frameCount; ++count) {
					*output++ = CFConvertDoubleSwappedToHost(*input);
					input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
				}

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

#pragma mark Packed Conversions

UInt32
DeinterleavingFloatConverter::ConvertFromPacked8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{	
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				vDSP_vflt8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		double unsignedSampleDelta = -scale;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vfltu8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);				
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}

	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{	
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
					vDSP_vflt16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				else {
					input += inputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<short>(OSSwapInt16(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				}

				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		double unsignedSampleDelta = -scale;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
					vDSP_vfltu16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				else {
					input += inputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned short>(OSSwapInt16(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				}

				vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	double normalizingFactor = 1u << 8;

	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
				}
				else {
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
				}

				output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		double unsignedSampleDelta = -scale;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
				}
				else {
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
				}
				
				output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
				vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}

	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount, double scale)
{
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				int *input = static_cast<int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
					vDSP_vflt32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				else {
					input += inputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>(OSSwapInt32(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				}
				
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	else {
		double unsignedSampleDelta = -scale;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
					vDSP_vfltu32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				else {
					input += inputChannelIndex;
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>(OSSwapInt32(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				}
				
				vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

#pragma mark High-Aligned Conversions

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 7:
			return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount, 1u << 7);
			
		default:
			throw std::runtime_error("Unsupported 8-bit high-aligned bit depth");
	}
	
	return 0;
}

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vflt8D(
#if __BIG_ENDIAN__
									input + (2 * inputChannelIndex), 
#else
									input + (2 * inputChannelIndex) + 1, 
#endif
									2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vfltu8D(
#if ___BIG_ENDIAN__
									 input + (2 * inputChannelIndex),
#else
									 input + (2 * inputChannelIndex) + 1,
#endif
									 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 15:
			return ConvertFromPacked16(inputBuffer, outputBuffer, frameCount, 1u << 15);

		default:
			throw std::runtime_error("Unsupported 16-bit high-aligned bit depth");
	}

	return 0;
}

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		case 9 ... 16:
		case 17 ... 23:
			return ConvertFromPacked24(inputBuffer, outputBuffer, frameCount, 1u << 23);

		default:
			throw std::runtime_error("Unsupported 24-bit high-aligned bit depth");
	}

	return 0;
}
	
UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vflt8D(
#if __BIG_ENDIAN__
									input + (4 * inputChannelIndex), 
#else
									input + (4 * inputChannelIndex) + 3, 
#endif
									4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vfltu8D(
#if __BIG_ENDIAN__
									 input + (4 * inputChannelIndex), 
#else
									 input + (4 * inputChannelIndex) + 3, 
#endif
									 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}
			
		case 9 ... 16:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
							vDSP_vflt16D(
#if __BIG_ENDIAN__
										 input + (2 * inputChannelIndex), 
#else
										 input + (2 * inputChannelIndex) + 1, 
#endif
										 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						else {
							input += 2 * inputChannelIndex;
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<short>(OSSwapInt16(*input));
								input += 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
							output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						}
						
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
							vDSP_vfltu16D(
#if __BIG_ENDIAN__
										  input + (2 * inputChannelIndex), 
#else
										  input + (2 * inputChannelIndex) + 1, 
#endif
										  2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						else {
							input += 2 * inputChannelIndex;
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<unsigned short>(OSSwapInt16(*input));
								input += 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
							output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						}
						
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}
	
		case 17 ... 24:
		{
			double scale = 1u << 23;
			double unsignedSampleDelta = -scale;
			double normalizingFactor = 1u << 8;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
#else
								*output++ = static_cast<int>((input[1] << 24) | (input[2] << 16) | (input[3] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
#else
								*output++ = static_cast<int>((input[3] << 24) | (input[2] << 16) | (input[1] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
#else
								*output++ = static_cast<unsigned int>((input[1] << 24) | (input[2] << 16) | (input[3] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
#else
								*output++ = static_cast<unsigned int>((input[3] << 24) | (input[2] << 16) | (input[1] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 25 ... 31:
			return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount, 1u << 31);

		default:
			throw std::runtime_error("Unsupported 32-bit high-aligned bit depth");
	}

	return 0;
}

#pragma mark Low-Aligned Conversions

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 7:
			return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount, 1u << (mSourceFormat.mBitsPerChannel - 1));
			
		default:
			throw std::runtime_error("Unsupported 8-bit low-aligned bit depth");
	}
	
	return 0;
}

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vflt8D(
#if __BIG_ENDIAN__
									input + (2 * inputChannelIndex) + 1, 
#else
									input + (2 * inputChannelIndex), 
#endif
									2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vfltu8D(
#if __BIG_ENDIAN__
									 input + (2 * inputChannelIndex) + 1, 
#else
									 input + (2 * inputChannelIndex), 
#endif
									 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 15:
			return ConvertFromPacked16(inputBuffer, outputBuffer, frameCount, 1u << (mSourceFormat.mBitsPerChannel - 1));

		default:
			throw std::runtime_error("Unsupported 16-bit low-aligned bit depth");
	}

	return 0;
}

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		case 9 ... 16:
		case 17 ... 23:
			return ConvertFromPacked24(inputBuffer, outputBuffer, frameCount, 1u << (mSourceFormat.mBitsPerChannel - 1));

		default:
			throw std::runtime_error("Unsupported 24-bit low-aligned bit depth");
	}
	
	return 0;
}

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	switch(mSourceFormat.mBitsPerChannel) {
		case 1 ... 8:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vflt8D(
#if __BIG_ENDIAN__
									input + (4 * inputChannelIndex) + 3, 
#else
									input + (4 * inputChannelIndex), 
#endif
									4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vfltu8D(
#if __BIG_ENDIAN__
									 input + (4 * inputChannelIndex) + 3, 
#else
									 input + (4 * inputChannelIndex), 
#endif
									 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 9 ... 16:
		{
			double scale = 1u << (mSourceFormat.mBitsPerChannel - 1);
			double unsignedSampleDelta = -scale;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
							vDSP_vflt16D(
#if __BIG_ENDIAN__
										 input + (2 * inputChannelIndex) + 1, 
#else
										 input + (2 * inputChannelIndex), 
#endif
										 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						else {
							input += 2 * inputChannelIndex;
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<short>(OSSwapInt16(*input));
								input += 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
							output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						}

						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags))
							vDSP_vfltu16D(
#if __BIG_ENDIAN__
										  input + (2 * inputChannelIndex) + 1, 
#else
										  input + (2 * inputChannelIndex), 
#endif
										  2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
						else {
							input += 2 * inputChannelIndex;
							for(UInt32 count = 0; count < frameCount; ++count) {
								*output++ = static_cast<unsigned short>(OSSwapInt16(*input));
								input += 2 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
							output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						}

						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 17 ... 24:
		{
			double scale = 1u << 23;
			double unsignedSampleDelta = -scale;
			double normalizingFactor = 1u << 8;
			
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<int>((input[1] << 24) | (input[2] << 16) | (input[3] << 8));
#else
								*output++ = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<int>((input[3] << 24) | (input[2] << 16) | (input[1] << 8));
#else
								*output++ = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<unsigned int>((input[1] << 24) | (input[2] << 16) | (input[3] << 8));
#else
								*output++ = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						else {
							for(UInt32 count = 0; count < frameCount; ++count) {
#if __BIG_ENDIAN__
								*output++ = static_cast<unsigned int>((input[3] << 24) | (input[2] << 16) | (input[1] << 8));
#else
								*output++ = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
#endif
								input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
							}
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &normalizingFactor, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &scale, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			
			return frameCount;
		}

		case 25 ... 31:
			return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount, 1u << (mSourceFormat.mBitsPerChannel - 1));

		default:
			throw std::runtime_error("Unsupported 32-bit low-aligned bit depth");
	}

	return 0;
}
