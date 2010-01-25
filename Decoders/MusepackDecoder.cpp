/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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
#include <algorithm>
#include <stdexcept>

#include "AudioEngineDefines.h"
#include "MusepackDecoder.h"


#pragma mark Static Methods


CFArrayRef MusepackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mpc") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MusepackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/musepack") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MusepackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mpc"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MusepackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/musepack"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


MusepackDecoder::MusepackDecoder(CFURLRef url)
	: AudioDecoder(url), mDemux(NULL), mTotalFrames(0), mCurrentFrame(0)
{
	assert(NULL != url);
	
	UInt8 buf [PATH_MAX];
	if(FALSE == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		throw std::runtime_error("CFURLGetFileSystemRepresentation failed");
	
	if(MPC_STATUS_OK != mpc_reader_init_stdio(&mReader, reinterpret_cast<char *>(buf)))
		throw std::runtime_error("mpc_reader_init_stdio failed");

	mDemux = mpc_demux_init(&mReader);
	if(NULL == mDemux) {
		mpc_reader_exit_stdio(&mReader);
		throw std::runtime_error("mpc_demux_init failed");
	}
	
	// Get input file information
	mpc_streaminfo streaminfo;
	mpc_demux_get_info(mDemux, &streaminfo);
	
	mTotalFrames				= mpc_streaminfo_get_length_samples(&streaminfo);
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= streaminfo.sample_freq;
	mFormat.mChannelsPerFrame	= streaminfo.channels;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'MUSE';
	
	mSourceFormat.mSampleRate			= streaminfo.sample_freq;
	mSourceFormat.mChannelsPerFrame		= streaminfo.channels;
	
	mSourceFormat.mFramesPerPacket		= (1 << streaminfo.block_pwr);
	
	// Setup the channel layout
	switch(streaminfo.channels) {
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
	}
	
	// Allocate the buffer list
	mBufferList = static_cast<AudioBufferList *>(calloc(1, sizeof(AudioBufferList) + (sizeof(AudioBuffer) * (mFormat.mChannelsPerFrame - 1))));
	
	if(NULL == mBufferList) {
		mpc_demux_exit(mDemux), mDemux = NULL;
		mpc_reader_exit_stdio(&mReader);
		
		throw std::bad_alloc();
	}

	mBufferList->mNumberBuffers = mFormat.mChannelsPerFrame;
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		mBufferList->mBuffers[i].mData = calloc(MPC_FRAME_LENGTH, sizeof(float));

		if(NULL == mBufferList->mBuffers[i].mData) {
			mpc_demux_exit(mDemux), mDemux = NULL;
			mpc_reader_exit_stdio(&mReader);
			
			for(UInt32 j = 0; j < i; ++j)
				free(mBufferList->mBuffers[j].mData), mBufferList->mBuffers[j].mData = NULL;
			
			free(mBufferList), mBufferList = NULL;
			
			throw std::bad_alloc();
		}
		
		mBufferList->mBuffers[i].mNumberChannels = 1;
	}
}

MusepackDecoder::~MusepackDecoder()
{
	if(mDemux)
		mpc_demux_exit(mDemux), mDemux = NULL;

    mpc_reader_exit_stdio(&mReader);
	
	if(mBufferList) {
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
			free(mBufferList->mBuffers[i].mData), mBufferList->mBuffers[i].mData = NULL;	
		free(mBufferList), mBufferList = NULL;
	}
}


#pragma mark Functionality


CFStringRef MusepackDecoder::CreateSourceFormatDescription()
{
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("Musepack, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MusepackDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());
	
	mpc_status result = mpc_demux_seek_sample(mDemux, frame);
	if(MPC_STATUS_OK == result)
		mCurrentFrame = frame;
	
	return ((MPC_STATUS_OK == result) ? mCurrentFrame : -1);
}

UInt32 MusepackDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	MPC_SAMPLE_FORMAT	buffer			[MPC_DECODER_BUFFER_LENGTH];
	UInt32				framesRead		= 0;
	
	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;
	
	for(;;) {
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);
		
		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			float *floatBuffer = static_cast<float *>(bufferList->mBuffers[i].mData);
			memcpy(floatBuffer + framesToSkip, mBufferList->mBuffers[i].mData, framesToCopy * sizeof(float));
			bufferList->mBuffers[i].mDataByteSize += static_cast<UInt32>(framesToCopy * sizeof(float));
			
			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				floatBuffer = static_cast<float *>(mBufferList->mBuffers[i].mData);
				memmove(floatBuffer, floatBuffer + framesToCopy, (framesInBuffer - framesToCopy) * sizeof(float));
			}
			
			mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToCopy * sizeof(float));
		}
		
		framesRead += framesToCopy;
		
		// All requested frames were read
		if(framesRead == frameCount)
			break;
		
		// Decode one frame of MPC data
		mpc_frame_info frame;
		frame.buffer = buffer;

		mpc_status result = mpc_demux_decode(mDemux, &frame);
		if(MPC_STATUS_OK != result) {
			LOG("Musepack decoding error");
			break;
		}

		// End of input
		if(-1 == frame.bits)
			break;
		
#ifdef MPC_FIXED_POINT
#error "Fixed point not yet supported"
#else
		float		*inputBuffer	= reinterpret_cast<float *>(buffer);
		float		audioSample		= 0;
		
		// Deinterleave the normalized samples
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *floatBuffer = static_cast<float *>(mBufferList->mBuffers[channel].mData);

			for(UInt32 sample = channel; sample < frame.samples * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame) {
				audioSample = inputBuffer[sample];
				*floatBuffer++ = std::max(-1.0f, std::min(audioSample, 1.0f));
			}
			
			mBufferList->mBuffers[channel].mNumberChannels	= 1;
			mBufferList->mBuffers[channel].mDataByteSize	= static_cast<UInt32>(frame.samples * sizeof(float));
		}
#endif /* MPC_FIXED_POINT */		
	}
	
	mCurrentFrame += framesRead;
	
	return framesRead;
}
