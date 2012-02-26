/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif
#include <AudioToolbox/AudioFormat.h>
#include <Accelerate/Accelerate.h>
#include <stdexcept>

#include "CoreAudioDecoder.h"
#include "CFErrorUtilities.h"
#include "CreateChannelLayout.h"
#include "CreateStringForOSType.h"
#include "Logger.h"

#pragma mark Callbacks

static OSStatus
myAudioFile_ReadProc(void		*inClientData,
					 SInt64		inPosition, 
					 UInt32		requestCount, 
					 void		*buffer, 
					 UInt32		*actualCount)
{
	assert(nullptr != inClientData);

	CoreAudioDecoder *decoder = static_cast<CoreAudioDecoder *>(inClientData);
	InputSource *inputSource = decoder->GetInputSource();
	
	if(inPosition != inputSource->GetOffset()) {
		if(!inputSource->SupportsSeeking() || !inputSource->SeekToOffset(inPosition))
			return kAudioFileOperationNotSupportedError;
	} 
	
	*actualCount = static_cast<UInt32>(inputSource->Read(buffer, requestCount));
	
	if(0 == *actualCount)
#if !TARGET_OS_IPHONE
		return (inputSource->AtEOF() ? eofErr : ioErr);
#else
		return (inputSource->AtEOF() ? kAudioFileEndOfFileError : kAudioFilePositionError);
#endif
	
	return noErr;
}

static SInt64
myAudioFile_GetSizeProc(void *inClientData)
{
	assert(nullptr != inClientData);

	CoreAudioDecoder *decoder = static_cast<CoreAudioDecoder *>(inClientData);
	return decoder->GetInputSource()->GetLength();
}

#pragma mark Static Methods

CFArrayRef CoreAudioDecoder::CreateSupportedFileExtensions()
{
	CFArrayRef		supportedExtensions			= nullptr;
	UInt32			size						= sizeof(supportedExtensions);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 
																		 0, 
																		 nullptr, 
																		 &size, 
																		 &supportedExtensions);
	
	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: " << result << osType);
		CFRelease(osType), osType = nullptr;

		return nullptr;
	}
	
	return supportedExtensions;
}

CFArrayRef CoreAudioDecoder::CreateSupportedMIMETypes()
{
	CFArrayRef		supportedMIMETypes			= nullptr;
	UInt32			size						= sizeof(supportedMIMETypes);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 
																		 0, 
																		 nullptr, 
																		 &size, 
																		 &supportedMIMETypes);
	
	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: " << result << osType);
		CFRelease(osType), osType = nullptr;
		
		return nullptr;
	}
	
	return CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
}

bool CoreAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	CFArrayRef		supportedExtensions			= nullptr;
	UInt32			size						= sizeof(supportedExtensions);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 
																		 0, 
																		 nullptr, 
																		 &size, 
																		 &supportedExtensions);
	
	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: " << result << osType);
		CFRelease(osType), osType = nullptr;
		
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
		
	CFRelease(supportedExtensions), supportedExtensions = nullptr;
	
	return extensionIsSupported;
}

bool CoreAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	CFArrayRef		supportedMIMETypes			= nullptr;
	UInt32			size						= sizeof(supportedMIMETypes);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 
																		 0, 
																		 nullptr, 
																		 &size, 
																		 &supportedMIMETypes);
	
	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: " << result << osType);
		CFRelease(osType), osType = nullptr;
		
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
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = nullptr;
	
	return mimeTypeIsSupported;
}

#pragma mark Creation and Destruction

CoreAudioDecoder::CoreAudioDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mAudioFile(nullptr), mExtAudioFile(nullptr), mUseM4AWorkarounds(false), mCurrentFrame(0)
{}

