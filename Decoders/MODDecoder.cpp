/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "MODDecoder.h"

#define DUMB_SAMPLE_RATE	65536
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16

namespace {

	void RegisterMODDecoder() __attribute__ ((constructor));
	void RegisterMODDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MODDecoder>();
	}

#pragma mark Callbacks

	int skip_callback(void *f, long n)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);
		return (decoder->GetInputSource().SeekToOffset(decoder->GetInputSource().GetOffset() + n) ? 0 : 1);
	}

	int getc_callback(void *f)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);

		uint8_t value;
		return (1 == decoder->GetInputSource().Read(&value, 1) ? value : -1);
	}

	long getnc_callback(char *ptr, long n, void *f)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);
		return static_cast<long>(decoder->GetInputSource().Read(ptr, n));
	}

	void close_callback(void */*f*/)
	{}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MODDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("it"), CFSTR("xm"), CFSTR("s3m"), CFSTR("mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 4, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MODDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/it"), CFSTR("audio/xm"), CFSTR("audio/s3m"), CFSTR("audio/mod"), CFSTR("audio/x-mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 5, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MODDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mod"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MODDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mod"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-mod"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MODDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MODDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MODDecoder::MODDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), df(nullptr, nullptr), duh(nullptr, nullptr), dsr(nullptr, nullptr), mTotalFrames(0), mCurrentFrame(0)
{}

#pragma mark Functionality

bool SFB::Audio::MODDecoder::_Open(CFErrorRef *error)
{
	dfs.open = nullptr;
	dfs.skip = skip_callback;
	dfs.getc = getc_callback;
	dfs.getnc = getnc_callback;
	dfs.close = close_callback;

	df = unique_DUMBFILE_ptr(dumbfile_open_ex(this, &dfs), dumbfile_close);
	if(!df) {
		return false;
	}

	SFB::CFString pathExtension(CFURLCopyPathExtension(GetURL()));
	if(nullptr == pathExtension)
		return false;

	// Attempt to create the appropriate decoder based on the file's extension
	if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("it"), kCFCompareCaseInsensitive))
		duh = unique_DUH_ptr(dumb_read_it(df.get()), unload_duh);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("xm"), kCFCompareCaseInsensitive))
		duh = unique_DUH_ptr(dumb_read_xm(df.get()), unload_duh);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		duh = unique_DUH_ptr(dumb_read_s3m(df.get()), unload_duh);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("mod"), kCFCompareCaseInsensitive))
		duh = unique_DUH_ptr(dumb_read_mod(df.get()), unload_duh);

	if(!duh) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// NB: This must change if the sample rate changes because it is based on 65536 Hz
	mTotalFrames = duh_get_length(duh.get());

	dsr = unique_DUH_SIGRENDERER_ptr(duh_start_sigrenderer(duh.get(), 0, 2, 0), duh_end_sigrenderer);
	if(!dsr) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate interleaved 2 channel 44.1 16-bit output
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

	mFormat.mSampleRate			= DUMB_SAMPLE_RATE;
	mFormat.mChannelsPerFrame	= DUMB_CHANNELS;
	mFormat.mBitsPerChannel		= DUMB_BIT_DEPTH;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMOD;

	mSourceFormat.mSampleRate			= DUMB_SAMPLE_RATE;
	mSourceFormat.mChannelsPerFrame		= DUMB_CHANNELS;

	// Setup the channel layout
	mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);

	return true;
}

bool SFB::Audio::MODDecoder::_Close(CFErrorRef */*error*/)
{
	dsr.reset();
	duh.reset();
	df.reset();

	return true;
}

SFB::CFString SFB::Audio::MODDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("MOD, %u channels, %u Hz"),
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MODDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	// EOF reached
	if(duh_sigrenderer_get_position(dsr.get()) > mTotalFrames) {
		bufferList->mBuffers[0].mDataByteSize = 0;
		return 0;
	}

	long framesRendered = duh_render(dsr.get(), DUMB_BIT_DEPTH, 0, 1, 65536.0f / DUMB_SAMPLE_RATE, frameCount, bufferList->mBuffers[0].mData);

	mCurrentFrame += framesRendered;

	bufferList->mBuffers[0].mDataByteSize = (UInt32)(framesRendered * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)framesRendered;
}

SInt64 SFB::Audio::MODDecoder::_SeekToFrame(SInt64 frame)
{
	// DUMB cannot seek backwards, so the decoder must be reset
	if(frame < mCurrentFrame) {
		if(!_Close(nullptr) || !mInputSource->SeekToOffset(0) || !_Open(nullptr)) {
			os_log_error(OS_LOG_DEFAULT, "Error reseting DUMB decoder");
			return -1;
		}

		mCurrentFrame = 0;
	}

	long framesToSkip = frame - mCurrentFrame;
	duh_sigrenderer_generate_samples(dsr.get(), 1, 65536.0f / DUMB_SAMPLE_RATE, framesToSkip, nullptr);
	mCurrentFrame += framesToSkip;

	return mCurrentFrame;
}
