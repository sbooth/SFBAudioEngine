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
#include <stdexcept>

#include "AudioEngineDefines.h"
#include "CoreAudioDecoder.h"


#pragma mark Static Methods


CFArrayRef CoreAudioDecoder::CreateSupportedFileExtensions()
{
	CFArrayRef		supportedExtensions			= NULL;
	UInt32			size						= sizeof(supportedExtensions);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 
																		 0, 
																		 NULL, 
																		 &size, 
																		 &supportedExtensions);
	
	if(noErr != result) {
		ERR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		return NULL;
	}
	
	return CFArrayCreateCopy(kCFAllocatorDefault, supportedExtensions);
}

CFArrayRef CoreAudioDecoder::CreateSupportedMIMETypes()
{
	CFArrayRef		supportedMIMETypes			= NULL;
	UInt32			size						= sizeof(supportedMIMETypes);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 
																		 0, 
																		 NULL, 
																		 &size, 
																		 &supportedMIMETypes);
	
	if(noErr != result) {
		ERR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		return NULL;
	}
	
	return CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
}

bool CoreAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);

	CFArrayRef		supportedExtensions			= NULL;
	UInt32			size						= sizeof(supportedExtensions);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 
																		 0, 
																		 NULL, 
																		 &size, 
																		 &supportedExtensions);
	
	if(noErr != result) {
		ERR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		return false;
	}
	
	bool extensionIsSupported = false;
	
	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedExtensions, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive)) {
			extensionIsSupported = true;
			break;
		}
	}
		
	CFRelease(supportedExtensions), supportedExtensions = NULL;
	
	return extensionIsSupported;
}

bool CoreAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);

	CFArrayRef		supportedMIMETypes			= NULL;
	UInt32			size						= sizeof(supportedMIMETypes);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 
																		 0, 
																		 NULL, 
																		 &size, 
																		 &supportedMIMETypes);
	
	if(noErr != result) {
		ERR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		return false;
	}
	
	bool mimeTypeIsSupported = false;
	
	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		CFStringRef currentMIMEType = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(mimeType, currentMIMEType, kCFCompareCaseInsensitive)) {
			mimeTypeIsSupported = true;
			break;
		}
	}
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return mimeTypeIsSupported;
}


#pragma mark Creation and Destruction


CoreAudioDecoder::CoreAudioDecoder(CFURLRef url)
	: AudioDecoder(url), mExtAudioFile(NULL), mPrimingFrameBugWorkaroundAdjustment(0)
{
	// Open the input file		
	OSStatus result = ExtAudioFileOpenURL(mURL, &mExtAudioFile);
	if(noErr != result)
		throw std::runtime_error("ExtAudioFileOpenURL failed");
	
	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);
	if(noErr != result) {
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %i", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		mExtAudioFile = NULL;
		
		throw std::runtime_error("ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed");
	}
			
	// Tell the ExtAudioFile the format in which we'd like our data
	mFormat.mSampleRate			= mSourceFormat.mSampleRate;
	mFormat.mChannelsPerFrame	= mSourceFormat.mChannelsPerFrame;
	
	result = ExtAudioFileSetProperty(mExtAudioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(mFormat), &mFormat);
	if(noErr != result) {
		ERR("ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %i", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		mExtAudioFile = NULL;
		
		throw std::runtime_error("ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed");
	}
	
	// Work around a bug in ExtAudioFile: http://lists.apple.com/archives/coreaudio-api/2009/Nov/msg00119.html
	// Synopsis: ExtAudioFileTell() returns values too small by the number of priming frames
	SInt64 currentFrame = GetCurrentFrame();
	if(0 > currentFrame)
		mPrimingFrameBugWorkaroundAdjustment = -1 * currentFrame;
	
	// Setup the channel layout
	dataSize = sizeof(mChannelLayout);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, &mChannelLayout);
	if(noErr != result) {
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %i", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		mExtAudioFile = NULL;
		
		throw std::runtime_error("ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed");
	}
}

CoreAudioDecoder::~CoreAudioDecoder()
{
	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);

		mExtAudioFile = NULL;
	}
}


#pragma mark Functionality


SInt64 CoreAudioDecoder::GetTotalFrames()
{
	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);
	
	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result)
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %i", result);
	
	return totalFrames;
}

SInt64 CoreAudioDecoder::GetCurrentFrame()
{
	SInt64 currentFrame = -1;
	
	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
		ERR("ExtAudioFileTell failed: %i", result);
		return -1;
	}
	
	return currentFrame + mPrimingFrameBugWorkaroundAdjustment;
}

SInt64 CoreAudioDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());
	
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		ERR("ExtAudioFileSeek failed: %i", result);
		return -1;
	}
	
	return this->GetCurrentFrame();
}

UInt32 CoreAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		ERR("ExtAudioFileRead failed: %i", result);
		return 0;
	}
	
	return frameCount;
}
