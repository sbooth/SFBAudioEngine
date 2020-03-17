/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "OggOpusDecoder.h"

#define OPUS_SAMPLE_RATE 48000

namespace {

	void RegisterOggOpusDecoder() __attribute__ ((constructor));
	void RegisterOggOpusDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::OggOpusDecoder>();
	}

#pragma mark Callbacks

	int read_callback(void *stream, unsigned char *ptr, int nbytes)
	{
		assert(nullptr != stream);

		auto decoder = static_cast<SFB::Audio::OggOpusDecoder *>(stream);
		return (int)decoder->GetInputSource().Read(ptr, nbytes);
	}


	int seek_callback(void *stream, opus_int64 offset, int whence)
	{
		assert(nullptr != stream);

		auto decoder = static_cast<SFB::Audio::OggOpusDecoder *>(stream);
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

	opus_int64 tell_callback(void *stream)
	{
		assert(nullptr != stream);

		auto decoder = static_cast<SFB::Audio::OggOpusDecoder *>(stream);
		return decoder->GetInputSource().GetOffset();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::OggOpusDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("opus") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::OggOpusDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/opus"), CFSTR("audio/ogg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::OggOpusDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("opus"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::OggOpusDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/opus"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::OggOpusDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new OggOpusDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::OggOpusDecoder::OggOpusDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mOpusFile(nullptr, nullptr)
{}

#pragma mark Functionality

bool SFB::Audio::OggOpusDecoder::_Open(CFErrorRef *error)
{
	OpusFileCallbacks callbacks = {
		.read = read_callback,
		.seek = seek_callback,
		.tell = tell_callback,
		.close = nullptr
	};

	mOpusFile = unique_op_ptr(op_test_callbacks(this, &callbacks, nullptr, 0, nullptr), op_free);

	if(!mOpusFile) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg Opus file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg Opus file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(0 != op_test_open(mOpusFile.get())) {
		os_log_error(OS_LOG_DEFAULT, "op_test_open failed");
		return false;
	}

	const OpusHead *header = op_head(mOpusFile.get(), 0);

	// Output interleaved floating point data
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	mFormat.mSampleRate			= OPUS_SAMPLE_RATE;
	mFormat.mChannelsPerFrame	= (UInt32)header->channel_count;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatOpus;

	mSourceFormat.mSampleRate			= header->input_sample_rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)header->channel_count;

	switch(header->channel_count) {
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

bool SFB::Audio::OggOpusDecoder::_Close(CFErrorRef */*error*/)
{
	mOpusFile.reset();
	return true;
}

SFB::CFString SFB::Audio::OggOpusDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("Ogg Opus, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::OggOpusDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	float		*buffer				= (float *)bufferList->mBuffers[0].mData;
	UInt32		framesRemaining		= frameCount;
	UInt32		totalFramesRead		= 0;

	while(0 < framesRemaining) {
		int framesRead = op_read_float(mOpusFile.get(), buffer, (int)(framesRemaining * mFormat.mChannelsPerFrame), nullptr);

		if(0 > framesRead) {
			os_log_error(OS_LOG_DEFAULT, "Ogg Opus decoding error: %d", framesRead);
			return 0;
		}

		// 0 frames indicates EOS
		if(0 == framesRead)
			break;

		buffer += (UInt32)framesRead * mFormat.mChannelsPerFrame;

		totalFramesRead += (UInt32)framesRead;
		framesRemaining -= (UInt32)framesRead;
	}

	bufferList->mBuffers[0].mDataByteSize = totalFramesRead * mFormat.mBytesPerFrame;
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return totalFramesRead;
}

SInt64 SFB::Audio::OggOpusDecoder::_SeekToFrame(SInt64 frame)
{
	if(0 != op_pcm_seek(mOpusFile.get(), frame)) {
		os_log_error(OS_LOG_DEFAULT, "op_pcm_seek() failed");
		return -1;
	}

	return this->GetCurrentFrame();
}
