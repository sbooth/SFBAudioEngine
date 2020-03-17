/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>

#include <unistd.h>
#include <os/log.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Accelerate/Accelerate.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "MPEGDecoder.h"

namespace {

	void RegisterMPEGDecoder() __attribute__ ((constructor));
	void RegisterMPEGDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MPEGDecoder>();
	}

#pragma mark Initialization

	void Setupmpg123() __attribute__ ((constructor));
	void Setupmpg123()
	{
		// What happens if this fails?
		int result = mpg123_init();
		if(MPG123_OK != result)
			os_log_debug(OS_LOG_DEFAULT, "Unable to initialize mpg123: %s", mpg123_plain_strerror(result));
	}

	void Teardownmpg123() __attribute__ ((destructor));
	void Teardownmpg123()
	{
		mpg123_exit();
	}

#pragma mark Callbacks

	ssize_t read_callback(void *dataSource, void *ptr, size_t size)
	{
		assert(nullptr != dataSource);

		auto decoder = static_cast<SFB::Audio::MPEGDecoder *>(dataSource);
		return (ssize_t)decoder->GetInputSource().Read(ptr, (SInt64)size);
	}

	off_t lseek_callback(void *datasource, off_t offset, int whence)
	{
		assert(nullptr != datasource);

		auto decoder = static_cast<SFB::Audio::MPEGDecoder *>(datasource);
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

		return offset;
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MPEGDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mp3") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MPEGDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MPEGDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp3"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MPEGDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MPEGDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MPEGDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MPEGDecoder::MPEGDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mDecoder(nullptr), mCurrentFrame(0)
{}

#pragma mark Functionality

bool SFB::Audio::MPEGDecoder::_Open(CFErrorRef *error)
{
	auto decoder = unique_mpg123_ptr(mpg123_new(nullptr, nullptr), [](mpg123_handle *mh) {
		mpg123_close(mh);
		mpg123_delete(mh);
	});

	if(!decoder) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Force decode to floating point instead of 16-bit signed integer
	mpg123_param(decoder.get(), MPG123_FLAGS, MPG123_FORCE_FLOAT | MPG123_SKIP_ID3V2 | MPG123_GAPLESS | MPG123_QUIET, 0);
	mpg123_param(decoder.get(), MPG123_RESYNC_LIMIT, 2048, 0);

	if(MPG123_OK != mpg123_replace_reader_handle(decoder.get(), read_callback, lseek_callback, nullptr)) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(MPG123_OK != mpg123_open_handle(decoder.get(), this)) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
 	}

	long rate;
	int channels, encoding;
	if(MPG123_OK != mpg123_getformat(decoder.get(), &rate, &channels, &encoding) || MPG123_ENC_FLOAT_32 != encoding || 0 >= channels) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= rate;
	mFormat.mChannelsPerFrame	= (UInt32)channels;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	size_t bufferSizeBytes = mpg123_outblock(decoder.get());
	UInt32 framesPerMPEGFrame = (UInt32)(bufferSizeBytes / ((size_t)channels * sizeof(float)));

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMPEG1;

	mSourceFormat.mSampleRate			= rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)channels;

	mSourceFormat.mFramesPerPacket		= framesPerMPEGFrame;

	// Setup the channel layout
	switch(channels) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);		break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);	break;
	}

	if(MPG123_OK != mpg123_scan(decoder.get())) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Allocate the buffer list
	if(!mBufferList.Allocate(mFormat, framesPerMPEGFrame)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mDecoder = std::move(decoder);

	return true;
}

bool SFB::Audio::MPEGDecoder::_Close(CFErrorRef */*error*/)
{
	mDecoder.reset();
	mBufferList.Deallocate();

	return true;
}

SFB::CFString SFB::Audio::MPEGDecoder::_GetSourceFormatDescription() const
{
	mpg123_frameinfo mi;
	if(MPG123_OK != mpg123_info(mDecoder.get(), &mi)) {
		return CFString(nullptr,
						CFSTR("MPEG-1 Audio, %u channels, %u Hz"),
						(unsigned int)mSourceFormat.mChannelsPerFrame,
						(unsigned int)mSourceFormat.mSampleRate);
	}

	CFStringRef layerDescription = nullptr;
	switch(mi.layer) {
		case 1:							layerDescription = CFSTR("Layer I");			break;
		case 2:							layerDescription = CFSTR("Layer II");			break;
		case 3:							layerDescription = CFSTR("Layer III");			break;
	}

	CFStringRef channelDescription = nullptr;
	switch(mi.mode) {
		case MPG123_M_MONO:				channelDescription = CFSTR("Single Channel");	break;
		case MPG123_M_DUAL:				channelDescription = CFSTR("Dual Channel");		break;
		case MPG123_M_JOINT:			channelDescription = CFSTR("Joint Stereo");		break;
		case MPG123_M_STEREO:			channelDescription = CFSTR("Stereo");			break;
	}

	return CFString(nullptr,
					CFSTR("MPEG-1 Audio (%@), %@, %u Hz"),
					layerDescription,
					channelDescription,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MPEGDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 framesRead = 0;

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

		// Read and decode an MPEG frame
		off_t frameNumber;
		unsigned char *audioData = nullptr;
		size_t bytesDecoded;
		int result = mpg123_decode_frame(mDecoder.get(), &frameNumber, &audioData, &bytesDecoded);

		if(MPG123_DONE == result)
			break;
		else if(MPG123_OK != result) {
			os_log_error(OS_LOG_DEFAULT, "mpg123_decode_frame failed: %s", mpg123_strerror(mDecoder.get()));
			break;
		}

		// The analyzer error about division by zero may be safely ignored, because mChannelsPerFrame is verified > 0 in Open()
		UInt32 framesDecoded = (UInt32)(bytesDecoded / (sizeof(float) * mFormat.mChannelsPerFrame));

		// Deinterleave the samples
		// In my experiments adding zero using Accelerate.framework is faster than looping through the buffer and copying each sample
		float zero = 0;
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *inputBuffer = (float *)audioData + channel;
			float *outputBuffer = (float *)mBufferList->mBuffers[channel].mData;

			vDSP_vsadd(inputBuffer, (vDSP_Stride)mFormat.mChannelsPerFrame, &zero, outputBuffer, 1, framesDecoded);

			mBufferList->mBuffers[channel].mNumberChannels	= 1;
			mBufferList->mBuffers[channel].mDataByteSize	= framesDecoded * sizeof(float);
		}
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::MPEGDecoder::_GetTotalFrames() const
{
	return mpg123_length(mDecoder.get());
}

SInt64 SFB::Audio::MPEGDecoder::_SeekToFrame(SInt64 frame)
{
	frame = mpg123_seek(mDecoder.get(), frame, SEEK_SET);
	if(0 <= frame)
		mCurrentFrame = frame;

	return ((0 <= frame) ? mCurrentFrame : -1);
}
