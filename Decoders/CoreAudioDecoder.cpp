/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioFormat.h>
#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "CoreAudioDecoder.h"
#include "SFBCStringForOSType.h"

namespace {

	void RegisterCoreAudioDecoder() __attribute__ ((constructor));
	void RegisterCoreAudioDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::CoreAudioDecoder>(-75);
	}

#pragma mark Callbacks

	OSStatus myAudioFile_ReadProc(void		*inClientData,
								  SInt64	inPosition,
								  UInt32	requestCount,
								  void		*buffer,
								  UInt32	*actualCount)
	{
		assert(nullptr != inClientData);

		auto decoder = static_cast<SFB::Audio::CoreAudioDecoder *>(inClientData);
		SFB::InputSource& inputSource = decoder->GetInputSource();

		if(inPosition != inputSource.GetOffset()) {
			if(!inputSource.SupportsSeeking() || !inputSource.SeekToOffset(inPosition))
				return kAudioFileOperationNotSupportedError;
		}

		*actualCount = (UInt32)inputSource.Read(buffer, requestCount);

		if(0 == *actualCount)
#if !TARGET_OS_IPHONE
			return (inputSource.AtEOF() ? eofErr : ioErr);
#else
		return (inputSource.AtEOF() ? kAudioFileEndOfFileError : kAudioFilePositionError);
#endif

		return noErr;
	}

	SInt64 myAudioFile_GetSizeProc(void *inClientData)
	{
		assert(nullptr != inClientData);

		auto decoder = static_cast<SFB::Audio::CoreAudioDecoder *>(inClientData);
		return decoder->GetInputSource().GetLength();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::CoreAudioDecoder::CreateSupportedFileExtensions()
{
	CFArrayRef		supportedExtensions			= nullptr;
	UInt32			size						= sizeof(supportedExtensions);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions,
																		 0,
																		 nullptr,
																		 &size,
																		 &supportedExtensions);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

		return nullptr;
	}

	return supportedExtensions;
}

CFArrayRef SFB::Audio::CoreAudioDecoder::CreateSupportedMIMETypes()
{
	CFArrayRef		supportedMIMETypes			= nullptr;
	UInt32			size						= sizeof(supportedMIMETypes);
	OSStatus		result						= AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes,
																		 0,
																		 nullptr,
																		 &size,
																		 &supportedMIMETypes);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

		return nullptr;
	}

	return CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
}

bool SFB::Audio::CoreAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	SFB::CFArray supportedExtensions(CreateSupportedFileExtensions());
	if(!supportedExtensions)
		return false;

	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = (CFStringRef)CFArrayGetValueAtIndex(supportedExtensions, currentIndex);
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive))
			return true;
	}

	return false;
}

bool SFB::Audio::CoreAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	SFB::CFArray supportedMIMETypes(CreateSupportedMIMETypes());
	if(!supportedMIMETypes)
		return false;

	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		CFStringRef currentMIMEType = (CFStringRef)CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex);
		if(kCFCompareEqualTo == CFStringCompare(mimeType, currentMIMEType, kCFCompareCaseInsensitive))
			return true;
	}

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::CoreAudioDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new CoreAudioDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::CoreAudioDecoder::CoreAudioDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mAudioFile(nullptr), mExtAudioFile(nullptr)
{}

SFB::Audio::CoreAudioDecoder::~CoreAudioDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::CoreAudioDecoder::_Open(CFErrorRef *error)
{
	// Open the input file
	OSStatus result = AudioFileOpenWithCallbacks(this, myAudioFile_ReadProc, nullptr, myAudioFile_GetSizeProc, nullptr, 0, &mAudioFile);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFileOpenWithCallbacks failed: %d", result);

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	result = ExtAudioFileWrapAudioFileID(mAudioFile, false, &mExtAudioFile);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileWrapAudioFileID failed: %d", result);

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

		mAudioFile = nullptr;

		return false;
	}

	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %d", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

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
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %d", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	// Setup the channel layout
	result = ExtAudioFileGetPropertyInfo(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, nullptr);
	if(noErr == result) {
		auto channelLayout = (AudioChannelLayout *)malloc(dataSize);
		result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, channelLayout);

		if(noErr != result) {
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %d", result);

            free(channelLayout);

			result = ExtAudioFileDispose(mExtAudioFile);
			if(noErr != result)
				os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

			result = AudioFileClose(mAudioFile);
			if(noErr != result)
				os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

			mAudioFile = nullptr;
			mExtAudioFile = nullptr;

			return false;
		}

		mChannelLayout = channelLayout;

		free(channelLayout);
	}
	else
//		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: %d", result);
		os_log_error(OS_LOG_DEFAULT, "AudioFileGetPropertyInfo (kAudioFilePropertyChannelLayout) failed: %d", result);

	// Work around bugs in ExtAudioFile: http://lists.apple.com/archives/coreaudio-api/2009/Nov/msg00119.html
	// Synopsis: ExtAudioFileTell() and ExtAudioFileSeek() are broken for m4a files
	AudioFileID audioFile;
	dataSize = sizeof(audioFile);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_AudioFile, &dataSize, &audioFile);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileGetProperty (kExtAudioFileProperty_AudioFile) failed: %d", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	AudioFileTypeID fileFormat;
	dataSize = sizeof(fileFormat);
	result = AudioFileGetProperty(audioFile, kAudioFilePropertyFileFormat, &dataSize, &fileFormat);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFileGetProperty (kAudioFilePropertyFileFormat) failed: %d", result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioDecoder::_Close(CFErrorRef */*error*/)
{
	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "ExtAudioFileDispose failed: %d", result);

		mExtAudioFile = nullptr;
	}

	if(mAudioFile) {
		OSStatus result = AudioFileClose(mAudioFile);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioFileClose failed: %d", result);

		mAudioFile = nullptr;
	}

	return true;
}

SFB::CFString SFB::Audio::CoreAudioDecoder::_GetSourceFormatDescription() const
{
	CFStringRef		sourceFormatDescription		= nullptr;
	UInt32			sourceFormatNameSize		= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName,
																		 sizeof(mSourceFormat),
																		 &mSourceFormat,
																		 &sourceFormatNameSize,
																		 &sourceFormatDescription);

	if(noErr != result)
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

	return CFString(sourceFormatDescription);
}

UInt32 SFB::Audio::CoreAudioDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileRead failed: %d", result);
		return 0;
	}

	return frameCount;
}

SInt64 SFB::Audio::CoreAudioDecoder::_GetTotalFrames() const
{
	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);

	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result)
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %d", result);

	return totalFrames;
}

SInt64 SFB::Audio::CoreAudioDecoder::_GetCurrentFrame() const
{
	SInt64 currentFrame = -1;

	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileTell failed: %d", result);
		return -1;
	}

	return currentFrame;
}

SInt64 SFB::Audio::CoreAudioDecoder::_SeekToFrame(SInt64 frame)
{
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "ExtAudioFileSeek failed: %d", result);
		return -1;
	}

	return _GetCurrentFrame();
}
