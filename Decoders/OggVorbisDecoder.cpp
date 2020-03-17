/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <AudioToolbox/AudioFormat.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "OggVorbisDecoder.h"

#define BUFFER_SIZE_FRAMES 2048

namespace {

	void RegisterOggVorbisDecoder() __attribute__ ((constructor));
	void RegisterOggVorbisDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::OggVorbisDecoder>();
	}

#pragma mark Callbacks

	size_t read_func_callback(void *ptr, size_t size, size_t nmemb, void *datasource)
	{
		assert(nullptr != datasource);

		auto decoder = static_cast<SFB::Audio::OggVorbisDecoder *>(datasource);
		return (size_t)decoder->GetInputSource().Read(ptr, (SInt64)(size * nmemb));
	}

	int seek_func_callback(void *datasource, ogg_int64_t offset, int whence)
	{
		assert(nullptr != datasource);

		auto decoder = static_cast<SFB::Audio::OggVorbisDecoder *>(datasource);
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

		return (!inputSource.SeekToOffset(offset));
	}

	long tell_func_callback(void *datasource)
	{
		assert(nullptr != datasource);

		auto decoder = static_cast<SFB::Audio::OggVorbisDecoder *>(datasource);
		return (long)decoder->GetInputSource().GetOffset();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::OggVorbisDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ogg"), CFSTR("oga") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::OggVorbisDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/ogg-vorbis") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::OggVorbisDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ogg"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::OggVorbisDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg-vorbis"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::OggVorbisDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new OggVorbisDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::OggVorbisDecoder::OggVorbisDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource))
{
	memset(&mVorbisFile, 0, sizeof(mVorbisFile));
}

SFB::Audio::OggVorbisDecoder::~OggVorbisDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::OggVorbisDecoder::_Open(CFErrorRef *error)
{
	ov_callbacks callbacks = {
		.read_func = read_func_callback,
		.seek_func = seek_func_callback,
		.tell_func = tell_func_callback,
		.close_func = nullptr
	};

	if(0 != ov_test_callbacks(this, &mVorbisFile, nullptr, 0, callbacks)) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg Vorbis file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg Vorbis file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(0 != ov_test_open(&mVorbisFile)) {
		os_log_error(OS_LOG_DEFAULT, "ov_test_open failed");

		if(0 != ov_clear(&mVorbisFile))
			os_log_error(OS_LOG_DEFAULT, "ov_clear failed");

		return false;
	}

	vorbis_info *ovInfo = ov_info(&mVorbisFile, -1);
	if(nullptr == ovInfo) {
		os_log_error(OS_LOG_DEFAULT, "ov_info failed");

		if(0 != ov_clear(&mVorbisFile))
			os_log_error(OS_LOG_DEFAULT, "ov_clear failed");

		return false;
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	mFormat.mSampleRate			= ovInfo->rate;
	mFormat.mChannelsPerFrame	= (UInt32)ovInfo->channels;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatVorbis;

	mSourceFormat.mSampleRate			= ovInfo->rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)ovInfo->channels;

	switch(ovInfo->channels) {
			// Default channel layouts from Vorbis I specification section 4.3.9
			// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9

		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 3:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_AC3_3_0);		break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_C);	break;
		case 6:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_C);	break;

		case 7:
			mChannelLayout = ChannelLayout::ChannelLayoutWithChannelLabels({
				kAudioChannelLabel_Left, kAudioChannelLabel_Center, kAudioChannelLabel_Right,
				kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_CenterSurround,
				kAudioChannelLabel_LFEScreen});
			break;

		case 8:
			mChannelLayout = ChannelLayout::ChannelLayoutWithChannelLabels({
				kAudioChannelLabel_Left, kAudioChannelLabel_Center, kAudioChannelLabel_Right,
				kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_RearSurroundLeft, kAudioChannelLabel_RearSurroundRight,
				kAudioChannelLabel_LFEScreen});
			break;
	}

	return true;
}

bool SFB::Audio::OggVorbisDecoder::_Close(CFErrorRef */*error*/)
{
	if(0 != ov_clear(&mVorbisFile))
		os_log_error(OS_LOG_DEFAULT, "ov_clear failed");

	return true;
}

SFB::CFString SFB::Audio::OggVorbisDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("Ogg Vorbis, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::OggVorbisDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	float		**buffer			= nullptr;
	UInt32		framesRemaining		= frameCount;
	UInt32		totalFramesRead		= 0;
	int			currentSection		= 0;

	// Mark the output buffers as empty
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		bufferList->mBuffers[i].mDataByteSize = 0;
		bufferList->mBuffers[i].mNumberChannels = 1;
	}

	while(0 < framesRemaining) {
		// Decode a chunk of samples from the file
		long framesRead = ov_read_float(&mVorbisFile,
										&buffer,
										std::min(BUFFER_SIZE_FRAMES, (int)framesRemaining),
										&currentSection);

		if(0 > framesRead) {
			os_log_error(OS_LOG_DEFAULT, "Ogg Vorbis decoding error");
			return 0;
		}

		// 0 frames indicates EOS
		if(0 == framesRead)
			break;

		// Copy the frames from the decoding buffer to the output buffer
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			// Skip over any frames already decoded
			memcpy((float *)bufferList->mBuffers[channel].mData + totalFramesRead, buffer[channel], (size_t)framesRead * sizeof(float));
			bufferList->mBuffers[channel].mDataByteSize += (size_t)framesRead * sizeof(float);
		}

		totalFramesRead += (UInt32)framesRead;
		framesRemaining -= (UInt32)framesRead;
	}

	return totalFramesRead;
}

SInt64 SFB::Audio::OggVorbisDecoder::_SeekToFrame(SInt64 frame)
{
	if(0 != ov_pcm_seek(&mVorbisFile, frame)) {
		os_log_error(OS_LOG_DEFAULT, "Ogg Vorbis seek error");
		return -1;
	}

	return _GetCurrentFrame();
}
