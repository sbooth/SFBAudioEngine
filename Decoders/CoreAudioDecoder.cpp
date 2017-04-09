/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif
#include <AudioToolbox/AudioFormat.h>
#include <Accelerate/Accelerate.h>

#include "CoreAudioDecoder.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "CreateStringForOSType.h"
#include "Logger.h"

namespace {

	void RegisterCoreAudioDecoder() __attribute__ ((constructor));
	void RegisterCoreAudioDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::CoreAudioDecoder>(-100);
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
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

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
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

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
	: Decoder(std::move(inputSource)), mAudioFile(nullptr), mExtAudioFile(nullptr), mUseM4AWorkarounds(false), mCurrentFrame(0)
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
		LOGGER_CRIT("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileOpenWithCallbacks failed: " << result);

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
		LOGGER_CRIT("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileWrapAudioFileID failed: " << result);

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

		mAudioFile = nullptr;

		return false;
	}

	// Query file format
	UInt32 dataSize = sizeof(mSourceFormat);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileDataFormat, &dataSize, &mSourceFormat);

	if(noErr != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: " << result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

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
		LOGGER_CRIT("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: " << result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	// Setup the channel layout
	// There is a bug in EAF where if the underlying AF doesn't return a channel layout it returns an empty struct
//	result = ExtAudioFileGetPropertyInfo(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, nullptr);
	result = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyChannelLayout, &dataSize, nullptr);
	if(noErr == result) {
		auto channelLayout = (AudioChannelLayout *)malloc(dataSize);
//		result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileChannelLayout, &dataSize, mChannelLayout);
		result = AudioFileGetProperty(mAudioFile, kAudioFilePropertyChannelLayout, &dataSize, channelLayout);

		if(noErr != result) {
//			LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: " << result);
			LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileGetProperty (kAudioFilePropertyChannelLayout) failed: " << result);

            free(channelLayout);

			result = ExtAudioFileDispose(mExtAudioFile);
			if(noErr != result)
				LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

			result = AudioFileClose(mAudioFile);
			if(noErr != result)
				LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

			mAudioFile = nullptr;
			mExtAudioFile = nullptr;

			return false;
		}

		mChannelLayout = channelLayout;

		free(channelLayout);
	}
	else
//		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: " << result);
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileGetPropertyInfo (kAudioFilePropertyChannelLayout) failed: " << result);

	// Work around bugs in ExtAudioFile: http://lists.apple.com/archives/coreaudio-api/2009/Nov/msg00119.html
	// Synopsis: ExtAudioFileTell() and ExtAudioFileSeek() are broken for m4a files
	AudioFileID audioFile;
	dataSize = sizeof(audioFile);
	result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_AudioFile, &dataSize, &audioFile);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_AudioFile) failed: " << result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	AudioFileTypeID fileFormat;
	dataSize = sizeof(fileFormat);
	result = AudioFileGetProperty(audioFile, kAudioFilePropertyFileFormat, &dataSize, &fileFormat);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileGetProperty (kAudioFilePropertyFileFormat) failed: " << result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

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
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileTell failed: " << result);

		result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

		mAudioFile = nullptr;
		mExtAudioFile = nullptr;

		return false;
	}

	if(0 > currentFrame)
		mUseM4AWorkarounds = true;
#endif

	return true;
}

bool SFB::Audio::CoreAudioDecoder::_Close(CFErrorRef */*error*/)
{
	// Close the output file
	if(mExtAudioFile) {
		OSStatus result = ExtAudioFileDispose(mExtAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileDispose failed: " << result);

		mExtAudioFile = nullptr;
	}

	if(mAudioFile) {
		OSStatus result = AudioFileClose(mAudioFile);
		if(noErr != result)
			LOGGER_NOTICE("org.sbooth.AudioEngine.Decoder.CoreAudio", "AudioFileClose failed: " << result);

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
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder", "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

	return CFString(sourceFormatDescription);
}

UInt32 SFB::Audio::CoreAudioDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	OSStatus result = ExtAudioFileRead(mExtAudioFile, &frameCount, bufferList);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileRead failed: " << result);
		return 0;
	}

	if(mUseM4AWorkarounds)
		mCurrentFrame += frameCount;

	return frameCount;
}

SInt64 SFB::Audio::CoreAudioDecoder::_GetTotalFrames() const
{
	SInt64 totalFrames = -1;
	UInt32 dataSize = sizeof(totalFrames);

	OSStatus result = ExtAudioFileGetProperty(mExtAudioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrames);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: " << result);

	return totalFrames;
}

SInt64 SFB::Audio::CoreAudioDecoder::_GetCurrentFrame() const
{
	if(mUseM4AWorkarounds)
		return mCurrentFrame;

	SInt64 currentFrame = -1;

	OSStatus result = ExtAudioFileTell(mExtAudioFile, &currentFrame);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileTell failed: " << result);
		return -1;
	}

	return currentFrame;
}

SInt64 SFB::Audio::CoreAudioDecoder::_SeekToFrame(SInt64 frame)
{
	OSStatus result = ExtAudioFileSeek(mExtAudioFile, frame);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.CoreAudio", "ExtAudioFileSeek failed: " << result);
		return -1;
	}

	if(mUseM4AWorkarounds)
		mCurrentFrame = frame;

	return _GetCurrentFrame();
}
