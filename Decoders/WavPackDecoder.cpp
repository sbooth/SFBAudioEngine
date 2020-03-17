/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <AudioToolbox/AudioFormat.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "WavPackDecoder.h"

#define BUFFER_SIZE_FRAMES 2048

namespace {

	void RegisterWavPackDecoder() __attribute__ ((constructor));
	void RegisterWavPackDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::WavPackDecoder>();
	}

#pragma mark Callbacks

	int32_t read_bytes_callback(void *id, void *data, int32_t bcount)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		return (int32_t)decoder->GetInputSource().Read(data, bcount);
	}

	uint32_t get_pos_callback(void *id)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		return (uint32_t)decoder->GetInputSource().GetOffset();
	}

	int set_pos_abs_callback(void *id, uint32_t pos)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		return !decoder->GetInputSource().SeekToOffset(pos);
	}

	int set_pos_rel_callback(void *id, int32_t delta, int mode)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		SFB::InputSource& inputSource = decoder->GetInputSource();

		if(!inputSource.SupportsSeeking())
			return -1;

		// Adjust offset as required
		SInt64 offset = delta;
		switch(mode) {
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

	// FIXME: How does one emulate ungetc when the data is non-seekable?
	int push_back_byte_callback(void *id, int c)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		SFB::InputSource& inputSource = decoder->GetInputSource();

		if(!inputSource.SupportsSeeking())
			return EOF;

		if(!inputSource.SeekToOffset(inputSource.GetOffset() - 1))
			return EOF;

		return c;
	}

	uint32_t get_length_callback(void *id)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		return (uint32_t)decoder->GetInputSource().GetLength();
	}

	int can_seek_callback(void *id)
	{
		assert(nullptr != id);

		auto decoder = static_cast<SFB::Audio::WavPackDecoder *>(id);
		return (int)decoder->GetInputSource().SupportsSeeking();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::WavPackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wv") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::WavPackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wavpack"), CFSTR("audio/x-wavpack") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::WavPackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wv"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::WavPackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wavpack"), kCFCompareCaseInsensitive))
		return true;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-wavpack"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::WavPackDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new WavPackDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::WavPackDecoder::WavPackDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mWPC(nullptr, nullptr), mTotalFrames(0), mCurrentFrame(0)
{
	memset(&mStreamReader, 0, sizeof(mStreamReader));
}

#pragma mark Functionality

bool SFB::Audio::WavPackDecoder::_Open(CFErrorRef *error)
{
	mStreamReader.read_bytes = read_bytes_callback;
	mStreamReader.get_pos = get_pos_callback;
	mStreamReader.set_pos_abs = set_pos_abs_callback;
	mStreamReader.set_pos_rel = set_pos_rel_callback;
	mStreamReader.push_back_byte = push_back_byte_callback;
	mStreamReader.get_length = get_length_callback;
	mStreamReader.can_seek = can_seek_callback;

	char errorBuf [80];

	// Setup converter
	mWPC = unique_WavpackContext_ptr(WavpackOpenFileInputEx(&mStreamReader, this, nullptr, errorBuf, OPEN_WVC | OPEN_NORMALIZE/* | OPEN_DSD_NATIVE*/, 0), WavpackCloseFile);
	if(!mWPC) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a WavPack file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Floating-point and lossy files will be handed off in the canonical Core Audio format
	int mode = WavpackGetMode(mWPC.get());
//	int qmode = WavpackGetQualifyMode(mWPC.get());
	if(MODE_FLOAT & mode || !(MODE_LOSSLESS & mode)) {
		// Canonical Core Audio format
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;

		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC.get());
		mFormat.mChannelsPerFrame	= (UInt32)WavpackGetNumChannels(mWPC.get());
		mFormat.mBitsPerChannel		= 8 * sizeof(float);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mFormat.mReserved			= 0;
	}
