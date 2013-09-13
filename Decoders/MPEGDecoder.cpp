/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include <algorithm>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Accelerate/Accelerate.h>

#include "MPEGDecoder.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

static void RegisterMPEGDecoder() __attribute__ ((constructor));
static void RegisterMPEGDecoder()
{
	AudioDecoder::RegisterSubclass<MPEGDecoder>();
}

#pragma mark Initialization

static void Setupmpg123() __attribute__ ((constructor));
static void Setupmpg123()
{
	// What happens if this fails?
	int result = mpg123_init();
	if(MPG123_OK != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "Unable to initialize mpg123: " << mpg123_plain_strerror(result));
}

static void Teardownmpg123() __attribute__ ((destructor));
static void Teardownmpg123()
{
	mpg123_exit();
}

#pragma mark Callbacks

static ssize_t
read_callback(void *dataSource, void *ptr, size_t size)
{
	assert(nullptr != dataSource);
	
	MPEGDecoder *decoder = static_cast<MPEGDecoder *>(dataSource);
	return (ssize_t)decoder->GetInputSource()->Read(ptr, (SInt64)size);
}

static off_t
lseek_callback(void *datasource, off_t offset, int whence)
{
	assert(nullptr != datasource);
	
	MPEGDecoder *decoder = static_cast<MPEGDecoder *>(datasource);
	InputSource *inputSource = decoder->GetInputSource();
	
	if(!inputSource->SupportsSeeking())
		return -1;
	
	// Adjust offset as required
	switch(whence) {
		case SEEK_SET:
			// offset remains unchanged
			break;
		case SEEK_CUR:
			offset += inputSource->GetOffset();
			break;
		case SEEK_END:
			offset += inputSource->GetLength();
			break;
	}
	
	if(!inputSource->SeekToOffset(offset))
		return -1;

	return offset;
}

#pragma mark Static Methods

CFArrayRef MPEGDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mp3") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MPEGDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool MPEGDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp3"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MPEGDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

AudioDecoder * MPEGDecoder::CreateDecoder(InputSource *inputSource)
{
	return new MPEGDecoder(inputSource);
}

#pragma mark Creation and Destruction

MPEGDecoder::MPEGDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mDecoder(nullptr), mBufferList(nullptr), mCurrentFrame(0)
{}

MPEGDecoder::~MPEGDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool MPEGDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	auto decoder = std::unique_ptr<mpg123_handle, std::function<void (mpg123_handle *)>>(mpg123_new(nullptr, nullptr), [](mpg123_handle *mh) {
		mpg123_close(mh);
		mpg123_delete(mh);
	});

	if(!decoder) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}
		
		return false;
	}

	// Force decode to floating point instead of 16-bit signed integer
	mpg123_param(decoder.get(), MPG123_FLAGS, MPG123_FORCE_FLOAT | MPG123_SKIP_ID3V2 | MPG123_GAPLESS | MPG123_QUIET, 0);
	mpg123_param(decoder.get(), MPG123_RESYNC_LIMIT, 2048, 0);

	if(MPG123_OK != mpg123_replace_reader_handle(decoder.get(), read_callback, lseek_callback, nullptr)) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(MPG123_OK != mpg123_open_handle(decoder.get(), this)) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}
		
		return false;
 	}

	long rate;
	int channels, encoding;
	if(MPG123_OK != mpg123_getformat(decoder.get(), &rate, &channels, &encoding) || MPG123_ENC_FLOAT_32 != encoding || 0 >= channels) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
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
	mSourceFormat.mFormatID				= 'MPEG';
	
	mSourceFormat.mSampleRate			= rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)channels;

	mSourceFormat.mFramesPerPacket		= framesPerMPEGFrame;
	
	// Setup the channel layout
	switch(channels) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
	}

	if(MPG123_OK != mpg123_scan(decoder.get())) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}
	
	// Allocate the buffer list
	mBufferList = AllocateABL(mFormat, framesPerMPEGFrame);
	
	if(nullptr == mBufferList) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		
		return false;
	}
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mDecoder = std::move(decoder);

	mIsOpen = true;
	return true;
}

bool MPEGDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	mDecoder.reset();

	if(mBufferList)
		mBufferList = DeallocateABL(mBufferList);

	mIsOpen = false;
	return true;
}

CFStringRef MPEGDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	mpg123_frameinfo mi;
	if(MPG123_OK != mpg123_info(mDecoder.get(), &mi)) {
		return CFStringCreateWithFormat(kCFAllocatorDefault, 
										nullptr, 
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

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("MPEG-1 Audio (%@), %@, %u Hz"), 
									layerDescription,
									channelDescription,
									(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 MPEGDecoder::GetTotalFrames() const
{
	return mpg123_length(mDecoder.get());
}

SInt64 MPEGDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	frame = mpg123_seek(mDecoder.get(), frame, SEEK_SET);
	if(0 <= frame)
		mCurrentFrame = frame;
	
	return ((0 <= frame) ? mCurrentFrame : -1);
}

UInt32 MPEGDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

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
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "mpg123_decode_frame failed: " << mpg123_strerror(mDecoder.get()));
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
