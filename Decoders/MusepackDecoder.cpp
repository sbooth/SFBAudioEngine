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
#include <Accelerate/Accelerate.h>
#include <algorithm>
#include <stdexcept>

#include "MusepackDecoder.h"
#include "CFErrorUtilities.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

#pragma mark Callbacks

static mpc_int32_t
read_callback(mpc_reader *p_reader, void *ptr, mpc_int32_t size)
{
	assert(nullptr != p_reader);
	
	MusepackDecoder *decoder = static_cast<MusepackDecoder *>(p_reader->data);
	return static_cast<mpc_int32_t>(decoder->GetInputSource()->Read(ptr, size));
}

static mpc_bool_t
seek_callback(mpc_reader *p_reader, mpc_int32_t offset)
{
	assert(nullptr != p_reader);
	
	MusepackDecoder *decoder = static_cast<MusepackDecoder *>(p_reader->data);
	return decoder->GetInputSource()->SeekToOffset(offset);
}

static mpc_int32_t
tell_callback(mpc_reader *p_reader)
{
	assert(nullptr != p_reader);
	
	MusepackDecoder *decoder = static_cast<MusepackDecoder *>(p_reader->data);
	return static_cast<mpc_int32_t>(decoder->GetInputSource()->GetOffset());
}

static mpc_int32_t
get_size_callback(mpc_reader *p_reader)
{
	assert(nullptr != p_reader);
	
	MusepackDecoder *decoder = static_cast<MusepackDecoder *>(p_reader->data);
	return static_cast<mpc_int32_t>(decoder->GetInputSource()->GetLength());
}

static mpc_bool_t
canseek_callback(mpc_reader *p_reader)
{
	assert(nullptr != p_reader);
	
	MusepackDecoder *decoder = static_cast<MusepackDecoder *>(p_reader->data);
	return decoder->GetInputSource()->SupportsSeeking();
}

#pragma mark Static Methods

CFArrayRef MusepackDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mpc") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MusepackDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/musepack") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MusepackDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mpc"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MusepackDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/musepack"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MusepackDecoder::MusepackDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mDemux(nullptr), mTotalFrames(0), mCurrentFrame(0)
{}

MusepackDecoder::~MusepackDecoder()
{
	if(IsOpen())
		Close();
}


#pragma mark Functionality


bool MusepackDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Musepack", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mInputSource->GetURL(), FALSE, buf, PATH_MAX))
		return false;

	mReader.read = read_callback;
	mReader.seek = seek_callback;
	mReader.tell = tell_callback;
	mReader.get_size = get_size_callback;
	mReader.canseek = canseek_callback;
	mReader.data = this;
	
	mDemux = mpc_demux_init(&mReader);
	if(nullptr == mDemux) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Musepack file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a Musepack file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		mpc_reader_exit_stdio(&mReader);
		
		return false;
	}
	
	// Get input file information
	mpc_streaminfo streaminfo;
	mpc_demux_get_info(mDemux, &streaminfo);
	
	mTotalFrames				= mpc_streaminfo_get_length_samples(&streaminfo);
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= streaminfo.sample_freq;
	mFormat.mChannelsPerFrame	= streaminfo.channels;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'MUSE';
	
	mSourceFormat.mSampleRate			= streaminfo.sample_freq;
	mSourceFormat.mChannelsPerFrame		= streaminfo.channels;
	
	mSourceFormat.mFramesPerPacket		= (1 << streaminfo.block_pwr);
	
	// Setup the channel layout
	switch(streaminfo.channels) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}
	
	// Allocate the buffer list
	mBufferList = AllocateABL(mFormat, MPC_FRAME_LENGTH);

	if(nullptr == mBufferList) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		mpc_demux_exit(mDemux), mDemux = nullptr;
		mpc_reader_exit_stdio(&mReader);
		
		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mIsOpen = true;
	return true;
}

bool MusepackDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Musepack", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(mDemux)
		mpc_demux_exit(mDemux), mDemux = nullptr;
	
    mpc_reader_exit_stdio(&mReader);
	
	if(mBufferList)
		mBufferList = DeallocateABL(mBufferList);

	mIsOpen = false;
	return true;
}

CFStringRef MusepackDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("Musepack, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MusepackDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	mpc_status result = mpc_demux_seek_sample(mDemux, frame);
	if(MPC_STATUS_OK == result)
		mCurrentFrame = frame;
	
	return ((MPC_STATUS_OK == result) ? mCurrentFrame : -1);
}

UInt32 MusepackDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	MPC_SAMPLE_FORMAT	buffer			[MPC_DECODER_BUFFER_LENGTH];
	UInt32				framesRead		= 0;
	
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
		
		// Decode one frame of MPC data
		mpc_frame_info frame;
		frame.buffer = buffer;

		mpc_status result = mpc_demux_decode(mDemux, &frame);
		if(MPC_STATUS_OK != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Musepack", "Musepack decoding error");
			break;
		}

		// End of input
		if(-1 == frame.bits)
			break;
		
#ifdef MPC_FIXED_POINT
#error "Fixed point not yet supported"
#else
		float *inputBuffer = reinterpret_cast<float *>(buffer);

		// Clip the samples to [-1, 1)
		float minValue = -1.f;
		float maxValue = 8388607.f / 8388608.f;

		vDSP_vclip(inputBuffer, 1, &minValue, &maxValue, inputBuffer, 1, frame.samples * mFormat.mChannelsPerFrame);

		// Deinterleave the normalized samples
		for(UInt32 channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
			float *floatBuffer = static_cast<float *>(mBufferList->mBuffers[channel].mData);

			for(UInt32 sample = channel; sample < frame.samples * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
				*floatBuffer++ = inputBuffer[sample];
			
			mBufferList->mBuffers[channel].mNumberChannels	= 1;
			mBufferList->mBuffers[channel].mDataByteSize	= static_cast<UInt32>(frame.samples * sizeof(float));
		}
#endif /* MPC_FIXED_POINT */		
	}
	
	mCurrentFrame += framesRead;
	
	return framesRead;
}
