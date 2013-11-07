/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include "MODDecoder.h"
#include "CreateChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

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
	: Decoder(std::move(inputSource)), df(nullptr), duh(nullptr), dsr(nullptr), mCurrentFrame(0), mTotalFrames(0)
{}

SFB::Audio::MODDecoder::~MODDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::MODDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.MOD", "Open() called on a Decoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	dfs.open = nullptr;
	dfs.skip = skip_callback;
	dfs.getc = getc_callback;
	dfs.getnc = getnc_callback;
	dfs.close = close_callback;

	df = dumbfile_open_ex(this, &dfs);
	if(nullptr == df) {
		return false;
	}

	SFB::CFString pathExtension = CFURLCopyPathExtension(GetURL());
	if(nullptr == pathExtension)
		return false;

	// Attempt to create the appropriate decoder based on the file's extension
	if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("it"), kCFCompareCaseInsensitive))
		duh = dumb_read_it(df);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("xm"), kCFCompareCaseInsensitive))
		duh = dumb_read_xm(df);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		duh = dumb_read_s3m(df);
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("mod"), kCFCompareCaseInsensitive))
		duh = dumb_read_mod(df);
	
	if(nullptr == duh) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a MOD file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}
		
		if(df)
			dumbfile_close(df), df = nullptr;

		return false;
	}

	// NB: This must change if the sample rate changes because it is based on 65536 Hz
	mTotalFrames = duh_get_length(duh);

	dsr = duh_start_sigrenderer(duh, 0, 2, 0);
	if(nullptr == dsr) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a MOD file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		if(df)
			dumbfile_close(df), df = nullptr;
		if(duh)
			unload_duh(duh), duh = nullptr;

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
	mSourceFormat.mFormatID				= 'MOD ';
	
	mSourceFormat.mSampleRate			= DUMB_SAMPLE_RATE;
	mSourceFormat.mChannelsPerFrame		= DUMB_CHANNELS;
	
	// Setup the channel layout
	mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);

	mIsOpen = true;
	return true;
}

bool SFB::Audio::MODDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.MOD", "Close() called on a Decoder that hasn't been opened");
		return true;
	}

	if(dsr)
		duh_end_sigrenderer(dsr), dsr = nullptr;
	if(duh)
		unload_duh(duh), duh = nullptr;
	if(df)
		dumbfile_close(df), df = nullptr;

	mIsOpen = false;
	return true;
}

CFStringRef SFB::Audio::MODDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("MOD, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 SFB::Audio::MODDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	// DUMB cannot seek backwards, so the decoder must be reset
	if(frame < mCurrentFrame) {
		if(!Close(nullptr) || !GetInputSource().SeekToOffset(0) || !Open(nullptr)) {
			LOGGER_ERR("org.sbooth.AudioEngine.Decoder.MOD", "Error reseting DUMB decoder");
			return -1;
		}

		mCurrentFrame = 0;
	}

	long framesToSkip = frame - mCurrentFrame;
	duh_sigrenderer_generate_samples(dsr, 1, 65536.0f / DUMB_SAMPLE_RATE, framesToSkip, nullptr);
	mCurrentFrame += framesToSkip;
	
	return mCurrentFrame;
}

UInt32 SFB::Audio::MODDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	// EOF reached
	if(duh_sigrenderer_get_position(dsr) > mTotalFrames) {
		bufferList->mBuffers[0].mDataByteSize = 0;
		return 0;
	}

	long framesRendered = duh_render(dsr, DUMB_BIT_DEPTH, 0, 1, 65536.0f / DUMB_SAMPLE_RATE, frameCount, bufferList->mBuffers[0].mData);

	mCurrentFrame += framesRendered;

	bufferList->mBuffers[0].mDataByteSize = (UInt32)(framesRendered * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)framesRendered;
}