CoreAudioDecoder::~CoreAudioDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool CoreAudioDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	// Open the input file
	OSStatus result = AudioFileOpenWithCallbacks(this, myAudioFile_ReadProc, nullptr, myAudioFile_GetSizeProc, nullptr, 0, &mAudioFile);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileOpenWithCallbacks failed: " << result);
		
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	result = ExtAudioFileWrapAudioFileID(mAudioFile, false, &mExtAudioFile);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileWrapAudioFileID failed: " << result);
		
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;
		
		return false;
	}
	
	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: " << result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;		
		mExtAudioFile = nullptr;
		
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
		
		if(kAppleLosslessFormatFlag_16BitSourceData == mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 16;
		else if(kAppleLosslessFormatFlag_20BitSourceData == mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 20;
		else if(kAppleLosslessFormatFlag_24BitSourceData == mSourceFormat.mFormatFlags)
			mFormat.mBitsPerChannel	= 24;
		else if(kAppleLosslessFormatFlag_32BitSourceData == mSourceFormat.mFormatFlags)
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
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: " << result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;		
		mExtAudioFile = nullptr;
		
		return false;
	}
	
	// Setup the channel layout
	// There is a bug in EAF where if the underlying AF doesn't return a channel layout it returns an empty struct
//	result = ExtAudioFileGetPropertyInfo(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, nullptr);
	result = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyChannelLayout, &dataSize, nullptr);
	if(noErr == result) {
		mChannelLayout = static_cast<AudioChannelLayout *>(malloc(dataSize));
//		result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, mChannelLayout);
		result = AudioFileGetProperty(mAudioFile, kAudioFilePropertyChannelLayout, &dataSize, mChannelLayout);

		if(noErr != result) {
//			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: " << result);
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetProperty (kAudioFilePropertyChannelLayout) failed: " << result);
			
			result = ExtAudioFileDispose(mExtAudioFile);
			if(noErr != result)
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
			
			result = AudioFileClose(mAudioFile);
			if(noErr != result)
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
			
			mAudioFile = nullptr;		
			mExtAudioFile = nullptr;
			
			return false;
		}
	}
	else
//		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: " << result);
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetPropertyInfo (kAudioFilePropertyChannelLayout) failed: " << result);

	// Work around bugs in ExtAudioFile: http://lists.apple.com/archives/coreaudio-api/2009/Nov/msg00119.html
	// Synopsis: ExtAudioFileTell() and ExtAudioFileSeek() are broken for m4a files
	AudioFileID audioFile;
	dataSize = sizeof(audioFile);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_AudioFile, &dataSize, &audioFile);
	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_AudioFile) failed: " << result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;		
		mExtAudioFile = nullptr;
		
		return false;
	}
	
	AudioFileTypeID fileFormat;
	dataSize = sizeof(fileFormat);
	result = AudioFileGetProperty(audioFile, kAudioFilePropertyFileFormat, &dataSize, &fileFormat);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileGetProperty (kAudioFilePropertyFileFormat) failed: " << result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;		
		mExtAudioFile = nullptr;
		
		return false;
	}
	
	if(kAudioFileM4AType == fileFormat || kAudioFileMPEG4Type == fileFormat || kAudioFileAAC_ADTSType == fileFormat)
		mUseM4AWorkarounds = true;
	
#if 0
	// This was supposed to determine if ExtAudioFile had been fixed, but even though
	// it passes on 10.6.2 things are not behaving properly
	SInt64 currentFrame = -1;	
	result = ExtAudioFileTell(mExtAudioFile, &currentFrame);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileTell failed: " << result);
		
		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;		
		mExtAudioFile = nullptr;
		
		return false;
	}
	
	if(0 > currentFrame)
		mUseM4AWorkarounds = true;
#endif

	mIsOpen = true;
	return true;
}

bool CoreAudioDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileDispose failed: " << result);
		
		mExtAudioFile = nullptr;
	}

	if(mAudioFile) {
		OSStatus result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "AudioFileClose failed: " << result);
		
		mAudioFile = nullptr;
	}

	mIsOpen = false;
	return true;
}

SInt64 CoreAudioDecoder::GetTotalFrames() const
{
	if(!IsOpen())
		return -1;

	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);
	
	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: " << result);
	
	return totalFrames;
}

SInt64 CoreAudioDecoder::GetCurrentFrame() const
{
	if(!IsOpen())
		return -1;

	if(mUseM4AWorkarounds)
		return mCurrentFrame;
	
	SInt64 currentFrame = -1;
	
	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileTell failed: " << result);
		return -1;
	}
	
	return currentFrame;
}

SInt64 CoreAudioDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileSeek failed: " << result);
		return -1;
	}
	
	if(mUseM4AWorkarounds)
		mCurrentFrame = frame;
	
	return GetCurrentFrame();
}

UInt32 CoreAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.CoreAudio", "ExtAudioFileRead failed: " << result);
		return 0;
	}

	if(mUseM4AWorkarounds)
		mCurrentFrame += frameCount;
	
	return frameCount;
}
