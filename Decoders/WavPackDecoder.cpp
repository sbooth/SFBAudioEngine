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

#include "WavPackDecoder.h"
#include "CFErrorUtilities.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

#define BUFFER_SIZE_FRAMES 2048

#pragma mark Callbacks

static int32_t
read_bytes_callback(void *id, void *data, int32_t bcount)
{
	assert(nullptr != id);

	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	return static_cast<int32_t>(decoder->GetInputSource()->Read(data, bcount));
}

static uint32_t
get_pos_callback(void *id)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	return static_cast<uint32_t>(decoder->GetInputSource()->GetOffset());
}

static int
set_pos_abs_callback(void *id, uint32_t pos)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	return !decoder->GetInputSource()->SeekToOffset(pos);
}

static int
set_pos_rel_callback(void *id, int32_t delta, int mode)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	InputSource *inputSource = decoder->GetInputSource();

	if(!inputSource->SupportsSeeking())
		return -1;
	
	// Adjust offset as required
	SInt64 offset = delta;
	switch(mode) {
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

// FIXME: How does one emulate ungetc when the data is non-seekable?
static int
push_back_byte_callback(void *id, int c)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	InputSource *inputSource = decoder->GetInputSource();
	
	if(!inputSource->SupportsSeeking())
		return EOF;
	
	if(!inputSource->SeekToOffset(inputSource->GetOffset() - 1))
		return EOF;
	
	return c;
}

static uint32_t
get_length_callback(void *id)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	return static_cast<uint32_t>(decoder->GetInputSource()->GetLength());
}

static int
can_seek_callback(void *id)
{
	assert(nullptr != id);
	
	WavPackDecoder *decoder = static_cast<WavPackDecoder *>(id);
	return static_cast<uint32_t>(decoder->GetInputSource()->SupportsSeeking());
}

#pragma mark Static Methods

CFArrayRef WavPackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wv") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef WavPackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wavpack") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool WavPackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wv"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool WavPackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wavpack"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

WavPackDecoder::WavPackDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mWPC(nullptr), mTotalFrames(0), mCurrentFrame(0)
{
	memset(&mStreamReader, 0, sizeof(mStreamReader));
}

WavPackDecoder::~WavPackDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool WavPackDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.WavPack", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	mStreamReader.read_bytes = read_bytes_callback;
	mStreamReader.get_pos = get_pos_callback;
	mStreamReader.set_pos_abs = set_pos_abs_callback;
	mStreamReader.set_pos_rel = set_pos_rel_callback;
	mStreamReader.push_back_byte = push_back_byte_callback;
	mStreamReader.get_length = get_length_callback;
	mStreamReader.can_seek = can_seek_callback;
	
	char errorBuf [80];
	
	// Setup converter
	mWPC = WavpackOpenFileInputEx(&mStreamReader, this, nullptr, errorBuf, OPEN_WVC | OPEN_NORMALIZE, 0);
	if(nullptr == mWPC) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a WavPack file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	// Floating-point and lossy files will be handed off in the canonical Core Audio format
	int mode = WavpackGetMode(mWPC);
	if(MODE_FLOAT & mode || !(MODE_LOSSLESS & mode)) {
		// Canonical Core Audio format
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
		
		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
		mFormat.mChannelsPerFrame	= WavpackGetNumChannels(mWPC);		
		mFormat.mBitsPerChannel		= 8 * sizeof(float);
		
		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
	}
	else {
		mFormat.mFormatID			= kAudioFormatLinearPCM;
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

		// Don't set kAudioFormatFlagIsAlignedHigh for 32-bit integer files
		mFormat.mFormatFlags		|= (32 == WavpackGetBitsPerSample(mWPC) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh);

		mFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
		mFormat.mChannelsPerFrame	= WavpackGetNumChannels(mWPC);
		mFormat.mBitsPerChannel		= WavpackGetBitsPerSample(mWPC);
		
		mFormat.mBytesPerPacket		= sizeof(int32_t);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
		
		mFormat.mReserved			= 0;
	}
	
	mTotalFrames						= WavpackGetNumSamples(mWPC);
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'WVPK';
	
	mSourceFormat.mSampleRate			= WavpackGetSampleRate(mWPC);
	mSourceFormat.mChannelsPerFrame		= WavpackGetNumChannels(mWPC);
	mSourceFormat.mBitsPerChannel		= WavpackGetBitsPerSample(mWPC);
	
	// Setup the channel layout
	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}
	
	mBuffer = static_cast<int32_t *>(calloc(BUFFER_SIZE_FRAMES * mFormat.mChannelsPerFrame, sizeof(int32_t)));

	if(nullptr == mBuffer) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		
		return false;		
	}

	mIsOpen = true;
	return true;
}

bool WavPackDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.WavPack", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	memset(&mStreamReader, 0, sizeof(mStreamReader));

	if(mWPC)
		WavpackCloseFile(mWPC), mWPC = nullptr;
	
	if(mBuffer)
		free(mBuffer), mBuffer = nullptr;

	mIsOpen = false;
	return true;
}

CFStringRef WavPackDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;
	
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("WavPack, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 WavPackDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	int result = WavpackSeekSample(mWPC, static_cast<uint32_t>(frame));
	if(result)
		mCurrentFrame = frame;
	
	return (result ? mCurrentFrame : -1);
}

UInt32 WavPackDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	UInt32 framesRemaining = frameCount;
	UInt32 totalFramesRead = 0;
	
	while(0 < framesRemaining) {
		UInt32 framesToRead = std::min(framesRemaining, static_cast<UInt32>(BUFFER_SIZE_FRAMES));
		
		// Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
		uint32_t samplesRead = WavpackUnpackSamples(mWPC, mBuffer, framesToRead);
		
		if(0 == samplesRead)
			break;
		
		// The samples returned are handled differently based on the file's mode
		int mode = WavpackGetMode(mWPC);
		
		// Floating point files require no special handling other than deinterleaving
		if(MODE_FLOAT & mode) {
			float *inputBuffer = reinterpret_cast<float *>(mBuffer);
			
			// Deinterleave the samples
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = inputBuffer[sample];
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
			}
		}
		// Lossless files will be handed off as integers
		else if(MODE_LOSSLESS & mode) {
			// WavPack hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
			UInt32 shift = static_cast<UInt32>(8 * (sizeof(int32_t) - WavpackGetBytesPerSample(mWPC)));
			
			// Deinterleave the 32-bit samples and shift to high-alignment
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				int32_t *shiftedBuffer = static_cast<int32_t *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*shiftedBuffer++ = mBuffer[sample] << shift;
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(int32_t));
			}		
		}
		// Convert lossy files to float
		else {
			float scaleFactor = (1 << ((WavpackGetBytesPerSample(mWPC) * 8) - 1));
			
			// Deinterleave the 32-bit samples and convert to float
			for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
				float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
				
				for(UInt32 sample = channel; sample < samplesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
					*floatBuffer++ = mBuffer[sample] / scaleFactor;
				
				bufferList->mBuffers[channel].mNumberChannels	= 1;
				bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(samplesRead * sizeof(float));
			}
		}
		
		totalFramesRead += samplesRead;
		framesRemaining -= samplesRead;
	}
	
	mCurrentFrame += totalFramesRead;
	
	return totalFramesRead;
}
