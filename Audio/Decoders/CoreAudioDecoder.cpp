/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>

#include "CoreAudioDecoder.h"


// ========================================
// Utility Functions
// ========================================
#if DEBUG
static inline void
CFLog(CFStringRef format, ...)
{
	va_list args;
	va_start(args, format);
	
	CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault,
															   NULL,
															   format,
															   args);
	
	CFShow(message);
	CFRelease(message);
}
#endif


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
#if DEBUG
		CFLog(CFSTR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("ExtAudioFileOpenURL (kExtAudioFileProperty_FileDataFormat) failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %i"), result);
#endif
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
		OSStatus result =ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result) {
#if DEBUG
			CFLog(CFSTR("ExtAudioFileDispose failed: %i"), result);
#endif
		}
		mExtAudioFile = NULL;
	}
}


#pragma mark Functionality


SInt64 CoreAudioDecoder::TotalFrames()
{
	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);
	
	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %i"), result);
#endif
	}
	
	return totalFrames;
}

SInt64 CoreAudioDecoder::CurrentFrame()
{
	SInt64 currentFrame = -1;
	
	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("ExtAudioFileTell failed: %i"), result);
#endif
	}
	
	return currentFrame;
}

SInt64 CoreAudioDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->TotalFrames());
	
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("ExtAudioFileSeek failed: %i"), result);
#endif
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
#if DEBUG
		CFLog(CFSTR("ExtAudioFileRead failed: %i"), result);
#endif
		return 0;
	}
	
	return frameCount;
}
