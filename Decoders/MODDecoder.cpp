/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

//#include <CoreServices/CoreServices.h>

#include "MODDecoder.h"
#include "CFErrorUtilities.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

#define DUMB_SAMPLE_RATE	44100
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16

#pragma mark Callbacks

static int skip_callback(void *f, long n)
{
	assert(nullptr != f);
	
	MODDecoder *decoder = static_cast<MODDecoder *>(f);
	return (decoder->GetInputSource()->SeekToOffset(decoder->GetInputSource()->GetOffset() + n) ? 0 : 1);
}

static int getc_callback(void *f)
{
	assert(nullptr != f);
	
	MODDecoder *decoder = static_cast<MODDecoder *>(f);
	
	uint8_t value;
	return (1 == decoder->GetInputSource()->Read(&value, 1) ? value : -1);
}

static long getnc_callback(char *ptr, long n, void *f)
{
	assert(nullptr != f);
	
	MODDecoder *decoder = static_cast<MODDecoder *>(f);
	return static_cast<long>(decoder->GetInputSource()->Read(ptr, n));
}

static void close_callback(void */*f*/)
{}

#pragma mark Static Methods

CFArrayRef MODDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("it"), CFSTR("xm"), CFSTR("s3m"), CFSTR("mod") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 4, &kCFTypeArrayCallBacks);
}

CFArrayRef MODDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/it"), CFSTR("audio/xm"), CFSTR("audio/s3m"), CFSTR("audio/mod"), CFSTR("audio/x-mod") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 5, &kCFTypeArrayCallBacks);
}

bool MODDecoder::HandlesFilesWithExtension(CFStringRef extension)
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

bool MODDecoder::HandlesMIMEType(CFStringRef mimeType)
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

#pragma mark Creation and Destruction

MODDecoder::MODDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), df(nullptr), duh(nullptr), dsr(nullptr), mCurrentFrame(0), mTotalFrames(0)
{}

MODDecoder::~MODDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool MODDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MOD", "Open() called on an AudioDecoder that is already open");		
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

	CFStringRef fileSystemPath = CFURLCopyFileSystemPath(GetURL(), kCFURLPOSIXPathStyle);
	CFStringRef extension = nullptr;

	CFRange range;
	if(CFStringFindWithOptionsAndLocale(fileSystemPath, CFSTR("."), CFRangeMake(0, CFStringGetLength(fileSystemPath)), kCFCompareBackwards, CFLocaleGetSystem(), &range)) {
		extension = CFStringCreateWithSubstring(kCFAllocatorDefault, fileSystemPath, CFRangeMake(range.location + 1, CFStringGetLength(fileSystemPath) - range.location - 1));
	}

	CFRelease(fileSystemPath), fileSystemPath = nullptr;

	if(nullptr == extension) {
		return false;
	}
	
	// Attempt to create the appropriate decoder based on the file's extension
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("it"), kCFCompareCaseInsensitive))
		duh = dumb_read_it(df);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("xm"), kCFCompareCaseInsensitive))
		duh = dumb_read_xm(df);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		duh = dumb_read_s3m(df);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mod"), kCFCompareCaseInsensitive))
		duh = dumb_read_mod(df);
	
	CFRelease(extension), extension = nullptr;

	if(nullptr == duh) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a MOD file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		if(df)
			dumbfile_close(df), df = nullptr;

		return false;
	}

	mTotalFrames = duh_get_length(duh);

	dsr = duh_start_sigrenderer(duh, 0, 2, 0);
	if(nullptr == dsr) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a MOD file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
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

bool MODDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MOD", "Close() called on an AudioDecoder that hasn't been opened");
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

CFStringRef MODDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("MOD, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MODDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	// DUMB cannot seek backwards, so the decoder must be reset
	if(frame < mCurrentFrame) {
		if(!Close(nullptr) || !GetInputSource()->SeekToOffset(0) || !Open(nullptr)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.MOD", "Error reseting DUMB decoder");
			return -1;
		}

		mCurrentFrame = 0;
	}

	long framesToSkip = frame - mCurrentFrame;
	duh_sigrenderer_generate_samples(dsr, 1, static_cast<float>(65536 / DUMB_SAMPLE_RATE), framesToSkip, nullptr);
	mCurrentFrame += framesToSkip;
	
	return mCurrentFrame;
}

UInt32 MODDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	// EOF reached
	if(duh_sigrenderer_get_position(dsr) > mTotalFrames)
		return 0;

	long framesRendered = duh_render(dsr, DUMB_BIT_DEPTH, 0, 1, static_cast<float>(65536.0 / DUMB_SAMPLE_RATE), frameCount, bufferList->mBuffers[0].mData);

	mCurrentFrame += framesRendered;

	bufferList->mBuffers[0].mDataByteSize = static_cast<UInt32>(framesRendered * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return static_cast<UInt32>(framesRendered);
}
