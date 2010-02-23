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
#include <stdexcept>

#include "AudioEngineDefines.h"
#include "WavPackDecoder.h"
#include "CreateDisplayNameForURL.h"


#define BUFFER_SIZE_FRAMES 2048


#pragma mark Static Methods


CFArrayRef WavPackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wv") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef WavPackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wavpack") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

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
{}

WavPackDecoder::~WavPackDecoder()
{
	if(FileIsOpen())
		CloseFile();
}


#pragma mark Functionality


bool WavPackDecoder::OpenFile(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(FALSE == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	
	char errorBuf [80];
	
	// Setup converter
	mWPC = WavpackOpenFileInput(reinterpret_cast<char *>(buf), errorBuf, OPEN_WVC | OPEN_NORMALIZE, 0);
	if(NULL == mWPC) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a WavPack file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioDecoderErrorDomain, 
								   AudioDecoderInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		return false;
	}
	
	// Floating-point and lossy files will be handed off in the canonical Core Audio format
	int mode = WavpackGetMode(mWPC);
	if(MODE_FLOAT & mode || !(MODE_LOSSLESS & mode)) {
		// Canonical Core Audio format
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
		
		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
		mFormat.mChannelsPerFrame	= WavpackGetNumChannels(mWPC);		
		mFormat.mBitsPerChannel		= 8 * sizeof(float);
		
		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
	}
	else {
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh | kAudioFormatFlagIsNonInterleaved;
		
		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
		mFormat.mChannelsPerFrame	= WavpackGetNumChannels(mWPC);
		mFormat.mBitsPerChannel		= WavpackGetBitsPerSample(mWPC);
		
		mFormat.mBytesPerPacket		= sizeof(int32_t);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
	}
	
	mTotalFrames						= WavpackGetNumSamples(mWPC);
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'WVPK';
	
	mSourceFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
	mSourceFormat.mChannelsPerFrame		= WavpackGetNumChannels(mWPC);
	mSourceFormat.mBitsPerChannel		= WavpackGetBitsPerSample(mWPC);
	
	// Setup the channel layout
	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
	}
	
	mBuffer = static_cast<int32_t *>(calloc(BUFFER_SIZE_FRAMES * mFormat.mChannelsPerFrame, sizeof(int32_t)));

	if(NULL == mBuffer) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		
		return false;		
	}

	return true;
}

bool WavPackDecoder::CloseFile(CFErrorRef */*error*/)
{
	if(mWPC)
		WavpackCloseFile(mWPC), mWPC = NULL;
	
	if(mBuffer)
		free(mBuffer), mBuffer = NULL;
	
	return true;
}

CFStringRef WavPackDecoder::CreateSourceFormatDescription()
{
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("WavPack, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

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
	
	UInt32 framesRemaining = frameCount;
	UInt32 totalFramesRead = 0;
	
	while(0 < framesRemaining) {
		UInt32 framesToRead = std::min(framesRemaining, static_cast<UInt32>(BUFFER_SIZE_FRAMES));
		
		// Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
		uint32_t samplesRead = WavpackUnpackSamples(mWPC, mBuffer, framesToRead);
		
		// The samples returned are handled differently based on the file's mode
		int mode = WavpackGetMode(mWPC);
		
		// Floating point files require no special handling other than deinterleaving
		if(MODE_FLOAT & mode) {
			float *inputBuffer = reinterpret_cast<float *>(mBuffer);
			
			// Deinterleave the samples
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = inputBuffer[sample];
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
			}
		}
		// Lossless files will be handed off as integers
		else if(MODE_LOSSLESS & mode) {
			// WavPack hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
			UInt32 shift = static_cast<UInt32>(8 * (sizeof(int32_t) - WavpackGetBytesPerSample(mWPC)));
			
			// Deinterleave the 32-bit samples and shift to high-alignment
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				int32_t *shiftedBuffer = static_cast<int32_t *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*shiftedBuffer++ = mBuffer[sample] << shift;
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(int32_t));
			}		
		}
		// Convert lossy files to float
		else {
			float scaleFactor = (1 << ((WavpackGetBytesPerSample(mWPC) * 8) - 1));
			
			// Deinterleave the 32-bit samples and convert to float
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = mBuffer[sample] / scaleFactor;
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
			}
		}
		
		totalFramesRead += samplesRead;
		framesRemaining -= samplesRead;
	}
	
	mCurrentFrame += totalFramesRead;
	
	return totalFramesRead;
}
