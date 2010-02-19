/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>
#include <libkern/OSAtomic.h>
#include <stdexcept>
#include <typeinfo>

#include <FLAC/metadata.h>

#include "AudioEngineDefines.h"
#include "FLACDecoder.h"


//__attribute__ ((constructor)) static void register_decoder()
//{
//}

#pragma mark Callbacks


static FLAC__StreamDecoderWriteStatus 
writeCallback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	return flacDecoder->Write(decoder, frame, buffer);
}

static void
metadataCallback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	flacDecoder->Metadata(decoder, metadata);
}

static void
errorCallback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	flacDecoder->Error(decoder, status);
}


#pragma mark Static Methods


CFArrayRef FLACDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("flac"), CFSTR("oga") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 2, &kCFTypeArrayCallBacks);
}

CFArrayRef FLACDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/flac"), CFSTR("audio/ogg") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 2, &kCFTypeArrayCallBacks);
}

bool FLACDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


FLACDecoder::FLACDecoder(CFURLRef url)
	: AudioDecoder(url), mFLAC(NULL), mCurrentFrame(0), mBufferList(NULL)
{
	assert(NULL != url);
	
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(FALSE == success)
		throw std::runtime_error("CFURLGetFileSystemRepresentation failed");
	
	// Create FLAC decoder
	mFLAC = FLAC__stream_decoder_new();
	if(NULL == mFLAC)
		throw std::runtime_error("FLAC__stream_decoder_new failed");

	CFStringRef extension = CFURLCopyPathExtension(url);
	if(NULL == extension)
		throw std::runtime_error("CFURLCopyPathExtension failed");

	// Initialize decoder
	FLAC__StreamDecoderInitStatus status = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_file(mFLAC, 
												reinterpret_cast<const char *>(buf),
												writeCallback, 
												metadataCallback, 
												errorCallback,
												this);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_ogg_file(mFLAC, 
													reinterpret_cast<const char *>(buf),
													writeCallback, 
													metadataCallback, 
													errorCallback,
													this);

	CFRelease(extension), extension = NULL;
	
	if(FLAC__STREAM_DECODER_INIT_STATUS_OK != status) {
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		throw std::runtime_error("FLAC__stream_decoder_init_file (or FLAC__stream_decoder_init_ogg_file) failed");
	}

	// Process metadata
	if(FALSE == FLAC__stream_decoder_process_until_end_of_metadata(mFLAC)) {
		if(FALSE == FLAC__stream_decoder_finish(mFLAC))
			ERR("FLAC__stream_decoder_finish failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));

		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		
		throw std::runtime_error("FLAC__stream_decoder_process_until_end_of_metadata failed");
	}
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= mStreamInfo.sample_rate;
	mFormat.mChannelsPerFrame	= mStreamInfo.channels;
	mFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;
	
	mFormat.mBytesPerPacket		= sizeof(FLAC__int32);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= 'FLAC';

	mSourceFormat.mSampleRate			= mStreamInfo.sample_rate;
	mSourceFormat.mChannelsPerFrame		= mStreamInfo.channels;
	mSourceFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;

	mSourceFormat.mFramesPerPacket		= mStreamInfo.max_blocksize;
	
	switch(mStreamInfo.channels) {
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
		case 3:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_3_0_A;		break;
		case 4:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Quadraphonic;		break;
		case 5:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_0_A;		break;
		case 6:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_1_A;		break;
	}
	
	// Allocate the buffer list (which will convert from FLAC's push model to Core Audio's pull model)
	mBufferList = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * mFormat.mChannelsPerFrame)));

	if(NULL == mBufferList) {
		if(FALSE == FLAC__stream_decoder_finish(mFLAC))
			ERR("FLAC__stream_decoder_finish failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));
		
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;

		throw std::bad_alloc();
	}
	
	mBufferList->mNumberBuffers = mFormat.mChannelsPerFrame;
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		mBufferList->mBuffers[i].mData = calloc(mStreamInfo.max_blocksize, sizeof(FLAC__int32));
		
		if(NULL == mBufferList->mBuffers[i].mData) {
			if(FALSE == FLAC__stream_decoder_finish(mFLAC))
				ERR("FLAC__stream_decoder_finish failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));
			
			FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
			
			for(UInt32 j = 0; j < i; ++j)
				free(mBufferList->mBuffers[j].mData), mBufferList->mBuffers[j].mData = NULL;
			
			free(mBufferList), mBufferList = NULL;
			
			throw std::bad_alloc();
		}
		
		mBufferList->mBuffers[i].mNumberChannels = 1;
	}
}

