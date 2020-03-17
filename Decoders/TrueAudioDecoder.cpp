/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "TrueAudioDecoder.h"

struct SFB::Audio::TrueAudioDecoder::TTA_io_callback_wrapper
{
	TTA_io_callback iocb;
	SFB::Audio::Decoder *decoder;
};

namespace {

	void RegisterTrueAudioDecoder() __attribute__ ((constructor));
	void RegisterTrueAudioDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::TrueAudioDecoder>();
	}


#pragma mark Callbacks

	TTAint32 read_callback(struct _tag_TTA_io_callback *io, TTAuint8 *buffer, TTAuint32 size)
	{
		SFB::Audio::TrueAudioDecoder::TTA_io_callback_wrapper *iocb = (SFB::Audio::TrueAudioDecoder::TTA_io_callback_wrapper *)io;
		return (TTAint32)iocb->decoder->GetInputSource().Read(buffer, size);
	}

	TTAint64 seek_callback(struct _tag_TTA_io_callback *io, TTAint64 offset)
	{
		SFB::Audio::TrueAudioDecoder::TTA_io_callback_wrapper *iocb = (SFB::Audio::TrueAudioDecoder::TTA_io_callback_wrapper *)io;
		return iocb->decoder->GetInputSource().SeekToOffset(offset);
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::TrueAudioDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("tta") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::TrueAudioDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/x-tta") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::TrueAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("tta"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::TrueAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-tta"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::TrueAudioDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new TrueAudioDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::TrueAudioDecoder::TrueAudioDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mDecoder(nullptr), mCallbacks(nullptr), mCurrentFrame(0), mTotalFrames(0), mFramesToSkip(0)
{}

#pragma mark Functionality

bool SFB::Audio::TrueAudioDecoder::_Open(CFErrorRef *error)
{
	mCallbacks				= unique_callback_wrapper_ptr(new TTA_io_callback_wrapper);
	mCallbacks->iocb.read	= read_callback;
	mCallbacks->iocb.write	= nullptr;
	mCallbacks->iocb.seek	= seek_callback;
	mCallbacks->decoder		= this;

	TTA_info streamInfo;

	try {
		mDecoder = unique_tta_ptr(new tta::tta_decoder((TTA_io_callback *)mCallbacks.get()));
		mDecoder->init_get_info(&streamInfo, 0);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "Error creating True Audio decoder: %d", e.code());
	}

	if(!mDecoder) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a True Audio file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	mFormat.mSampleRate			= streamInfo.sps;
	mFormat.mChannelsPerFrame	= streamInfo.nch;
	mFormat.mBitsPerChannel		= streamInfo.bps;

	mFormat.mBytesPerPacket		= ((streamInfo.bps + 7) / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Support 4 to 32 bits per sample (True Audio may support more or less, but the documentation didn't say)
	switch(mFormat.mBitsPerChannel) {
		case 8:
		case 16:
		case 24:
		case 32:
			mFormat.mFormatFlags |= kAudioFormatFlagIsPacked;
			break;

		case 4 ... 7:
		case 9 ... 15:
		case 17 ... 23:
		case 25 ... 31:
			// Align high because Apple's AudioConverter doesn't handle low alignment
			mFormat.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
			break;

		default:
		{
			os_log_error(OS_LOG_DEFAULT, "Unsupported bit depth: %d", mFormat.mBitsPerChannel);

			if(error) {
				SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a supported True Audio file."), ""));
				SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Bit depth not supported"), ""));
				SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's bit depth is not supported."), ""));

				*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::FileFormatNotSupportedError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			}

			return false;
		}
	}

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatTrueAudio;

	mSourceFormat.mSampleRate			= streamInfo.sps;
	mSourceFormat.mChannelsPerFrame		= streamInfo.nch;
	mSourceFormat.mBitsPerChannel		= streamInfo.bps;

	// Setup the channel layout
	switch(streamInfo.nch) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}

	mTotalFrames = streamInfo.samples;

	return true;
}

bool SFB::Audio::TrueAudioDecoder::_Close(CFErrorRef */*error*/)
{
	mDecoder.reset();
	mCallbacks.reset();

	mTotalFrames = mCurrentFrame = 0;

	return true;
}

SFB::CFString SFB::Audio::TrueAudioDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("True Audio, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::TrueAudioDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	UInt32 framesRead = 0;
	bool eos = false;

	try {
		while(mFramesToSkip && !eos) {
			if(mFramesToSkip >= frameCount) {
				framesRead = (UInt32)mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount);
				mFramesToSkip -= framesRead;
			}
			else {
				framesRead = (UInt32)mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, mFramesToSkip);
				mFramesToSkip = 0;
			}

			if(0 == framesRead)
				eos = true;
		}

		if(!eos) {
			framesRead = (UInt32)mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount);
			if(0 == framesRead)
				eos = true;
		}
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "True Audio decoding error: %d", e.code());
		return 0;
	}

	if(eos)
		return 0;

	bufferList->mBuffers[0].mDataByteSize = (UInt32)(framesRead * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	mCurrentFrame += framesRead;
	return framesRead;
}

SInt64 SFB::Audio::TrueAudioDecoder::_SeekToFrame(SInt64 frame)
{
	TTAuint32 seconds = (TTAuint32)(frame / mSourceFormat.mSampleRate);
	TTAuint32 frame_start = 0;

	try {
		mDecoder->set_position(seconds, &frame_start);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "True Audio seek error: %d", e.code());
		return -1;
	}

	mCurrentFrame = frame;

	// We need to skip some samples from start of the frame if required
	mFramesToSkip = UInt32((seconds - frame_start) * mSourceFormat.mSampleRate + 0.5);

	return mCurrentFrame;
}
