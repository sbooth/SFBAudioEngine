/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include <stdexcept>
#include <Accelerate/Accelerate.h>

#include "MPEGDecoder.h"
#include "CFErrorUtilities.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

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
	return decoder->GetInputSource()->Read(ptr, size);
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
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MPEGDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
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

	mDecoder = mpg123_new(nullptr, nullptr);
	if(nullptr == mDecoder) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// Force decode to floating point instead of 16-bit signed integer
	mpg123_param(mDecoder, MPG123_FLAGS, MPG123_FORCE_FLOAT | MPG123_SKIP_ID3V2 | MPG123_GAPLESS | MPG123_QUIET, 0);
	mpg123_param(mDecoder, MPG123_RESYNC_LIMIT, 2048, 0);

	if(MPG123_OK != mpg123_replace_reader_handle(mDecoder, read_callback, lseek_callback, nullptr)) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;

		return false;
	}

	if(MPG123_OK != mpg123_open_handle(mDecoder, this)) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;

		return false;
 	}

	long rate;
	int channels, encoding;
	if(MPG123_OK != mpg123_getformat(mDecoder, &rate, &channels, &encoding) || MPG123_ENC_FLOAT_32 != encoding || 0 >= channels) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;

		return false;
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= rate;
	mFormat.mChannelsPerFrame	= channels;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;

	size_t bufferSizeBytes = mpg123_outblock(mDecoder);
	UInt32 framesPerMPEGFrame = static_cast<UInt32>(bufferSizeBytes / (channels * sizeof(float)));

	// Set up the source format
	mSourceFormat.mFormatID				= 'MPEG';
	
	mSourceFormat.mSampleRate			= rate;
	mSourceFormat.mChannelsPerFrame		= channels;

	mSourceFormat.mFramesPerPacket		= framesPerMPEGFrame;
	
	// Setup the channel layout
	switch(channels) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
	}

	if(MPG123_OK != mpg123_scan(mDecoder)) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MP3 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;

		return false;
	}
	
	// Allocate the buffer list
	mBufferList = AllocateABL(mFormat, framesPerMPEGFrame);
	
	if(nullptr == mBufferList) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		
		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;
		
		return false;
	}
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mIsOpen = true;
	return true;
}

bool MPEGDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(mDecoder) {
		mpg123_close(mDecoder);
		mpg123_delete(mDecoder), mDecoder = nullptr;
	}

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
	if(MPG123_OK != mpg123_info(mDecoder, &mi)) {
		return CFStringCreateWithFormat(kCFAllocatorDefault, 
										nullptr, 
										CFSTR("MPEG-1 Audio, %u channels, %u Hz"), 
										mSourceFormat.mChannelsPerFrame, 
										static_cast<unsigned int>(mSourceFormat.mSampleRate));
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
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MPEGDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	frame = mpg123_seek(mDecoder, frame, SEEK_SET);
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
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);
		
		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			float *floatBuffer = static_cast<float *>(bufferList->mBuffers[i].mData);
			memcpy(floatBuffer + framesToSkip, mBufferList->mBuffers[i].mData, framesToCopy * sizeof(float));
			bufferList->mBuffers[i].mDataByteSize += static_cast<UInt32>(framesToCopy * sizeof(float));
			
			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				floatBuffer = static_cast<float *>(mBufferList->mBuffers[i].mData);
				memmove(floatBuffer, floatBuffer + framesToCopy, (framesInBuffer - framesToCopy) * sizeof(float));
			}
			
			mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToCopy * sizeof(float));
		}
		
		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		// Read and decode an MPEG frame
		off_t frameNumber;
		unsigned char *audioData = nullptr;
		size_t bytesDecoded;
		int result = mpg123_decode_frame(mDecoder, &frameNumber, &audioData, &bytesDecoded);

		if(MPG123_DONE == result)
			break;
		else if(MPG123_OK != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MPEG", "mpg123_decode_frame failed: " << mpg123_strerror(mDecoder));
			break;
		}

		// The analyzer error about division by zero may be safely ignored, because mChannelsPerFrame is verified > 0 in Open()
		UInt32 framesDecoded = static_cast<UInt32>(bytesDecoded / (sizeof(float) * mFormat.mChannelsPerFrame));

		// Deinterleave the samples
		// In my experiments adding zero using Accelerate.framework is faster than looping through the buffer and copying each sample
		float zero = 0;
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *inputBuffer = reinterpret_cast<float *>(audioData) + channel;
			float *outputBuffer = static_cast<float *>(mBufferList->mBuffers[channel].mData);

			vDSP_vsadd(inputBuffer, mFormat.mChannelsPerFrame, &zero, outputBuffer, 1, framesDecoded);

			mBufferList->mBuffers[channel].mNumberChannels	= 1;
			mBufferList->mBuffers[channel].mDataByteSize	= static_cast<UInt32>(framesDecoded * sizeof(float));
		}		
	}
	
	mCurrentFrame += framesRead;

	return framesRead;
}
