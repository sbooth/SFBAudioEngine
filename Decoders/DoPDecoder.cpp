/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <array>

#include <os/log.h>

#include "CFErrorUtilities.h"
#include "DoPDecoder.h"

#define DSD_FRAMES_PER_DOP_FRAME 16

namespace {
	// Bit reversal lookup table from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
	static const uint8_t sBitReverseTable256 [256] =
	{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
		R6(0), R6(2), R6(1), R6(3)
	};

	// Support DSD64, DSD128, and DSD256 (64x, 128x, and 256x the CD sample rate of 44.1 KHz)
	// as well as the 48.0 KHz variants 6.144 MHz and 12.288 MHz
	static const std::array<Float64, 5> sSupportedSampleRates = { {2822400, 5644800, 11289600, 6144000, 12288000} };
}

#pragma mark Factory Methods

SFB::Audio::Decoder::unique_ptr SFB::Audio::DoPDecoder::CreateForURL(CFURLRef url, CFErrorRef *error)
{
	return CreateForInputSource(InputSource::CreateForURL(url, 0, error), error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DoPDecoder::CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	return CreateForDecoder(Decoder::CreateForInputSource(std::move(inputSource), error), error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DoPDecoder::CreateForDecoder(unique_ptr decoder, CFErrorRef *error)
{
#pragma unused(error)

	if(!decoder)
		return nullptr;

	return unique_ptr(new DoPDecoder(std::move(decoder)));
}

SFB::Audio::DoPDecoder::DoPDecoder(Decoder::unique_ptr decoder)
	: mDecoder(std::move(decoder)), mMarker(0x05), mReverseBits(false)
{
	assert(nullptr != mDecoder);
}

bool SFB::Audio::DoPDecoder::_Open(CFErrorRef *error)
{
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	const auto& decoderFormat = mDecoder->GetFormat();

	if(!decoderFormat.IsDSD()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(std::end(sSupportedSampleRates) == std::find(std::begin(sSupportedSampleRates), std::end(sSupportedSampleRates), decoderFormat.mSampleRate)) {
		os_log_error(OS_LOG_DEFAULT, "Unsupported sample rate: %f", decoderFormat.mSampleRate);

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not supported."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unsupported DSD sample rate"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's sample rate is not supported for DSD over PCM."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(!mBufferList.Allocate(decoderFormat, 4096)) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	// Generate non-interleaved 24-bit big endian output
	mFormat.mFormatID			= kAudioFormatDoP;
	mFormat.mFormatFlags		= kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= decoderFormat.mSampleRate / DSD_FRAMES_PER_DOP_FRAME;
	mFormat.mChannelsPerFrame	= decoderFormat.mChannelsPerFrame;
	mFormat.mBitsPerChannel		= 24;

	mFormat.mBytesPerPacket		= mFormat.mBitsPerChannel / 8;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	mReverseBits = !(kAudioFormatFlagIsBigEndian & decoderFormat.mFormatFlags);

	return true;
}

bool SFB::Audio::DoPDecoder::_Close(CFErrorRef *error)
{
	if(!mDecoder->Close(error))
		return false;

	mBufferList.Deallocate();

	return true;
}

SFB::CFString SFB::Audio::DoPDecoder::_GetSourceFormatDescription() const
{
	return CFString(mDecoder->CreateSourceFormatDescription());
}

#pragma mark Functionality

UInt32 SFB::Audio::DoPDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// Only multiples of 16 frames can be read (16 frames equals two bytes)
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 != frameCount % 16) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		// Grab the DSD audio
		UInt32 framesRemaining = frameCount - framesRead;
		UInt32 dsdFramesRemaining = DSD_FRAMES_PER_DOP_FRAME * framesRemaining;
		UInt32 dsdFramesDecoded = mDecoder->ReadAudio(mBufferList, std::min(mBufferList.GetCapacityFrames(), dsdFramesRemaining));
		if(0 == dsdFramesDecoded)
			break;

		UInt32 framesDecoded = dsdFramesDecoded / DSD_FRAMES_PER_DOP_FRAME;

		// Convert to DoP
		// NB: Currently DSDIFFDecoder and DSFDecoder only produce non-interleaved output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			const unsigned char *src = (const unsigned char *)mBufferList->mBuffers[i].mData;
			unsigned char *dst = (unsigned char *)bufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize;

			for(UInt32 j = 0; j < framesDecoded; ++j) {
				// Insert the DSD marker
				*dst++ = mMarker;

				// Copy the DSD bits
				if(mReverseBits) {
					*dst++ = sBitReverseTable256[*src++];
					*dst++ = sBitReverseTable256[*src++];
				}
				else {
					*dst++ = *src++;
					*dst++ = *src++;
				}

				mMarker = (uint8_t)0x05 == mMarker ? (uint8_t)0xfa : (uint8_t)0x05;
			}

			bufferList->mBuffers[i].mDataByteSize += mFormat.FrameCountToByteCount(framesDecoded);
		}

		framesRead += framesDecoded;

		// All requested frames were read
		if(framesRead == frameCount)
			break;
	}

	return framesRead;
}

SInt64 SFB::Audio::DoPDecoder::_GetTotalFrames() const
{
	return mDecoder->GetTotalFrames() / DSD_FRAMES_PER_DOP_FRAME;
}

SInt64 SFB::Audio::DoPDecoder::_GetCurrentFrame() const
{
	return mDecoder->GetCurrentFrame() / DSD_FRAMES_PER_DOP_FRAME;
}

SInt64 SFB::Audio::DoPDecoder::_SeekToFrame(SInt64 frame)
{
	if(-1 == mDecoder->SeekToFrame(DSD_FRAMES_PER_DOP_FRAME * frame))
		return -1;

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return _GetCurrentFrame();
}
