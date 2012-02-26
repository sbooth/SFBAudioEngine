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

#include <AudioToolbox/AudioFormat.h>
#include <stdexcept>

#include "OggVorbisDecoder.h"
#include "CFErrorUtilities.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

#define BUFFER_SIZE_FRAMES 2048

#pragma mark Callbacks

static size_t
read_func_callback(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	assert(nullptr != datasource);
	
	OggVorbisDecoder *decoder = static_cast<OggVorbisDecoder *>(datasource);
	return decoder->GetInputSource()->Read(ptr, size * nmemb);
}

static int
seek_func_callback(void *datasource, ogg_int64_t offset, int whence)
{
	assert(nullptr != datasource);
	
	OggVorbisDecoder *decoder = static_cast<OggVorbisDecoder *>(datasource);
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
	
	return (!inputSource->SeekToOffset(offset));
}

static long
tell_func_callback(void *datasource)
{
	assert(nullptr != datasource);
	
	OggVorbisDecoder *decoder = static_cast<OggVorbisDecoder *>(datasource);
	return static_cast<long>(decoder->GetInputSource()->GetOffset());
}

#pragma mark Static Methods

CFArrayRef OggVorbisDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ogg"), CFSTR("oga") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 2, &kCFTypeArrayCallBacks);
}

CFArrayRef OggVorbisDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/ogg-vorbis") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool OggVorbisDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ogg"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool OggVorbisDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg-vorbis"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

OggVorbisDecoder::OggVorbisDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource)
{
	memset(&mVorbisFile, 0, sizeof(mVorbisFile));
}

OggVorbisDecoder::~OggVorbisDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool OggVorbisDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	ov_callbacks callbacks;
	callbacks.read_func = read_func_callback;
	callbacks.seek_func = seek_func_callback;
	callbacks.tell_func = tell_func_callback;
	callbacks.close_func = nullptr;
	
	if(0 != ov_test_callbacks(this, &mVorbisFile, nullptr, 0, callbacks)) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg Vorbis file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an Ogg Vorbis file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	if(0 != ov_test_open(&mVorbisFile)) {
		LOGGER_CRIT("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "ov_test_open failed");

		if(0 != ov_clear(&mVorbisFile))
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "ov_clear failed");
		
		return false;
	}
	
	vorbis_info *ovInfo = ov_info(&mVorbisFile, -1);
	if(nullptr == ovInfo) {
		LOGGER_CRIT("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "ov_info failed");

		if(0 != ov_clear(&mVorbisFile))
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "ov_clear failed");
		
		return false;
	}
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	mFormat.mSampleRate			= ovInfo->rate;
	mFormat.mChannelsPerFrame	= ovInfo->channels;
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'VORB';
	
	mSourceFormat.mSampleRate			= ovInfo->rate;
	mSourceFormat.mChannelsPerFrame		= ovInfo->channels;
	
	switch(ovInfo->channels) {
			// Default channel layouts from Vorbis I specification section 4.3.9
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
			// FIXME: Is this the right tag for 3 channels?
		case 3:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_3_0_A);		break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_C);		break;
		case 6:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_C);		break;
	}

	mIsOpen = true;
	return true;
}

bool OggVorbisDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(0 != ov_clear(&mVorbisFile))
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "ov_clear failed");

	mIsOpen = false;
	return true;
}

CFStringRef OggVorbisDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("Ogg Vorbis, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 OggVorbisDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	if(0 != ov_pcm_seek(&mVorbisFile, frame))
		return -1;
	
	return this->GetCurrentFrame();
}

UInt32 OggVorbisDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	float		**buffer			= nullptr;
	UInt32		framesRemaining		= frameCount;
	UInt32		totalFramesRead		= 0;
	int			currentSection		= 0;

	// Mark the output buffers as empty
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		bufferList->mBuffers[i].mDataByteSize = 0;
		bufferList->mBuffers[i].mNumberChannels = 1;
	}
		
	while(0 < framesRemaining) {
		// Decode a chunk of samples from the file
		long framesRead = ov_read_float(&mVorbisFile, 
										&buffer, 
										std::min(BUFFER_SIZE_FRAMES, static_cast<int>(framesRemaining)), 
										&currentSection);
			
		if(0 > framesRead) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.OggVorbis", "Ogg Vorbis decoding error");
			return 0;
		}
		
		// 0 frames indicates EOS
		if(0 == framesRead)
			break;
		
		// Copy the frames from the decoding buffer to the output buffer
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			// Skip over any frames already decoded
			memcpy(static_cast<float *>(bufferList->mBuffers[channel].mData) + totalFramesRead, buffer[channel], framesRead * sizeof(float));
			bufferList->mBuffers[channel].mDataByteSize += static_cast<UInt32>(framesRead * sizeof(float));
		}
		
		totalFramesRead += static_cast<UInt32>(framesRead);
		framesRemaining -= static_cast<UInt32>(framesRead);
	}
	
	return totalFramesRead;
}
