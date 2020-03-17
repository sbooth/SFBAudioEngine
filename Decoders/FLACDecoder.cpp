/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <AudioToolbox/AudioFormat.h>

#include <FLAC/metadata.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "FLACDecoder.h"

namespace {

	void RegisterFLACDecoder() __attribute__ ((constructor));
	void RegisterFLACDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::FLACDecoder>();
	}

#pragma mark Callbacks

	FLAC__StreamDecoderReadStatus readCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__byte buffer[], size_t *bytes, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		SFB::InputSource& inputSource = flacDecoder->GetInputSource();

		*bytes = (size_t)inputSource.Read(buffer, (SInt64)*bytes);

		if(0 == *bytes)
			return (inputSource.AtEOF() ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_ABORT);

		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}

	FLAC__StreamDecoderSeekStatus seekCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 absolute_byte_offset, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		SFB::InputSource& inputSource = flacDecoder->GetInputSource();

		if(!inputSource.SupportsSeeking())
			return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

		if(!inputSource.SeekToOffset((SInt64)absolute_byte_offset))
			return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
	}

	FLAC__StreamDecoderTellStatus tellCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 *absolute_byte_offset, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);

		*absolute_byte_offset = (FLAC__uint64)flacDecoder->GetInputSource().GetOffset();

		if(-1ULL == *absolute_byte_offset)
			return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

		return FLAC__STREAM_DECODER_TELL_STATUS_OK;
	}

	FLAC__StreamDecoderLengthStatus lengthCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 *stream_length, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);

		*stream_length = (FLAC__uint64)flacDecoder->GetInputSource().GetLength();

		if(-1ULL == *stream_length)
			return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}

	FLAC__bool eofCallback(const FLAC__StreamDecoder */*decoder*/, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		return flacDecoder->GetInputSource().AtEOF();
	}

	FLAC__StreamDecoderWriteStatus writeCallback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		return flacDecoder->Write(decoder, frame, buffer);
	}

	void metadataCallback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		flacDecoder->Metadata(decoder, metadata);
	}

	void errorCallback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
	{
		assert(nullptr != client_data);

		SFB::Audio::FLACDecoder *flacDecoder = static_cast<SFB::Audio::FLACDecoder *>(client_data);
		flacDecoder->Error(decoder, status);
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::FLACDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("flac"), CFSTR("oga") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::FLACDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/flac"), CFSTR("audio/ogg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::FLACDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::FLACDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::FLACDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new FLACDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::FLACDecoder::FLACDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mFLAC(nullptr, nullptr), mCurrentFrame(0)
{
	memset(&mStreamInfo, 0, sizeof(mStreamInfo));
}

#pragma mark Functionality

bool SFB::Audio::FLACDecoder::_Open(CFErrorRef *error)
{
	SFB::CFString extension(CFURLCopyPathExtension(GetURL()));
	if(!extension)
		return false;

	// Create FLAC decoder
	mFLAC = unique_FLAC_ptr(FLAC__stream_decoder_new(), [](FLAC__StreamDecoder *decoder){
		if(decoder) {
			if(!FLAC__stream_decoder_finish(decoder))
				os_log_info(OS_LOG_DEFAULT, "FLAC__stream_decoder_finish failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(decoder));

			FLAC__stream_decoder_delete(decoder);
		}
	});

	if(!mFLAC) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	// Initialize decoder
	FLAC__StreamDecoderInitStatus status = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;

	// Attempt to create a stream decoder based on the file's extension
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_stream(mFLAC.get(),
												  readCallback,
												  seekCallback,
												  tellCallback,
												  lengthCallback,
												  eofCallback,
												  writeCallback,
												  metadataCallback,
												  errorCallback,
												  this);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_ogg_stream(mFLAC.get(),
													  readCallback,
													  seekCallback,
													  tellCallback,
													  lengthCallback,
													  eofCallback,
													  writeCallback,
													  metadataCallback,
													  errorCallback,
													  this);

	if(FLAC__STREAM_DECODER_INIT_STATUS_OK != status) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Process metadata
	if(!FLAC__stream_decoder_process_until_end_of_metadata(mFLAC.get())) {
		os_log_error(OS_LOG_DEFAULT, "FLAC__stream_decoder_process_until_end_of_metadata failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(mFLAC.get()));

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= mStreamInfo.sample_rate;
	mFormat.mChannelsPerFrame	= mStreamInfo.channels;
	mFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;

	mFormat.mBytesPerPacket		= (mStreamInfo.bits_per_sample + 7) / 8;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// FLAC supports from 4 to 32 bits per sample
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
			os_log_error(OS_LOG_DEFAULT, "Unsupported bit depth: %u", mFormat.mBitsPerChannel);

			if(error) {
				SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a supported FLAC file."), ""));
				SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Bit depth not supported"), ""));
				SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's bit depth is not supported."), ""));

				*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::FileFormatNotSupportedError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			}

			return false;
		}
	}

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatFLAC;

	mSourceFormat.mSampleRate			= mStreamInfo.sample_rate;
	mSourceFormat.mChannelsPerFrame		= mStreamInfo.channels;
	mSourceFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;

	mSourceFormat.mFramesPerPacket		= mStreamInfo.max_blocksize;

	switch(mStreamInfo.channels) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 3:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_3_0_A);	break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_A);	break;
		case 6:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);	break;
		case 7:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_6_1_A);	break;
		case 8:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_7_1_A);	break;
	}

	// Allocate the buffer list (which will convert from FLAC's push model to Core Audio's pull model)
	if(!mBufferList.Allocate(mFormat, mStreamInfo.max_blocksize)) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return true;
}

