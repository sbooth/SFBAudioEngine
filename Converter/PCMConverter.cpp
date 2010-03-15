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
#include <algorithm>

#include "PCMConverter.h"
#include "AudioEngineDefines.h"


#define CHUNK_SIZE_FRAMES 512


PCMConverter::PCMConverter(const AudioStreamBasicDescription& sourceFormat, const AudioStreamBasicDescription& destinationFormat)
	: AudioConverter(sourceFormat, destinationFormat)
{
	if(kAudioFormatLinearPCM != mSourceFormat.mFormatID || kAudioFormatLinearPCM != mDestinationFormat.mFormatID)
		throw std::runtime_error("Only PCM to PCM conversions are supported by PCMConverter");
	
	if(mSourceFormat.mSampleRate != mDestinationFormat.mSampleRate)
		throw std::runtime_error("Sample rate conversion is not supported by PCMConverter");

	if(mSourceFormat.mChannelsPerFrame != mDestinationFormat.mChannelsPerFrame)
		throw std::runtime_error("Channel mapping is not supported by PCMConverter");
	
	if(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags || kAudioFormatFlagIsFloat & mDestinationFormat.mFormatFlags)
		throw std::runtime_error("Only integer to integer conversions are supported by PCMConverter");

	if(1 < mSourceFormat.mChannelsPerFrame && !(kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags))
		throw std::runtime_error("Only deinterleaved source formats are supported by PCMConverter");
	
	// Allocate the transfer buffer
	// The transfer buffer always contains native endian, signed integer, deinterleaved audio stored as SInt64
	mTransferBuffer = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * mSourceFormat.mChannelsPerFrame)));
	
	if(!mTransferBuffer)
		throw std::bad_alloc();
	
	mTransferBuffer->mNumberBuffers = mSourceFormat.mChannelsPerFrame;
	
	for(UInt32 i = 0; i < mTransferBuffer->mNumberBuffers; ++i) {
		mTransferBuffer->mBuffers[i].mData = static_cast<void *>(calloc(CHUNK_SIZE_FRAMES, sizeof(SInt64)));
		
		if(!mTransferBuffer->mBuffers[i].mData) {
			for(UInt32 j = 0; j < i; ++j)
				free(mTransferBuffer->mBuffers[j].mData), mTransferBuffer->mBuffers[j].mData = NULL;
			free(mTransferBuffer), mTransferBuffer = NULL;
			
			throw std::bad_alloc();
		}
		
		mTransferBuffer->mBuffers[i].mDataByteSize = CHUNK_SIZE_FRAMES * sizeof(SInt64);
		mTransferBuffer->mBuffers[i].mNumberChannels = 1;
	}
}

PCMConverter::~PCMConverter()
{
	if(mTransferBuffer) {
		for(UInt32 bufferIndex = 0; bufferIndex < mTransferBuffer->mNumberBuffers; ++bufferIndex)
			free(mTransferBuffer->mBuffers[bufferIndex].mData), mTransferBuffer->mBuffers[bufferIndex].mData = NULL;
		
		free(mTransferBuffer), mTransferBuffer = NULL;
	}
}

UInt32 PCMConverter::Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);
	
	// Nothing to do
	if(0 == frameCount) {
		for(UInt32 bufferIndex = 0; bufferIndex < outputBuffer->mNumberBuffers; ++bufferIndex)
			outputBuffer->mBuffers[bufferIndex].mDataByteSize = 0;

		return 0;
	}
	
	UInt32 framesConverted = 0;
	UInt32 framesRemaining = frameCount;
	
	while(0 < framesRemaining) {
		UInt32 framesToConvert = std::min(static_cast<UInt32>(CHUNK_SIZE_FRAMES), framesRemaining);
		
		UInt32 framesRead = ReadInteger(inputBuffer, mTransferBuffer, framesConverted, framesToConvert);
		
		if(framesRead != framesToConvert)
			ERR("fnord");
		
		UInt32 framesWritten = WriteInteger(mTransferBuffer, outputBuffer, framesConverted, framesRead);
		
		if(framesWritten != framesRead)
			ERR("fnord!");
		
		framesRemaining -= framesWritten;
		framesConverted += framesWritten;
	}
	
	// We're finished!
	return framesConverted;	
}

