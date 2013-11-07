/*
 *  Copyright (C) 2013 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "OggOpusDecoder.h"
#include "CreateChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

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
	: Decoder(std::move(inputSource)), mOpusFile(nullptr)
{}

SFB::Audio::OggOpusDecoder::~OggOpusDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::OggOpusDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.OggOpus", "Open() called on a Decoder that is already open");
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;


	OpusFileCallbacks callbacks = {
		.read = read_callback,
		.seek = seek_callback,
		.tell = tell_callback,
		.close = nullptr
	};

	mOpusFile = op_test_callbacks(this, &callbacks, nullptr, 0, nullptr);

	if(nullptr == mOpusFile) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg Opus file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an Ogg Opus file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(0 != op_test_open(mOpusFile)) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.OggOpus", "op_test_open failed");

		op_free(mOpusFile), mOpusFile = nullptr;

		return false;
	}

	const OpusHead *header = op_head(mOpusFile, 0);

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
	mSourceFormat.mFormatID				= 'OPUS';

	mSourceFormat.mSampleRate			= header->input_sample_rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)header->channel_count;

	switch(header->channel_count) {
			// Default channel layouts from Vorbis I specification section 4.3.9
			// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9

		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 3:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_AC3_3_0);		break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_C);	break;
		case 6:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_C);	break;

		case 7:
			mChannelLayout = CreateChannelLayout(7);

			mChannelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
			mChannelLayout->mChannelBitmap = 0;

			mChannelLayout->mNumberChannelDescriptions = 7;

			mChannelLayout->mChannelDescriptions[0].mChannelLabel = kAudioChannelLabel_Left;
			mChannelLayout->mChannelDescriptions[1].mChannelLabel = kAudioChannelLabel_Center;
			mChannelLayout->mChannelDescriptions[2].mChannelLabel = kAudioChannelLabel_Right;
			mChannelLayout->mChannelDescriptions[3].mChannelLabel = kAudioChannelLabel_LeftSurround;
			mChannelLayout->mChannelDescriptions[4].mChannelLabel = kAudioChannelLabel_RightSurround;
			mChannelLayout->mChannelDescriptions[5].mChannelLabel = kAudioChannelLabel_CenterSurround;
			mChannelLayout->mChannelDescriptions[6].mChannelLabel = kAudioChannelLabel_LFEScreen;

			break;

		case 8:
			mChannelLayout = CreateChannelLayout(8);

			mChannelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
			mChannelLayout->mChannelBitmap = 0;

			mChannelLayout->mNumberChannelDescriptions = 8;

			mChannelLayout->mChannelDescriptions[0].mChannelLabel = kAudioChannelLabel_Left;
			mChannelLayout->mChannelDescriptions[1].mChannelLabel = kAudioChannelLabel_Center;
			mChannelLayout->mChannelDescriptions[2].mChannelLabel = kAudioChannelLabel_Right;
			mChannelLayout->mChannelDescriptions[3].mChannelLabel = kAudioChannelLabel_LeftSurround;
			mChannelLayout->mChannelDescriptions[4].mChannelLabel = kAudioChannelLabel_RightSurround;
			mChannelLayout->mChannelDescriptions[5].mChannelLabel = kAudioChannelLabel_RearSurroundLeft;
			mChannelLayout->mChannelDescriptions[6].mChannelLabel = kAudioChannelLabel_RearSurroundRight;
			mChannelLayout->mChannelDescriptions[7].mChannelLabel = kAudioChannelLabel_LFEScreen;
			
			break;
	}

	mIsOpen = true;
	return true;
}

bool SFB::Audio::OggOpusDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.OggOpus", "Close() called on a Decoder that hasn't been opened");
		return true;
	}

	op_free(mOpusFile), mOpusFile = nullptr;

	mIsOpen = false;
	return true;
}

CFStringRef SFB::Audio::OggOpusDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault,
									nullptr,
									CFSTR("Ogg Opus, %u channels, %u Hz"),
									(unsigned int)mSourceFormat.mChannelsPerFrame,
									(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 SFB::Audio::OggOpusDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	if(0 != op_pcm_seek(mOpusFile, frame))
		return -1;

	return this->GetCurrentFrame();
}

UInt32 SFB::Audio::OggOpusDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	float		*buffer				= (float *)bufferList->mBuffers[0].mData;
	UInt32		framesRemaining		= frameCount;
	UInt32		totalFramesRead		= 0;

	while(0 < framesRemaining) {
		int framesRead = op_read_float(mOpusFile, buffer, (int)(framesRemaining * mFormat.mChannelsPerFrame), nullptr);

		if(0 > framesRead) {
			LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.OggOpus", "Ogg Opus decoding error: " << framesRead);
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
