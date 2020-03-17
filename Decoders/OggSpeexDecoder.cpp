/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioFormat.h>

#include <speex/speex.h>
#include <speex/speex_callbacks.h>
#include <speex/speex_header.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "OggSpeexDecoder.h"

#define MAX_FRAME_SIZE 2000
#define READ_SIZE_BYTES 4096

namespace {

	void RegisterOggSpeexDecoder() __attribute__ ((constructor));
	void RegisterOggSpeexDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::OggSpeexDecoder>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::OggSpeexDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("spx") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::OggSpeexDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/speex"), CFSTR("audio/ogg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::OggSpeexDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("spx"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::OggSpeexDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/speex"), kCFCompareCaseInsensitive))
		return true;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::OggSpeexDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new OggSpeexDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::OggSpeexDecoder::OggSpeexDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mCurrentFrame(0), mTotalFrames(-1), mSpeexDecoder(nullptr), mSpeexStereoState(nullptr), mSpeexSerialNumber(-1), mSpeexEOSReached(false), mSpeexFramesPerOggPacket(0), mOggPacketCount(0), mExtraSpeexHeaderCount(0)
{}

SFB::Audio::OggSpeexDecoder::~OggSpeexDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::OggSpeexDecoder::_Open(CFErrorRef *error)
{
	// Initialize Ogg data struct
	ogg_sync_init(&mOggSyncState);

	// Get the ogg buffer for writing
	char *data = ogg_sync_buffer(&mOggSyncState, READ_SIZE_BYTES);

	// Read bitstream from input file
	ssize_t bytesRead = (ssize_t)GetInputSource().Read(data, READ_SIZE_BYTES);
	if(-1 == bytesRead) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” could not be read."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Read error"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("Unable to read from the input file."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	// Tell the sync layer how many bytes were written to its internal buffer
	int result = ogg_sync_wrote(&mOggSyncState, bytesRead);
	if(-1 == result) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	// Turn the data we wrote into an ogg page
	result = ogg_sync_pageout(&mOggSyncState, &mOggPage);
	if(1 != result) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	// Initialize the stream and grab the serial number
	ogg_stream_init(&mOggStreamState, ogg_page_serialno(&mOggPage));

	// Get the first Ogg page
	result = ogg_stream_pagein(&mOggStreamState, &mOggPage);
	if(0 != result) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	// Get the first packet (should be the header) from the page
	ogg_packet op;
	result = ogg_stream_packetout(&mOggStreamState, &op);
	if(1 != result) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	if(op.bytes >= 5 && !memcmp(op.packet, "Speex", 5))
		mSpeexSerialNumber = mOggStreamState.serialno;

	++mOggPacketCount;

	// Convert the packet to the Speex header
	SpeexHeader *header = speex_packet_to_header((char *)op.packet, (int)op.bytes);
	if(nullptr == header) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg Speex file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an Ogg Speex file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::FileFormatNotRecognizedError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}
	else if(SPEEX_NB_MODES <= header->mode) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The Speex mode in the file “%@” is not supported."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unsupported Ogg Speex file mode"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("This file may have been encoded with a newer version of Speex."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::FileFormatNotSupportedError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		speex_header_free(header);
		header = nullptr;
		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	const SpeexMode *mode = speex_lib_get_mode(header->mode);
	if(mode->bitstream_version != header->mode_bitstream_version) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The Speex version in the file “%@” is not supported."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unsupported Ogg Speex file version"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("This file was encoded with a different version of Speex."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::FileFormatNotSupportedError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		speex_header_free(header);
		header = nullptr;
		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	// Initialize the decoder
	mSpeexDecoder = speex_decoder_init(mode);
	if(nullptr== mSpeexDecoder) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("Unable to initialize the Speex decoder."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Error initializing Speex decoder"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("An unknown error occurred."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		speex_header_free(header);
		header = nullptr;
		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	speex_decoder_ctl(mSpeexDecoder, SPEEX_SET_SAMPLING_RATE, &header->rate);

	mSpeexFramesPerOggPacket = (0 == header->frames_per_packet ? 1 : header->frames_per_packet);
	mExtraSpeexHeaderCount = (UInt32)header->extra_headers;

	// Initialize the speex bit-packing data structure
	speex_bits_init(&mSpeexBits);

	// Initialize the stereo mode
	mSpeexStereoState = speex_stereo_state_init();

	if(2 == header->nb_channels) {
		SpeexCallback callback;
		callback.callback_id = SPEEX_INBAND_STEREO;
		callback.func = speex_std_stereo_request_handler;
		callback.data = mSpeexStereoState;
		speex_decoder_ctl(mSpeexDecoder, SPEEX_SET_HANDLER, &callback);
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	mFormat.mSampleRate			= header->rate;
	mFormat.mChannelsPerFrame	= (UInt32)header->nb_channels;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatSpeex;

	mSourceFormat.mSampleRate			= header->rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)header->nb_channels;

	switch(header->nb_channels) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);		break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);	break;
	}

	speex_header_free(header);
	header = nullptr;

	// Allocate the buffer list
	spx_int32_t speexFrameSize = 0;
	speex_decoder_ctl(mSpeexDecoder, SPEEX_GET_FRAME_SIZE, &speexFrameSize);

	if(!mBufferList.Allocate(mFormat, (UInt32)speexFrameSize)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		speex_stereo_state_destroy(mSpeexStereoState);
		mSpeexStereoState = nullptr;
		speex_decoder_destroy(mSpeexDecoder);
		mSpeexDecoder = nullptr;
		speex_bits_destroy(&mSpeexBits);

		ogg_sync_destroy(&mOggSyncState);
		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return true;
}

bool SFB::Audio::OggSpeexDecoder::_Close(CFErrorRef */*error*/)
{
	mBufferList.Deallocate();

	// Speex cleanup
	speex_stereo_state_destroy(mSpeexStereoState);
	mSpeexStereoState = nullptr;
	speex_decoder_destroy(mSpeexDecoder);
	mSpeexDecoder = nullptr;
	speex_bits_destroy(&mSpeexBits);

	// Ogg cleanup
	ogg_stream_clear(&mOggStreamState);
	ogg_sync_clear(&mOggSyncState);

	return true;
}

SFB::CFString SFB::Audio::OggSpeexDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("Ogg Speex, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::OggSpeexDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
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

		// EOS reached
		if(mSpeexEOSReached)
			break;

		// Attempt to process the desired number of packets
		unsigned packetsDesired = 1;
		while(0 < packetsDesired && !mSpeexEOSReached) {

			// Process any packets in the current page
			while(0 < packetsDesired && !mSpeexEOSReached) {

				// Grab a packet from the streaming layer
				ogg_packet oggPacket;
				int result = ogg_stream_packetout(&mOggStreamState, &oggPacket);
				if(-1 == result) {
					os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding error: Ogg loss of streaming");
					break;
				}

				// If result is 0, there is insufficient data to assemble a packet
				if(0 == result)
					break;

				// Otherwise, we got a valid packet for processing
				if(1 == result) {
					if(5 <= oggPacket.bytes && !memcmp(oggPacket.packet, "Speex", 5))
						mSpeexSerialNumber = mOggStreamState.serialno;

					if(-1 == mSpeexSerialNumber || mOggStreamState.serialno != mSpeexSerialNumber)
						break;

					// Ignore the following:
					//  - Speex comments in packet #2
					//  - Extra headers (optionally) in packets 3+
					if(1 != mOggPacketCount && 1 + mExtraSpeexHeaderCount <= mOggPacketCount) {
						// Detect Speex EOS
						if(oggPacket.e_o_s && mOggStreamState.serialno == mSpeexSerialNumber)
							mSpeexEOSReached = true;

						// SPEEX_GET_FRAME_SIZE is in samples
						spx_int32_t speexFrameSize;
						speex_decoder_ctl(mSpeexDecoder, SPEEX_GET_FRAME_SIZE, &speexFrameSize);
						float buffer [(2 == mFormat.mChannelsPerFrame) ? 2 * speexFrameSize : speexFrameSize];

						// Copy the Ogg packet to the Speex bitstream
						speex_bits_read_from(&mSpeexBits, (char *)oggPacket.packet, (int)oggPacket.bytes);

						// Decode each frame in the Speex packet
						for(spx_int32_t i = 0; i < mSpeexFramesPerOggPacket; ++i) {

							result = speex_decode(mSpeexDecoder, &mSpeexBits, buffer);

							// -1 indicates EOS
							if(-1 == result)
								break;
							else if(-2 == result) {
								os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding error: possible corrupted stream");
								break;
							}

							if(0 > speex_bits_remaining(&mSpeexBits)) {
								os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding overflow: possible corrupted stream");
								break;
							}

							// Normalize the values
							float maxSampleValue = 1u << 15;
							vDSP_vsdiv(buffer, 1, &maxSampleValue, buffer, 1, (vDSP_Length)speexFrameSize);

							// Copy the frames from the decoding buffer to the output buffer, skipping over any frames already decoded
							framesInBuffer = mBufferList->mBuffers[0].mDataByteSize / sizeof(float);
							memcpy((float *)mBufferList->mBuffers[0].mData + framesInBuffer, buffer, (size_t)speexFrameSize * sizeof(float));
							mBufferList->mBuffers[0].mDataByteSize += (size_t)speexFrameSize * sizeof(float);

							// Process stereo channel, if present
							if(2 == mFormat.mChannelsPerFrame) {
								speex_decode_stereo(buffer, speexFrameSize, mSpeexStereoState);
								vDSP_vsdiv(buffer + speexFrameSize, 1, &maxSampleValue, buffer + speexFrameSize, 1, (vDSP_Length)speexFrameSize);

								memcpy((float *)mBufferList->mBuffers[1].mData + framesInBuffer, buffer + speexFrameSize, (size_t)speexFrameSize * sizeof(float));
								mBufferList->mBuffers[1].mDataByteSize += (size_t)speexFrameSize * sizeof(float);
							}

							// Packet processing finished
							--packetsDesired;
						}
					}

					++mOggPacketCount;
				}
			}

			// Grab a new Ogg page for processing, if necessary
			if(!mSpeexEOSReached && 0 < packetsDesired) {
				while(1 != ogg_sync_pageout(&mOggSyncState, &mOggPage)) {
					// Get the ogg buffer for writing
					char *data = ogg_sync_buffer(&mOggSyncState, READ_SIZE_BYTES);

					// Read bitstream from input file
					ssize_t bytesRead = (ssize_t)GetInputSource().Read(data, READ_SIZE_BYTES);
					if(-1 == bytesRead) {
						os_log_error(OS_LOG_DEFAULT, "Unable to read from the input file");
						break;
					}

					ogg_sync_wrote(&mOggSyncState, bytesRead);

					// No more data available from input file
					if(0 == bytesRead)
						break;
				}

				// Ensure all Ogg streams are read
				if(ogg_page_serialno(&mOggPage) != mOggStreamState.serialno)
					ogg_stream_reset_serialno(&mOggStreamState, ogg_page_serialno(&mOggPage));

				// Get the resultant Ogg page
				int result = ogg_stream_pagein(&mOggStreamState, &mOggPage);
				if(0 != result) {
					os_log_error(OS_LOG_DEFAULT, "Error reading Ogg page");
					break;
				}
			}
		}
	}

	mCurrentFrame += framesRead;

	if(0 == framesRead && mSpeexEOSReached)
		mTotalFrames = mCurrentFrame;

	return framesRead;
}
