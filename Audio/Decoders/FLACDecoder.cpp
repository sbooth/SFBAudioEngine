/*
 *  Copyright (C) 2006, 2007, 2008, 2009 Stephen F. Booth <me@sbooth.org>
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

#include "AudioEngineDefines.h"
#include "FLACDecoder.h"

#include <FLAC/metadata.h>


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


bool FLACDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;
//	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
//		return true;

	return false;
}

bool FLACDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
//	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
//		return true;
	
	return false;
}


#pragma mark Creation and Destruction


FLACDecoder::FLACDecoder(CFURLRef url)
	: AudioDecoder(url), mFLAC(NULL), mCurrentFrame(0), mBufferList(NULL)
{
	assert(NULL != url);
	
	// Create FLAC decoder
	mFLAC = FLAC__stream_decoder_new();

	if(NULL == mFLAC) {
		ERR("FLAC__stream_decoder_new failed");
		return;
	}
	
	UInt8 buf [PATH_MAX];
	if(FALSE == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		ERR("CFURLGetFileSystemRepresentation failed");

	// Initialize decoder
	FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_file(mFLAC, 
																		  reinterpret_cast<const char *>(buf),
																		  writeCallback, 
																		  metadataCallback, 
																		  errorCallback,
																		  this);

	if(FLAC__STREAM_DECODER_INIT_STATUS_OK != status)
		ERR("FLAC__stream_decoder_init_file failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));

	// Process metadata
	FLAC__bool result = FLAC__stream_decoder_process_until_end_of_metadata(mFLAC);

	if(FALSE == result)
		ERR("FLAC__stream_decoder_process_until_end_of_metadata failed: %s", FLAC__stream_decoder_get_resolved_state_string(mFLAC));
	
	mFormat.mSampleRate			= mStreamInfo.sample_rate;
	mFormat.mChannelsPerFrame	= mStreamInfo.channels;
	
	// The source's PCM format
	mSourceFormat.mFormatID				= kAudioFormatLinearPCM;
	mSourceFormat.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;

	mSourceFormat.mSampleRate			= mStreamInfo.sample_rate;
	mSourceFormat.mChannelsPerFrame		= mStreamInfo.channels;
	mSourceFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;

	mSourceFormat.mBytesPerPacket		= ((mSourceFormat.mBitsPerChannel + 7) / 8) * mSourceFormat.mChannelsPerFrame;
	mSourceFormat.mFramesPerPacket		= 1;
	mSourceFormat.mBytesPerFrame		= mSourceFormat.mBytesPerPacket * mSourceFormat.mFramesPerPacket;		
	
	switch(mStreamInfo.channels) {
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
		case 3:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_3_0_A;		break;
		case 4:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Quadraphonic;		break;
		case 5:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_0_A;		break;
		case 6:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_1_A;		break;
	}
	
	// Allocate the buffer list
	mBufferList = static_cast<AudioBufferList *>(calloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer) * (mFormat.mChannelsPerFrame - 1)), 1));
	
	mBufferList->mNumberBuffers = mFormat.mChannelsPerFrame;
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		mBufferList->mBuffers[i].mData = calloc(mStreamInfo.max_blocksize, sizeof(float));
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
		for(unsigned i = 0; i < mBufferList->mNumberBuffers; ++i)
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
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesToCopy	= (framesInBuffer > framesRemaining ? framesRemaining : framesInBuffer);
		
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
	
	// Normalize audio
	float scaleFactor = (1 << ((((frame->header.bits_per_sample + 7) / 8) * 8) - 1));
	
	for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
		float *floatBuffer = static_cast<float *>(mBufferList->mBuffers[channel].mData);
		
		for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
			*floatBuffer++ = buffer[channel][sample] / scaleFactor;
		
		mBufferList->mBuffers[channel].mNumberChannels		= 1;
		mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * sizeof(float));
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

void FLACDecoder::Error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus /*status*/)
{
	assert(NULL != decoder);
	
}