bool SFB::Audio::FLACDecoder::_Close(CFErrorRef */*error*/)
{
	mFLAC.reset();
	mBufferList.Deallocate();
	memset(&mStreamInfo, 0, sizeof(mStreamInfo));

	return true;
}

SFB::CFString SFB::Audio::FLACDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("FLAC, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::FLACDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
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
		UInt32	framesToSkip	= (UInt32)(bufferList->mBuffers[0].mDataByteSize / mFormat.mBytesPerFrame);
		UInt32	framesInBuffer	= (UInt32)(mBufferList->mBuffers[0].mDataByteSize / mFormat.mBytesPerFrame);
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);

		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			unsigned char *pullBuffer = (unsigned char *)bufferList->mBuffers[i].mData;
			memcpy(pullBuffer + (framesToSkip * mFormat.mBytesPerFrame), mBufferList->mBuffers[i].mData, framesToCopy * mFormat.mBytesPerFrame);
			bufferList->mBuffers[i].mDataByteSize += framesToCopy * mFormat.mBytesPerFrame;

			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				pullBuffer = (unsigned char *)mBufferList->mBuffers[i].mData;
				memmove(pullBuffer, pullBuffer + (framesToCopy * mFormat.mBytesPerFrame), (framesInBuffer - framesToCopy) * mFormat.mBytesPerFrame);
			}

			mBufferList->mBuffers[i].mDataByteSize -= (UInt32)(framesToCopy * mFormat.mBytesPerFrame);
		}

		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		// EOS?
		if(FLAC__STREAM_DECODER_END_OF_STREAM == FLAC__stream_decoder_get_state(mFLAC.get()))
			break;

		// Grab the next frame
		FLAC__bool result = FLAC__stream_decoder_process_single(mFLAC.get());
		if(!result)
			os_log_error(OS_LOG_DEFAULT, "FLAC__stream_decoder_process_single failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(mFLAC.get()));
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::FLACDecoder::_SeekToFrame(SInt64 frame)
{
	FLAC__bool result = FLAC__stream_decoder_seek_absolute(mFLAC.get(), (FLAC__uint64)frame);

	// Attempt to re-sync the stream if necessary
	if(FLAC__STREAM_DECODER_SEEK_ERROR == FLAC__stream_decoder_get_state(mFLAC.get()))
		result = FLAC__stream_decoder_flush(mFLAC.get());

	if(result) {
		mCurrentFrame = frame;
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
			mBufferList->mBuffers[i].mDataByteSize = 0;
	}

	return (result ? frame : -1);
}

#pragma mark Callbacks

FLAC__StreamDecoderWriteStatus SFB::Audio::FLACDecoder::Write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	assert(IsOpen());
	assert(nullptr != decoder);
	assert(nullptr != frame);

	// Avoid segfaults
	if(nullptr == mBufferList || mBufferList->mNumberBuffers != frame->header.channels)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	// FLAC hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
	UInt32 shift = (kAudioFormatFlagIsPacked & mFormat.mFormatFlags) ? 0 : (8 * mFormat.mBytesPerFrame) - mFormat.mBitsPerChannel;

	// Convert to native endian samples, high-aligned if necessary
	switch(mFormat.mBytesPerFrame) {
		case 1:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				char *pullBuffer = (char *)mBufferList->mBuffers[channel].mData;

				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = (char)(buffer[channel][sample] << shift);

				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= frame->header.blocksize * sizeof(char);
			}

			break;
		}

		case 2:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				short *pullBuffer = (short *)mBufferList->mBuffers[channel].mData;

				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = (short)(buffer[channel][sample] << shift);

				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= frame->header.blocksize * sizeof(short);
			}

			break;
		}

		case 3:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				unsigned char *pullBuffer = (unsigned char *)mBufferList->mBuffers[channel].mData;

				FLAC__int32 value;
				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample) {
					value = buffer[channel][sample] << shift;
#if __BIG_ENDIAN__
					*pullBuffer++ = (unsigned char)((value >> 16) & 0xff);
					*pullBuffer++ = (unsigned char)((value >> 8) & 0xff);
					*pullBuffer++ = (unsigned char)(value & 0xff);
#elif __LITTLE_ENDIAN__
					*pullBuffer++ = (unsigned char)(value & 0xff);
					*pullBuffer++ = (unsigned char)((value >> 8) & 0xff);
					*pullBuffer++ = (unsigned char)((value >> 16) & 0xff);
#else
#  error Unknown OS byte order
#endif
				}

				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= frame->header.blocksize * 3 * sizeof(unsigned char);
			}

			break;
		}

		case 4:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				int *pullBuffer = (int *)mBufferList->mBuffers[channel].mData;

				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = (int)(buffer[channel][sample] << shift);

				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= frame->header.blocksize * sizeof(int);
			}

			break;
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void SFB::Audio::FLACDecoder::Metadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata)
{
	assert(nullptr != decoder);
	assert(nullptr != metadata);

	switch(metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			memcpy(&mStreamInfo, &metadata->data.stream_info, sizeof(metadata->data.stream_info));
			break;

		default:
			break;
	}
}

void SFB::Audio::FLACDecoder::Error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
	assert(nullptr != decoder);

	os_log_error(OS_LOG_DEFAULT, "FLAC error: %{public}s", FLAC__StreamDecoderErrorStatusString[status]);
}
