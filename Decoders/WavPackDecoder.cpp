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

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>

#include "AudioEngineDefines.h"
#include "WavPackDecoder.h"


#pragma mark Static Methods


bool WavPackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wv"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool WavPackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wavpack"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


WavPackDecoder::WavPackDecoder(CFURLRef url)
	: AudioDecoder(url), mWPC(NULL), mTotalFrames(0), mCurrentFrame(0)
{
	assert(NULL != url);
		
	UInt8 buf [PATH_MAX];
	if(FALSE == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		ERR("CFURLGetFileSystemRepresentation failed");
	
	char errorBuf [80];

	// Setup converter
	mWPC = WavpackOpenFileInput(reinterpret_cast<char *>(buf), errorBuf, OPEN_WVC | OPEN_NORMALIZE, 0);
	
	if(NULL == mWPC) {
		ERR("WavpackOpenFileInput failed");
		return;
	}
	
	mFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
	mFormat.mChannelsPerFrame	= WavpackGetNumChannels(mWPC);
	
	// The source's PCM format
	mSourceFormat.mFormatID				= kAudioFormatLinearPCM;
	mSourceFormat.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
	
	mSourceFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
	mSourceFormat.mChannelsPerFrame		= WavpackGetNumChannels(mWPC);
	mSourceFormat.mBitsPerChannel		= WavpackGetBitsPerSample(mWPC);
	
	mSourceFormat.mBytesPerPacket		= ((mSourceFormat.mBitsPerChannel + 7) / 8) * mSourceFormat.mChannelsPerFrame;
	mSourceFormat.mFramesPerPacket		= 1;
	mSourceFormat.mBytesPerFrame		= mSourceFormat.mBytesPerPacket * mSourceFormat.mFramesPerPacket;		
	
	mTotalFrames = WavpackGetNumSamples(mWPC);
	
	// Setup the channel layout
	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
	}
}

WavPackDecoder::~WavPackDecoder()
{
	if(mWPC)
		WavpackCloseFile(mWPC), mWPC = NULL;
}

#pragma mark Functionality

SInt64 WavPackDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());
	
	int result = WavpackSeekSample(mWPC, static_cast<uint32_t>(frame));
	if(result)
		mCurrentFrame = frame;
	
	return (result ? mCurrentFrame : -1);
}

UInt32 WavPackDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	int32_t *buffer = static_cast<int32_t *>(calloc(frameCount * mFormat.mChannelsPerFrame, sizeof(int32_t)));
	if(NULL == buffer) {
		ERR("Unable to allocate memory");
		return 0;
	}
	
	// Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
	uint32_t samplesRead = WavpackUnpackSamples(mWPC, buffer, frameCount);
	
	// Handle floating point files
	if(MODE_FLOAT & WavpackGetMode(mWPC)) {
		float *inputBuffer = reinterpret_cast<float *>(buffer);
		
		// Deinterleave the normalized samples
		for(unsigned channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
			
			for(unsigned sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame) {
				float audioSample = inputBuffer[sample];				
				*floatBuffer++ = (audioSample < -1.0f ? -1.0f : (audioSample > 1.0f ? 1.0f : audioSample));
			}
			
			bufferList->mBuffers[channel].mNumberChannels	= 1;
			bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
		}
	}
	else {
		float scaleFactor = (1 << ((WavpackGetBytesPerSample(mWPC) * 8) - 1));
		
		// Deinterleave the 32-bit samples and convert to float
		for(unsigned channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
			
			for(unsigned sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
				*floatBuffer++ = buffer[sample] / scaleFactor;
			
			bufferList->mBuffers[channel].mNumberChannels	= 1;
			bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
		}		
	}
	
	free(buffer);
	
	mCurrentFrame += samplesRead;
	
	return samplesRead;
}