UInt32 PCMConverter::ReadInteger(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 startingFrame, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);

	UInt32 shift = 0;
	if(kAudioFormatFlagIsPacked & mSourceFormat.mFormatFlags)
		shift = 64 - mSourceFormat.mBitsPerChannel;	
	else {
		if(kAudioFormatFlagIsAlignedHigh & mSourceFormat.mFormatFlags)
			shift = 64 - (8 * mSourceFormat.mBytesPerFrame);
		else
			shift = 64 - (8 * mSourceFormat.mBytesPerFrame) + mSourceFormat.mBitsPerChannel;
	}
	
	// The input and transfer buffers are always deinterleaved
	for(UInt32 bufferIndex = 0; bufferIndex < inputBuffer->mNumberBuffers; ++bufferIndex) {
		SInt64 *output = static_cast<SInt64 *>(outputBuffer->mBuffers[bufferIndex].mData) + startingFrame;

		switch(mSourceFormat.mBytesPerFrame) {
			case 1:
			{
				UInt8 *input = static_cast<UInt8 *>(inputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
				
				UInt32 counter = frameCount;
				while(counter--)
					*output++ = static_cast<SInt64>(*input++) << shift;

				break;
			}
				
			case 2:
			{
				UInt16 *input = static_cast<UInt16 *>(inputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
				
				UInt32 counter = frameCount;
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
					while(counter--)
						*output++ = static_cast<SInt64>(*input++) << shift;
				}
				else {
					while(counter--)
						*output++ = static_cast<SInt64>(OSSwapInt16(*input++)) << shift;
				}

				break;
			}
				
			case 3:
			{
				UInt8 *input = static_cast<UInt8 *>(inputBuffer->mBuffers[bufferIndex].mData) + (3 * startingFrame);
				
				UInt32 counter = frameCount;
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
					while(counter--) {
						SInt64 sample = 0;
						
						sample |= *input++;
						sample <<= 8;
						
						sample |= *input++;
						sample <<= 8;
						
						sample |= *input++;
						sample <<= 8;
						
						*output++ = sample << shift;
					}
				}
				else {
					while(counter--) {
						SInt64 sample = 0;
						
						sample |= *input++;
						sample |= (*input++ << 8);
						sample |= (*input++ << 16);
						
						*output++ = sample << shift;
					}
				}

				break;
			}
				
			case 4:
			{
				UInt32 *input = static_cast<UInt32 *>(inputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
				
				UInt32 counter = frameCount;
				if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
					while(counter--)
						*output++ = static_cast<SInt64>(*input++) << shift;
				}
				else {
					while(counter--)
						*output++ = static_cast<SInt64>(OSSwapInt32(*input++)) << shift;
				}

				break;
			}
				
			default:
				outputBuffer->mBuffers[bufferIndex].mDataByteSize = 0;
				return 0;
		}

		outputBuffer->mBuffers[bufferIndex].mDataByteSize = 8 * frameCount;
	}
	
	return frameCount;
}