FLACDecoder::~FLACDecoder()
{	
	FLAC__bool result = FLAC__stream_decoder_finish(mFLAC);
	if(FALSE == result)
		ERR("FLAC__stream_decoder_finish failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));
	
	FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
	
	if(mBufferList) {
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
			free(mBufferList->mBuffers[i].mData), mBufferList->mBuffers[i].mData = NULL;	
		free(mBufferList), mBufferList = NULL;
	}
}


#pragma mark Functionality


CFStringRef FLACDecoder::CreateSourceFormatDescription()
{
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("FLAC, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 FLACDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());

	FLAC__bool result = FLAC__stream_decoder_seek_absolute(mFLAC, frame);	
	
	// Attempt to re-sync the stream if necessary
	if(FLAC__STREAM_DECODER_SEEK_ERROR == FLAC__stream_decoder_get_state(mFLAC))
		result = FLAC__stream_decoder_flush(mFLAC);
	
	if(result) {
		mCurrentFrame = frame;
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
			mBufferList->mBuffers[i].mDataByteSize = 0;	
	}
	
	return (result ? frame : -1);
}

UInt32 FLACDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	UInt32 framesRead = 0;
	
	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;
	
	for(;;) {
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / sizeof(FLAC__int32));
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(FLAC__int32));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);
		
		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			FLAC__int32 *pullBuffer = static_cast<FLAC__int32 *>(bufferList->mBuffers[i].mData);
			memcpy(pullBuffer + framesToSkip, mBufferList->mBuffers[i].mData, framesToCopy * sizeof(FLAC__int32));
			bufferList->mBuffers[i].mDataByteSize += static_cast<UInt32>(framesToCopy * sizeof(FLAC__int32));
			
			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				pullBuffer = static_cast<FLAC__int32 *>(mBufferList->mBuffers[i].mData);
				memmove(pullBuffer, pullBuffer + framesToCopy, (framesInBuffer - framesToCopy) * sizeof(FLAC__int32));
			}
			
			mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToCopy * sizeof(FLAC__int32));
		}
		
		framesRead += framesToCopy;
		
		// All requested frames were read
		if(framesRead == frameCount)
			break;
		
		// EOS?
		if(FLAC__STREAM_DECODER_END_OF_STREAM == FLAC__stream_decoder_get_state(mFLAC))
			break;
		
		// Grab the next frame
		FLAC__bool result = FLAC__stream_decoder_process_single(mFLAC);
		if(FALSE == result)
			ERR("FLAC__stream_decoder_process_single failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));
	}
	
	mCurrentFrame += framesRead;
	
	return framesRead;
}


#pragma mark Callbacks


FLAC__StreamDecoderWriteStatus FLACDecoder::Write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	assert(NULL != decoder);
	assert(NULL != frame);

	// Avoid segfaults
	if(NULL == mBufferList || mBufferList->mNumberBuffers != frame->header.channels)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	
	// FLAC hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
	UInt32 shift = static_cast<UInt32>((8 * sizeof(FLAC__int32)) - frame->header.bits_per_sample);
	
	for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
		FLAC__int32 *pullBuffer = static_cast<FLAC__int32 *>(mBufferList->mBuffers[channel].mData);
		
		for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
			*pullBuffer++ = buffer[channel][sample] << shift;
		
		mBufferList->mBuffers[channel].mNumberChannels		= 1;
		mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * sizeof(FLAC__int32));
	}
	
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;	
}

void FLACDecoder::Metadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata)
{
	assert(NULL != decoder);
	assert(NULL != metadata);
	
	switch(metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			memcpy(&mStreamInfo, &metadata->data.stream_info, sizeof(metadata->data.stream_info));
			break;
			
		default:
			break;
	}
}

void FLACDecoder::Error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
	assert(NULL != decoder);
	
	ERR("FLAC error: %s", FLAC__StreamDecoderErrorStatusString[status]);
}
