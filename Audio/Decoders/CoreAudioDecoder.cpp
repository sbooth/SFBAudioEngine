/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Stephen F. Booth nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY STEPHEN F. BOOTH ''AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL STEPHEN F. BOOTH BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>

#include "AudioEngineDefines.h"
#include "CoreAudioDecoder.h"


#pragma mark Static Methods


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
		DEBUG_LOG("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		
		return false;
	}
	
	bool extensionIsSupported = false;
	
	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		if(CFEqual(extension, CFArrayGetValueAtIndex(supportedExtensions, currentIndex))) {
			extensionIsSupported = true;
			break;
		}
	}
		
	CFRelease(supportedExtensions);
	
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
		DEBUG_LOG("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
		
		return false;
	}
	
	bool mimeTypeIsSupported = false;
	
	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		if(CFEqual(mimeType, CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex))) {
			mimeTypeIsSupported = true;
			break;
		}
	}
	
	CFRelease(supportedMIMETypes);
	
	return mimeTypeIsSupported;
}


#pragma mark Creation and Destruction


CoreAudioDecoder::CoreAudioDecoder(CFURLRef url, CFErrorRef *error)
	: AudioDecoder(url, error), mExtAudioFile(NULL)
{
	// Open the input file		
	OSStatus result = ExtAudioFileOpenURL(mURL, &mExtAudioFile);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileOpenURL failed: %i", result);

		if(NULL != error) {
			CFStringRef displayName = NULL;
			result = LSCopyDisplayNameForURL(mURL, &displayName);

			if(noErr != result)
				displayName = CFURLCopyLastPathComponent(mURL);

			CFStringRef errorDescription = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("The format of the file \"%@\" was not recognized."), displayName);

			CFTypeRef userInfoKeys [] = {
				kCFErrorLocalizedDescriptionKey,
				kCFErrorLocalizedFailureReasonKey,
				kCFErrorLocalizedRecoverySuggestionKey
			};

			CFTypeRef userInfoValues [] = {
				errorDescription,
				CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""),
				CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "")
			};
						
			*error = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault,
															AudioDecoderErrorDomain,
															AudioDecoderInputOutputError, 
															(const void * const *)&userInfoKeys, 
															(const void * const *)&userInfoValues,
															3);
			
			CFRelease(displayName);
			CFRelease(errorDescription);
		}
		
		return;
	}
	
	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %i", result);
		
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault,
								   kCFErrorDomainOSStatus,
								   result,
								   NULL);
	}
			
	// Tell the ExtAudioFile the format in which we'd like our data
	mFormat.mSampleRate			= mSourceFormat.mSampleRate;
	mFormat.mChannelsPerFrame	= mSourceFormat.mChannelsPerFrame;
	
	result = ExtAudioFileSetProperty(mExtAudioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(mFormat), &mFormat);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %i", result);
		
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault,
								   kCFErrorDomainOSStatus,
								   result,
								   NULL);
	}
	
	// Setup the channel layout
	dataSize = sizeof(mChannelLayout);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, &mChannelLayout);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %i", result);

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault,
								   kCFErrorDomainOSStatus,
								   result,
								   NULL);
	}
}

CoreAudioDecoder::~CoreAudioDecoder()
{
	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			DEBUG_LOG("ExtAudioFileDispose failed: %i", result);

		mExtAudioFile = NULL;
	}
}


#pragma mark Functionality


SInt64 CoreAudioDecoder::TotalFrames()
{
	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);
	
	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result)
		DEBUG_LOG("ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %i", result);
	
	return totalFrames;
}

SInt64 CoreAudioDecoder::CurrentFrame()
{
	SInt64 currentFrame = -1;
	
	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result)
		DEBUG_LOG("ExtAudioFileTell failed: %i", result);
	
	return currentFrame;
}

SInt64 CoreAudioDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->TotalFrames());
	
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileSeek failed: %i", result);
		
		return -1;
	}
	
	return this->CurrentFrame();
}

UInt32 CoreAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		DEBUG_LOG("ExtAudioFileRead failed: %i", result);
		
		return 0;
	}
	
	return frameCount;
}