//	else if(qmode & QMODE_DSD_AUDIO) {
//	}
	else {
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

		// Don't set kAudioFormatFlagIsAlignedHigh for 32-bit integer files
		mFormat.mFormatFlags		|= (32 == WavpackGetBitsPerSample(mWPC.get()) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh);

		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC.get());
		mFormat.mChannelsPerFrame	= (UInt32)WavpackGetNumChannels(mWPC.get());
		mFormat.mBitsPerChannel		= (UInt32)WavpackGetBitsPerSample(mWPC.get());

		mFormat.mBytesPerPacket		= sizeof(int32_t);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mFormat.mReserved			= 0;
	}

	mTotalFrames						= WavpackGetNumSamples(mWPC.get());

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatWavpack;

	mSourceFormat.mSampleRate			= WavpackGetSampleRate(mWPC.get());
	mSourceFormat.mChannelsPerFrame		= (UInt32)WavpackGetNumChannels(mWPC.get());
	mSourceFormat.mBitsPerChannel		= (UInt32)WavpackGetBitsPerSample(mWPC.get());

	// Setup the channel layout
	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}

	mBuffer = std::unique_ptr<int32_t []>(new int32_t [BUFFER_SIZE_FRAMES * mFormat.mChannelsPerFrame]);
	if(!mBuffer) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	return true;
}

bool SFB::Audio::WavPackDecoder::_Close(CFErrorRef */*error*/)
{
	memset(&mStreamReader, 0, sizeof(mStreamReader));

	mBuffer.reset();
	mWPC.reset();

	return true;
}

SFB::CFString SFB::Audio::WavPackDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("WavPack, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::WavPackDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	UInt32 framesRemaining = frameCount;
	UInt32 totalFramesRead = 0;

	while(0 < framesRemaining) {
		UInt32 framesToRead = std::min(framesRemaining, (UInt32)BUFFER_SIZE_FRAMES);

		// Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
		uint32_t samplesRead = WavpackUnpackSamples(mWPC.get(), mBuffer.get(), framesToRead);

		if(0 == samplesRead)
			break;

		// The samples returned are handled differently based on the file's mode
		int mode = WavpackGetMode(mWPC.get());
//		int qmode = WavpackGetQualifyMode(mWPC.get());

		// Floating point files require no special handling other than deinterleaving
		if(MODE_FLOAT & mode) {
			float *inputBuffer = (float *)mBuffer.get();

			// Deinterleave the samples
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = (float *)bufferList->mBuffers[channel].mData;

				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = inputBuffer[sample];

				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= samplesRead * sizeof(float);
			}
		}
		// Lossless files will be handed off as integers
		else if(MODE_LOSSLESS & mode) {
			// WavPack hands us 32-bit signed ints with the samples low-aligned
			UInt32 shift = (UInt32)(8 * (sizeof(int32_t) - (size_t)WavpackGetBytesPerSample(mWPC.get())));

			// Deinterleave the 32-bit samples, shifting to high alignment
			if(0 < shift) {
				int32_t mask = (1 << shift) - 1;

				for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
					int32_t *buffer = (int32_t *)bufferList->mBuffers[channel].mData;

					for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
						*buffer++ = (mBuffer[sample] & mask) << shift;

					bufferList->mBuffers[channel].mNumberChannels	= 1;
					bufferList->mBuffers[channel].mDataByteSize		= samplesRead * sizeof(int32_t);
				}
			}
			// Just deinterleave the 32-bit samples
			else {
				for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
					int32_t *buffer = (int32_t *)bufferList->mBuffers[channel].mData;

					for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
						*buffer++ = mBuffer[sample];

					bufferList->mBuffers[channel].mNumberChannels	= 1;
					bufferList->mBuffers[channel].mDataByteSize		= samplesRead * sizeof(int32_t);
				}
			}
		}
		// Convert lossy files to float
		else {
			float scaleFactor = (1 << ((WavpackGetBytesPerSample(mWPC.get()) * 8) - 1));

			// Deinterleave the 32-bit samples and convert to float
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = (float *)bufferList->mBuffers[channel].mData;

				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = mBuffer[sample] / scaleFactor;

				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= samplesRead * sizeof(float);
			}
		}

		totalFramesRead += samplesRead;
		framesRemaining -= samplesRead;
	}

	mCurrentFrame += totalFramesRead;

	return totalFramesRead;
}

SInt64 SFB::Audio::WavPackDecoder::_SeekToFrame(SInt64 frame)
{
	int result = WavpackSeekSample(mWPC.get(), (uint32_t)frame);
	if(result)
		mCurrentFrame = frame;

	return (result ? mCurrentFrame : -1);
}
