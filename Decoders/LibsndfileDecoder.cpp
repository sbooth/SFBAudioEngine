/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "LibsndfileDecoder.h"

namespace {

	void RegisterLibsndfileDecoder() __attribute__ ((constructor));
	void RegisterLibsndfileDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::LibsndfileDecoder>(-50);
	}

#pragma mark Callbacks

	sf_count_t my_sf_vio_get_filelen(void *user_data)
	{
		assert(nullptr != user_data);

		auto decoder = static_cast<SFB::Audio::LibsndfileDecoder *>(user_data);
		return decoder->GetInputSource().GetLength();
	}

	sf_count_t my_sf_vio_seek(sf_count_t offset, int whence, void *user_data)
	{
		assert(nullptr != user_data);

		auto decoder = static_cast<SFB::Audio::LibsndfileDecoder *>(user_data);
		SFB::InputSource& inputSource = decoder->GetInputSource();

		if(!inputSource.SupportsSeeking())
			return -1;

		// Adjust offset as required
		switch(whence) {
			case SEEK_SET:
				// offset remains unchanged
				break;
			case SEEK_CUR:
				offset += inputSource.GetOffset();
				break;
			case SEEK_END:
				offset += inputSource.GetLength();
				break;
		}

		if(!inputSource.SeekToOffset(offset))
			return -1;

		return inputSource.GetOffset();
	}

	sf_count_t my_sf_vio_read(void *ptr, sf_count_t count, void *user_data)
	{
		assert(nullptr != user_data);

		auto decoder = static_cast<SFB::Audio::LibsndfileDecoder *>(user_data);
		return decoder->GetInputSource().Read(ptr, count);
	}

	sf_count_t my_sf_vio_tell(void *user_data)
	{
		assert(nullptr != user_data);

		auto decoder = static_cast<SFB::Audio::LibsndfileDecoder *>(user_data);
		return decoder->GetInputSource().GetOffset();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::LibsndfileDecoder::CreateSupportedFileExtensions()
{
	int majorCount = 0;
	sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, majorCount, &kCFTypeArrayCallBacks);

	// Loop through each major mode
	for(int i = 0; i < majorCount; ++i) {
		SF_FORMAT_INFO formatInfo;
		formatInfo.format = i;
		if(0 == sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
			SFB::CFString extension(formatInfo.extension, kCFStringEncodingUTF8);
			if(extension)
				CFArrayAppendValue(supportedExtensions, extension);
		}
		else
			os_log_debug(OS_LOG_DEFAULT, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
	}

	return supportedExtensions;
}

CFArrayRef SFB::Audio::LibsndfileDecoder::CreateSupportedMIMETypes()
{
	return CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::LibsndfileDecoder::HandlesFilesWithExtension(CFStringRef extension)
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

bool SFB::Audio::LibsndfileDecoder::HandlesMIMEType(CFStringRef /*mimeType*/)
{
	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LibsndfileDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new LibsndfileDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::LibsndfileDecoder::LibsndfileDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mFile(nullptr, nullptr), mReadMethod(ReadMethod::Unknown)
{
	memset(&mFileInfo, 0, sizeof(SF_INFO));
}

#pragma mark Functionality

bool SFB::Audio::LibsndfileDecoder::_Open(CFErrorRef *error)
{
	// Set up the virtual IO function pointers
	SF_VIRTUAL_IO virtualIO;
	virtualIO.get_filelen	= my_sf_vio_get_filelen;
	virtualIO.seek			= my_sf_vio_seek;
	virtualIO.read			= my_sf_vio_read;
	virtualIO.write			= nullptr;
	virtualIO.tell			= my_sf_vio_tell;

	// Open the input file
	mFile = unique_SNDFILE_ptr(sf_open_virtual(&virtualIO, SFM_READ, &mFileInfo, this), sf_close);

	if(!mFile) {
		os_log_error(OS_LOG_DEFAULT, "sf_open_virtual failed: %{public}s", sf_error_number(sf_error(nullptr)));

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate interleaved PCM output
	mFormat.mFormatID			= kAudioFormatLinearPCM;

	mFormat.mSampleRate			= mFileInfo.samplerate;
	mFormat.mChannelsPerFrame	= (UInt32)mFileInfo.channels;

	int subFormat = SF_FORMAT_SUBMASK & mFileInfo.format;

	// 8-bit PCM will be high-aligned in shorts
	if(SF_FORMAT_PCM_U8 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsAlignedHigh;

		mFormat.mBitsPerChannel		= 8;

		mFormat.mBytesPerPacket		= sizeof(short) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Short;
	}
	else if(SF_FORMAT_PCM_S8 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;

		mFormat.mBitsPerChannel		= 8;

		mFormat.mBytesPerPacket		= sizeof(short) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Short;
	}
	// 16-bit PCM
	else if(SF_FORMAT_PCM_16 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

		mFormat.mBitsPerChannel		= 16;

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Short;
	}
	// 24-bit PCM will be high-aligned in ints
	else if(SF_FORMAT_PCM_24 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;

		mFormat.mBitsPerChannel		= 24;

		mFormat.mBytesPerPacket		= sizeof(int) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Int;
	}
	// 32-bit PCM
	else if(SF_FORMAT_PCM_32 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

		mFormat.mBitsPerChannel		= 32;

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Int;
	}
	// Floating point formats
	else if(SF_FORMAT_FLOAT == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(float);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Float;
	}
	else if(SF_FORMAT_DOUBLE == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(double);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Double;
	}
	// Everything else will be converted to 32-bit float
	else {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(float);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= ReadMethod::Float;
	}

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= 'SNDF';

	mSourceFormat.mSampleRate			= mFileInfo.samplerate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)mFileInfo.channels;

	switch(subFormat) {
		case SF_FORMAT_PCM_U8:
			mSourceFormat.mBitsPerChannel = 8;
			break;

		case SF_FORMAT_PCM_S8:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 8;
			break;

		case SF_FORMAT_PCM_16:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 16;
			break;

		case SF_FORMAT_PCM_24:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 24;
			break;

		case SF_FORMAT_PCM_32:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 32;
			break;

		case SF_FORMAT_FLOAT:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsFloat;
			mSourceFormat.mBitsPerChannel = 32;
			break;

		case SF_FORMAT_DOUBLE:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsFloat;
			mSourceFormat.mBitsPerChannel = 64;
			break;
	}

	return true;
}

bool SFB::Audio::LibsndfileDecoder::_Close(CFErrorRef */*error*/)
{
	mFile.reset();
	memset(&mFileInfo, 0, sizeof(SF_INFO));
	mReadMethod = ReadMethod::Unknown;

	return true;
}

SFB::CFString SFB::Audio::LibsndfileDecoder::_GetSourceFormatDescription() const
{
	SF_FORMAT_INFO formatInfo;
	formatInfo.format = mFileInfo.format;

	if(0 != sf_command(nullptr, SFC_GET_FORMAT_INFO, &formatInfo, sizeof(formatInfo))) {
		os_log_debug(OS_LOG_DEFAULT, "sf_command (SFC_GET_FORMAT_INFO) failed");
		return CFString();
	}

	return CFString(nullptr,
					CFSTR("%s, %u channels, %u Hz"),
					formatInfo.name,
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::LibsndfileDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	sf_count_t framesRead = 0;
	switch(mReadMethod) {
		case ReadMethod::Unknown:	/* Do nothing */																				break;
		case ReadMethod::Short:		framesRead = sf_readf_short(mFile.get(), (short *)bufferList->mBuffers[0].mData, frameCount);	break;
		case ReadMethod::Int:		framesRead = sf_readf_int(mFile.get(), (int *)bufferList->mBuffers[0].mData, frameCount);		break;
		case ReadMethod::Float:		framesRead = sf_readf_float(mFile.get(), (float *)bufferList->mBuffers[0].mData, frameCount);	break;
		case ReadMethod::Double:	framesRead = sf_readf_double(mFile.get(), (double *)bufferList->mBuffers[0].mData, frameCount);	break;
	}

	bufferList->mBuffers[0].mDataByteSize = (UInt32)(framesRead * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)framesRead;
}
