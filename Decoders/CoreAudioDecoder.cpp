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
#include "CoreAudioDecoder.h"
#include "CreateDisplayNameForURL.h"


#pragma mark Callbacks


static OSStatus
myAudioFile_ReadProc(void		*inClientData,
					 SInt64		inPosition, 
					 UInt32		requestCount, 
					 void		*buffer, 
					 UInt32		*actualCount)
{
	assert(NULL != inClientData);

	CoreAudioDecoder *decoder = static_cast<CoreAudioDecoder *>(inClientData);
	InputSource *inputSource = decoder->GetInputSource();
	
	if(inPosition != inputSource->GetOffset()) {
		if(!inputSource->SupportsSeeking() || !inputSource->SeekToOffset(inPosition))
			return kAudioFileOperationNotSupportedError;
	} 
	
	*actualCount = static_cast<UInt32>(inputSource->Read(buffer, requestCount));
	
	if(0 == *actualCount)
		return (inputSource->AtEOF() ? eofErr : ioErr);
	
	return noErr;
}

static SInt64
myAudioFile_GetSizeProc(void *inClientData)
{
	assert(NULL != inClientData);

	CoreAudioDecoder *decoder = static_cast<CoreAudioDecoder *>(inClientData);
	return decoder->GetInputSource()->GetLength();
}


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


CoreAudioDecoder::CoreAudioDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mAudioFile(NULL), mExtAudioFile(NULL), mUseM4AWorkarounds(false), mCurrentFrame(0)
{}

CoreAudioDecoder::~CoreAudioDecoder()
{
	if(FileIsOpen())
		CloseFile();
}


#pragma mark Functionality


bool CoreAudioDecoder::OpenFile(CFErrorRef *error)
{
	// Open the input file
	OSStatus result = AudioFileOpenWithCallbacks(this, myAudioFile_ReadProc, NULL, myAudioFile_GetSizeProc, NULL, 0, &mAudioFile);

	if(noErr != result) {
		ERR("AudioFileOpenWithCallbacks failed: %i", result);
		
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			
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
	
	result = ExtAudioFileWrapAudioFileID(mAudioFile, false, &mExtAudioFile);

	if(noErr != result) {
		ERR("ExtAudioFileOpenURL failed: %i", result);
		
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			
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

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;
		
		return false;
	}
	
	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);

	if(noErr != result) {
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	// Tell the ExtAudioFile the format in which we'd like our data
	
	// For Linear PCM formats, leave the data untouched
	if(kAudioFormatLinearPCM == mSourceFormat.mFormatID)
		mFormat = mSourceFormat;
	// For Apple Lossless, convert to high-aligned signed ints in 32 bits
	else if(kAudioFormatAppleLossless == mSourceFormat.mFormatID) {
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;
		
		mFormat.mSampleRate			= mSourceFormat.mSampleRate;
		mFormat.mChannelsPerFrame	= mSourceFormat.mChannelsPerFrame;
		
		if(kAppleLosslessFormatFlag_16BitSourceData & mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 16;
		else if(kAppleLosslessFormatFlag_20BitSourceData & mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 20;
		else if(kAppleLosslessFormatFlag_24BitSourceData & mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 24;
		else if(kAppleLosslessFormatFlag_32BitSourceData & mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 32;
		
		mFormat.mBytesPerPacket		= 4 * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
		
	}
	// For all other formats convert to the canonical Core Audio format
	else {
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
		
		mFormat.mSampleRate			= mSourceFormat.mSampleRate;
		mFormat.mChannelsPerFrame	= mSourceFormat.mChannelsPerFrame;
		mFormat.mBitsPerChannel		= 32;
		
		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
	}
	
	result = ExtAudioFileSetProperty(mExtAudioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(mFormat), &mFormat);

	if(noErr != result) {
		ERR("ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	// Setup the channel layout
	dataSize = sizeof(mChannelLayout);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, &mChannelLayout);
	
	if(noErr != result) {
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	// Work around bugs in ExtAudioFile: http://lists.apple.com/archives/coreaudio-api/2009/Nov/msg00119.html
	// Synopsis: ExtAudioFileTell() and ExtAudioFileSeek() are broken for m4a files
	AudioFileID audioFile;
	dataSize = sizeof(audioFile);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_AudioFile, &dataSize, &audioFile);
	
	if(noErr != result) {
		ERR("ExtAudioFileGetProperty (kExtAudioFileProperty_AudioFile) failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	AudioFileTypeID fileFormat;
	dataSize = sizeof(fileFormat);
	result = AudioFileGetProperty(audioFile, kAudioFilePropertyFileFormat, &dataSize, &fileFormat);

	if(noErr != result) {
		ERR("AudioFileGetProperty (kAudioFilePropertyFileFormat) failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	if(kAudioFileM4AType == fileFormat)
		mUseM4AWorkarounds = true;
	
#if 0
	// This was supposed to determine if ExtAudioFile had been fixed, but even though
	// it passes on 10.6.2 things are not behaving properly
	SInt64 currentFrame = -1;	
	result = ExtAudioFileTell(mExtAudioFile, &currentFrame);

	if(noErr != result) {
		ERR("ExtAudioFileTell failed: %i", result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;		
		mExtAudioFile = NULL;
		
		return false;
	}
	
	if(0 > currentFrame)
		mUseM4AWorkarounds = true;
#endif

	return true;
}

bool CoreAudioDecoder::CloseFile(CFErrorRef */*error*/)
{
	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			ERR("ExtAudioFileDispose failed: %i", result);
		
		mExtAudioFile = NULL;
	}

	if(mAudioFile) {
		OSStatus result = AudioFileClose(mAudioFile);
		if(noErr != result)
			ERR("AudioFileClose failed: %i", result);
		
		mAudioFile = NULL;
	}
	
	return true;
}

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
	if(mUseM4AWorkarounds)
		return mCurrentFrame;
	
	SInt64 currentFrame = -1;
	
	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
		ERR("ExtAudioFileTell failed: %i", result);
		return -1;
	}
	
	return currentFrame;
}

SInt64 CoreAudioDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < GetTotalFrames());
	
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		ERR("ExtAudioFileSeek failed: %i", result);
		return -1;
	}
	
	if(mUseM4AWorkarounds)
		mCurrentFrame = frame;
	
	return GetCurrentFrame();
}

UInt32 CoreAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(0 < frameCount);
	
	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		ERR("ExtAudioFileRead failed: %i", result);
		return 0;
	}
	
	if(mUseM4AWorkarounds)
		mCurrentFrame += frameCount;
	
	return frameCount;
}
