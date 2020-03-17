/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>

#include <os/log.h>

#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioFormat.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "MusepackDecoder.h"

namespace {

	void RegisterMusepackDecoder() __attribute__ ((constructor));
	void RegisterMusepackDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MusepackDecoder>();
	}

#pragma mark Callbacks

	mpc_int32_t read_callback(mpc_reader *p_reader, void *ptr, mpc_int32_t size)
	{
		assert(nullptr != p_reader);

		auto decoder = static_cast<SFB::Audio::MusepackDecoder *>(p_reader->data);
		return (mpc_int32_t)decoder->GetInputSource().Read(ptr, size);
	}

	mpc_bool_t seek_callback(mpc_reader *p_reader, mpc_int32_t offset)
	{
		assert(nullptr != p_reader);

		auto decoder = static_cast<SFB::Audio::MusepackDecoder *>(p_reader->data);
		return decoder->GetInputSource().SeekToOffset(offset);
	}

	mpc_int32_t tell_callback(mpc_reader *p_reader)
	{
		assert(nullptr != p_reader);

		auto decoder = static_cast<SFB::Audio::MusepackDecoder *>(p_reader->data);
		return (mpc_int32_t)decoder->GetInputSource().GetOffset();
	}

	mpc_int32_t get_size_callback(mpc_reader *p_reader)
	{
		assert(nullptr != p_reader);

		auto decoder = static_cast<SFB::Audio::MusepackDecoder *>(p_reader->data);
		return (mpc_int32_t)decoder->GetInputSource().GetLength();
	}

	mpc_bool_t canseek_callback(mpc_reader *p_reader)
	{
		assert(nullptr != p_reader);

		auto decoder = static_cast<SFB::Audio::MusepackDecoder *>(p_reader->data);
		return decoder->GetInputSource().SupportsSeeking();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MusepackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mpc") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MusepackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/musepack"), CFSTR("audio/x-musepack") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MusepackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mpc"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MusepackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/musepack"), kCFCompareCaseInsensitive))
		return true;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-musepack"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MusepackDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MusepackDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MusepackDecoder::MusepackDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mDemux(nullptr), mTotalFrames(0), mCurrentFrame(0)
{}

SFB::Audio::MusepackDecoder::~MusepackDecoder()
{
	if(IsOpen())
		Close();
}


#pragma mark Functionality


bool SFB::Audio::MusepackDecoder::_Open(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mInputSource->GetURL(), FALSE, buf, PATH_MAX))
		return false;

	mReader.read = read_callback;
	mReader.seek = seek_callback;
	mReader.tell = tell_callback;
	mReader.get_size = get_size_callback;
	mReader.canseek = canseek_callback;
	mReader.data = this;

	mDemux = mpc_demux_init(&mReader);
	if(nullptr == mDemux) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Musepack file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a Musepack file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		mpc_reader_exit_stdio(&mReader);

		return false;
	}

	// Get input file information
	mpc_streaminfo streaminfo;
	mpc_demux_get_info(mDemux, &streaminfo);

	mTotalFrames				= mpc_streaminfo_get_length_samples(&streaminfo);

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= streaminfo.sample_freq;
	mFormat.mChannelsPerFrame	= streaminfo.channels;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMusepack;

	mSourceFormat.mSampleRate			= streaminfo.sample_freq;
	mSourceFormat.mChannelsPerFrame		= streaminfo.channels;

	mSourceFormat.mFramesPerPacket		= (1 << streaminfo.block_pwr);

	// Setup the channel layout
	switch(streaminfo.channels) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}

	// Allocate the buffer list
	if(!mBufferList.Allocate(mFormat, MPC_FRAME_LENGTH)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		mpc_demux_exit(mDemux);
		mDemux = nullptr;
		mpc_reader_exit_stdio(&mReader);

		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return true;
}

bool SFB::Audio::MusepackDecoder::_Close(CFErrorRef */*error*/)
{
	if(mDemux) {
		mpc_demux_exit(mDemux);
		mDemux = nullptr;
	}

    mpc_reader_exit_stdio(&mReader);
	mBufferList.Deallocate();

	return true;
}

SFB::CFString SFB::Audio::MusepackDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("Musepack, %u channels, %u Hz"),
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MusepackDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	MPC_SAMPLE_FORMAT	buffer			[MPC_DECODER_BUFFER_LENGTH];
	UInt32				framesRead		= 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= (UInt32)(bufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesInBuffer	= (UInt32)(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);

		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			float *floatBuffer = (float *)bufferList->mBuffers[i].mData;
			memcpy(floatBuffer + framesToSkip, mBufferList->mBuffers[i].mData, framesToCopy * sizeof(float));
			bufferList->mBuffers[i].mDataByteSize += framesToCopy * sizeof(float);

			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				floatBuffer = (float *)mBufferList->mBuffers[i].mData;
				memmove(floatBuffer, floatBuffer + framesToCopy, (framesInBuffer - framesToCopy) * sizeof(float));
			}

			mBufferList->mBuffers[i].mDataByteSize -= framesToCopy * sizeof(float);
		}

		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		// Decode one frame of MPC data
		mpc_frame_info frame;
		frame.buffer = buffer;

		mpc_status result = mpc_demux_decode(mDemux, &frame);
		if(MPC_STATUS_OK != result) {
			os_log_error(OS_LOG_DEFAULT, "Musepack decoding error");
			break;
		}

		// End of input
		if(-1 == frame.bits)
			break;

#ifdef MPC_FIXED_POINT
#error "Fixed point not yet supported"
#else
		float *inputBuffer = (float *)buffer;

		// Clip the samples to [-1, 1)
		float minValue = -1.f;
		float maxValue = 8388607.f / 8388608.f;

		vDSP_vclip(inputBuffer, 1, &minValue, &maxValue, inputBuffer, 1, frame.samples * mFormat.mChannelsPerFrame);

		// Deinterleave the normalized samples
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *floatBuffer = (float *)mBufferList->mBuffers[channel].mData;

			for(UInt32 sample = channel; sample < frame.samples * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
				*floatBuffer++ = inputBuffer[sample];

			mBufferList->mBuffers[channel].mNumberChannels	= 1;
			mBufferList->mBuffers[channel].mDataByteSize	= frame.samples * sizeof(float);
		}
#endif /* MPC_FIXED_POINT */
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::MusepackDecoder::_SeekToFrame(SInt64 frame)
{
	mpc_status result = mpc_demux_seek_sample(mDemux, (mpc_uint64_t)frame);
	if(MPC_STATUS_OK == result)
		mCurrentFrame = frame;

	return ((MPC_STATUS_OK == result) ? mCurrentFrame : -1);
}