UInt32 PCMConverter::WriteInteger(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 startingFrame, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);
	
	UInt32 shift = 0;
	if(kAudioFormatFlagIsPacked & mDestinationFormat.mFormatFlags)
		shift = 64 - mDestinationFormat.mBitsPerChannel;	
	else {
		if(kAudioFormatFlagIsAlignedHigh & mDestinationFormat.mFormatFlags)
			shift = 64 - (8 * mDestinationFormat.mBytesPerFrame);
		else
			shift = 64 - (8 * mDestinationFormat.mBytesPerFrame) + mDestinationFormat.mBitsPerChannel;
	}

	// The destination is deinterleaved, so just convert
	if(kAudioFormatFlagIsNonInterleaved & mDestinationFormat.mFormatFlags) {
		
		for(UInt32 bufferIndex = 0; bufferIndex < inputBuffer->mNumberBuffers; ++bufferIndex) {
			SInt64 *input = static_cast<SInt64 *>(inputBuffer->mBuffers[bufferIndex].mData) + startingFrame;

			switch(mDestinationFormat.mBytesPerFrame) {
				case 1:
				{
					UInt8 *output = static_cast<UInt8 *>(outputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
					
					UInt32 counter = frameCount;
					while(counter--)
						*output++ = static_cast<UInt8>(*input++ >> shift);
					
					break;
				}
					
				case 2:
				{
					UInt16 *output = static_cast<UInt16 *>(outputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
						while(counter--)
							*output++ = static_cast<UInt16>(*input++ >> shift);
					}
					else {
						while(counter--)
							*output++ = OSSwapInt16(*input++ >> shift);
					}
					
					break;
				}
					
				case 3:
				{
					UInt8 *output = static_cast<UInt8 *>(outputBuffer->mBuffers[bufferIndex].mData) + (3 * startingFrame);
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
						while(counter--) {
							SInt64 sample = *input++ >> shift;
							
							*output++ = static_cast<UInt8>(sample & 0xff);
							*output++ = static_cast<UInt8>((sample >> 8) & 0xff);
							*output++ = static_cast<UInt8>((sample >> 16) & 0xff);
						}
					}
					else {
						while(counter--) {
							SInt64 sample = *input++ >> shift;
							
							*output++ = static_cast<UInt8>((sample >> 16) & 0xff);
							*output++ = static_cast<UInt8>((sample >> 8) & 0xff);
							*output++ = static_cast<UInt8>(sample & 0xff);
						}
					}
					
					break;
				}
					
				case 4:
				{
					UInt32 *output = static_cast<UInt32 *>(outputBuffer->mBuffers[bufferIndex].mData) + startingFrame;
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
						while(counter--)
							*output++ = static_cast<UInt32>(*input++ >> shift);
					}
					else {
						while(counter--)
							*output++ = OSSwapInt32(static_cast<UInt32>(*input++ >> shift));
					}
					
					
					break;
				}
					
				default:
					outputBuffer->mBuffers[bufferIndex].mDataByteSize = 0;
					return 0;
			}

			outputBuffer->mBuffers[bufferIndex].mDataByteSize = mDestinationFormat.mBytesPerFrame * frameCount;
		}
		
		return frameCount;
	}
	// The destination is interleaved, but the transfer buffer is always deinterleaved
	else {
		for(UInt32 bufferIndex = 0; bufferIndex < inputBuffer->mNumberBuffers; ++bufferIndex) {
			SInt64 *input = static_cast<SInt64 *>(inputBuffer->mBuffers[bufferIndex].mData) + startingFrame;

			switch(mDestinationFormat.mBytesPerFrame / mDestinationFormat.mChannelsPerFrame) {
				case 1:
				{
					UInt8 *output = static_cast<UInt8 *>(outputBuffer->mBuffers[0].mData) + (mDestinationFormat.mChannelsPerFrame * startingFrame) + bufferIndex;
					
					UInt32 counter = frameCount;
					while(counter--) {
						*output = static_cast<UInt8>(*input++ >> shift);
						output += mDestinationFormat.mChannelsPerFrame;
					}
					
					break;
				}
					
				case 2:
				{
					UInt16 *output = static_cast<UInt16 *>(outputBuffer->mBuffers[0].mData) + (mDestinationFormat.mChannelsPerFrame * startingFrame) + bufferIndex;
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian & mDestinationFormat.mFormatFlags) {
						while(counter--) {
							*output = static_cast<UInt16>(*input++ >> shift);
							output += mDestinationFormat.mChannelsPerFrame;
						}
					}
					else {
						while(counter--) {
							*output = OSSwapInt16(*input++ >> shift);
							output += mDestinationFormat.mChannelsPerFrame;
						}
					}
					
					break;
				}
					
				case 3:
				{
					UInt8 *output = static_cast<UInt8 *>(outputBuffer->mBuffers[0].mData) + (3 * ((mDestinationFormat.mChannelsPerFrame * startingFrame) + bufferIndex));
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian & mDestinationFormat.mFormatFlags) {
						while(counter--) {
							SInt64 sample = *input++ >> shift;
							
							*output++ = static_cast<UInt8>(sample & 0xff);
							*output++ = static_cast<UInt8>((sample >> 8) & 0xff);
							*output++ = static_cast<UInt8>((sample >> 16) & 0xff);

							output += 3 * (mDestinationFormat.mChannelsPerFrame - 1);
						}
					}
					else {
						while(counter--) {
							SInt64 sample = *input++ >> shift;

							*output++ = static_cast<UInt8>((sample >> 16) & 0xff);
							*output++ = static_cast<UInt8>((sample >> 8) & 0xff);
							*output++ = static_cast<UInt8>(sample & 0xff);

							output += 3 * (mDestinationFormat.mChannelsPerFrame - 1);
						}
					}
					
					break;
				}
					
				case 4:
				{
					UInt32 *output = static_cast<UInt32 *>(outputBuffer->mBuffers[0].mData) + (mDestinationFormat.mChannelsPerFrame * startingFrame) + bufferIndex;
					
					UInt32 counter = frameCount;
					if(kAudioFormatFlagsNativeEndian & mDestinationFormat.mFormatFlags) {
						while(counter--) {
							*output = static_cast<UInt32>(*input++ >> shift);
							output += mDestinationFormat.mChannelsPerFrame;
						}
					}
					else {
						while(counter--) {
							*output = OSSwapInt32(static_cast<UInt32>(*input++ >> shift));
							output += mDestinationFormat.mChannelsPerFrame;
						}
					}
					
					
					break;
				}
					
				default:
					outputBuffer->mBuffers[bufferIndex].mDataByteSize = 0;
					return 0;
			}			
		}
		
		outputBuffer->mBuffers[0].mDataByteSize = mDestinationFormat.mBytesPerFrame * frameCount;
		
		return frameCount;
	}
	
	return 0;
}
