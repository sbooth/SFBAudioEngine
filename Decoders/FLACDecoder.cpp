/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Stephen F. Booth <me@sbooth.org>
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

#include <FLAC/metadata.h>
#include <log4cxx/logger.h>

#include "FLACDecoder.h"
#include "CreateDisplayNameForURL.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"

#pragma mark Callbacks

static FLAC__StreamDecoderReadStatus
readCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	InputSource *inputSource = flacDecoder->GetInputSource();

	*bytes = inputSource->Read(buffer, *bytes);
	
	if(0 == *bytes)
		return (inputSource->AtEOF() ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_ABORT);
	
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus
seekCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	InputSource *inputSource = flacDecoder->GetInputSource();
	
	if(!inputSource->SupportsSeeking())
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	
	if(!inputSource->SeekToOffset(absolute_byte_offset))
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus
tellCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);

	*absolute_byte_offset = flacDecoder->GetInputSource()->GetOffset();
	
	if(-1ULL == *absolute_byte_offset)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
lengthCallback(const FLAC__StreamDecoder */*decoder*/, FLAC__uint64 *stream_length, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);

	*stream_length = flacDecoder->GetInputSource()->GetLength();
	
	if(-1ULL == *stream_length)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool
eofCallback(const FLAC__StreamDecoder */*decoder*/, void *client_data)
{
	assert(NULL != client_data);
	
	FLACDecoder *flacDecoder = static_cast<FLACDecoder *>(client_data);
	return flacDecoder->GetInputSource()->AtEOF();
}

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
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

FLACDecoder::FLACDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mFLAC(NULL), mCurrentFrame(0), mBufferList(NULL)
{}

FLACDecoder::~FLACDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool FLACDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
		LOG4CXX_WARN(logger, "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	// Create FLAC decoder
	mFLAC = FLAC__stream_decoder_new();
	if(NULL == mFLAC) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		return false;
	}
	
	CFStringRef fileSystemPath = CFURLCopyFileSystemPath(GetURL(), kCFURLPOSIXPathStyle);
	CFStringRef extension = NULL;
	
	CFRange range;
	if(CFStringFindWithOptionsAndLocale(fileSystemPath, CFSTR("."), CFRangeMake(0, CFStringGetLength(fileSystemPath)), kCFCompareBackwards, CFLocaleGetSystem(), &range)) {
		extension = CFStringCreateWithSubstring(kCFAllocatorDefault, fileSystemPath, CFRangeMake(range.location + 1, CFStringGetLength(fileSystemPath) - range.location - 1));
	}
	
	CFRelease(fileSystemPath), fileSystemPath = NULL;

	if(NULL == extension) {
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		return false;
	}
	
	// Initialize decoder
	FLAC__StreamDecoderInitStatus status = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;
	
	// Attempt to create a stream decoder based on the file's extension
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_stream(mFLAC,
												  readCallback,
												  seekCallback,
												  tellCallback,
												  lengthCallback,
												  eofCallback,
												  writeCallback,
												  metadataCallback,
												  errorCallback,
												  this);
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		status = FLAC__stream_decoder_init_ogg_stream(mFLAC,
													  readCallback,
													  seekCallback,
													  tellCallback,
													  lengthCallback,
													  eofCallback,
													  writeCallback,
													  metadataCallback,
													  errorCallback,
													  this);
												  
	CFRelease(extension), extension = NULL;
	
	if(FLAC__STREAM_DECODER_INIT_STATUS_OK != status) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioDecoderErrorDomain, 
								   AudioDecoderInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}

		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		
		return false;
	}
	
	// Process metadata
	if(!FLAC__stream_decoder_process_until_end_of_metadata(mFLAC)) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
		LOG4CXX_ERROR(logger, "FLAC__stream_decoder_process_until_end_of_metadata failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));

		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioDecoderErrorDomain, 
								   AudioDecoderInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}

		if(!FLAC__stream_decoder_finish(mFLAC))
			LOG4CXX_WARN(logger, "FLAC__stream_decoder_finish failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));
		
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		
		return false;
	}
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= mStreamInfo.sample_rate;
	mFormat.mChannelsPerFrame	= mStreamInfo.channels;
	mFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;
	
	mFormat.mBytesPerPacket		= (mStreamInfo.bits_per_sample + 7) / 8;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;

	// FLAC supports from 4 to 32 bits per sample
	switch(mFormat.mBitsPerChannel) {
		case 8:
		case 16:
		case 24:
		case 32:
			mFormat.mFormatFlags |= kAudioFormatFlagIsPacked;
			break;

		case 4 ... 7:
		case 9 ... 15:
		case 17 ... 23:
		case 25 ... 31:
			// Align high because Apple's AudioConverter doesn't handle low alignment
			mFormat.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
			break;

		default:
		{
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
			LOG4CXX_ERROR(logger, "Unsupported bit depth: " << mFormat.mBitsPerChannel)

			if(error) {
				CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																				   32,
																				   &kCFTypeDictionaryKeyCallBacks,
																				   &kCFTypeDictionaryValueCallBacks);
				
				CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
				CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																   NULL, 
																   CFCopyLocalizedString(CFSTR("The file “%@” is not a supported FLAC file."), ""), 
																   displayName);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedDescriptionKey, 
									 errorString);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedFailureReasonKey, 
									 CFCopyLocalizedString(CFSTR("Bit depth not supported"), ""));
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedRecoverySuggestionKey, 
									 CFCopyLocalizedString(CFSTR("The file's bit depth is not supported."), ""));
				
				CFRelease(errorString), errorString = NULL;
				CFRelease(displayName), displayName = NULL;
				
				*error = CFErrorCreate(kCFAllocatorDefault, 
									   AudioDecoderErrorDomain, 
									   AudioDecoderInputOutputError, 
									   errorDictionary);
				
				CFRelease(errorDictionary), errorDictionary = NULL;				
			}
			
			if(!FLAC__stream_decoder_finish(mFLAC))
				LOG4CXX_WARN(logger, "FLAC__stream_decoder_finish failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));
			
			FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
			
			return false;
		}
	}
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'FLAC';
	
	mSourceFormat.mSampleRate			= mStreamInfo.sample_rate;
	mSourceFormat.mChannelsPerFrame		= mStreamInfo.channels;
	mSourceFormat.mBitsPerChannel		= mStreamInfo.bits_per_sample;
	
	mSourceFormat.mFramesPerPacket		= mStreamInfo.max_blocksize;
	
	switch(mStreamInfo.channels) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 3:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_3_0_A);		break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_A);		break;
		case 6:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);		break;
	}
	
	// Allocate the buffer list (which will convert from FLAC's push model to Core Audio's pull model)
	mBufferList = AllocateABL(mFormat, mStreamInfo.max_blocksize);
	
	if(NULL == mBufferList) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
		LOG4CXX_ERROR(logger, "Unable to allocate memory")

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);

		if(!FLAC__stream_decoder_finish(mFLAC))
			LOG4CXX_WARN(logger, "FLAC__stream_decoder_finish failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));
		
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
		
		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mIsOpen = true;
	return true;
}

bool FLACDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
		LOG4CXX_WARN(logger, "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(mFLAC) {
		if(!FLAC__stream_decoder_finish(mFLAC)) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
			LOG4CXX_WARN(logger, "FLAC__stream_decoder_finish failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));
		}
		
		FLAC__stream_decoder_delete(mFLAC), mFLAC = NULL;
	}

	if(mBufferList)
		mBufferList = DeallocateABL(mBufferList);

	mIsOpen = false;
	return true;
}

CFStringRef FLACDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return NULL;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("FLAC, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 FLACDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

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
	if(!IsOpen() || NULL == bufferList || bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	UInt32 framesRead = 0;
	
	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;
	
	for(;;) {
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / mFormat.mBytesPerFrame);
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / mFormat.mBytesPerFrame);
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);
		
		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			unsigned char *pullBuffer = static_cast<unsigned char *>(bufferList->mBuffers[i].mData);
			memcpy(pullBuffer + (framesToSkip * mFormat.mBytesPerFrame), mBufferList->mBuffers[i].mData, framesToCopy * mFormat.mBytesPerFrame);
			bufferList->mBuffers[i].mDataByteSize += static_cast<UInt32>(framesToCopy * mFormat.mBytesPerFrame);
			
			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				pullBuffer = static_cast<unsigned char *>(mBufferList->mBuffers[i].mData);
				memmove(pullBuffer, pullBuffer + (framesToCopy * mFormat.mBytesPerFrame), (framesInBuffer - framesToCopy) * mFormat.mBytesPerFrame);
			}
			
			mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToCopy * mFormat.mBytesPerFrame);
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
		if(!result) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
			LOG4CXX_WARN(logger, "FLAC__stream_decoder_process_single failed: " << FLAC__stream_decoder_get_resolved_state_string(mFLAC));
		}
	}
	
	mCurrentFrame += framesRead;
	
	return framesRead;
}

#pragma mark Callbacks

FLAC__StreamDecoderWriteStatus FLACDecoder::Write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	assert(IsOpen());
	assert(NULL != decoder);
	assert(NULL != frame);

	// Avoid segfaults
	if(NULL == mBufferList || mBufferList->mNumberBuffers != frame->header.channels)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	// FLAC hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
	UInt32 shift = (kAudioFormatFlagIsPacked & mFormat.mFormatFlags) ? 0 : (8 * mFormat.mBytesPerFrame) - mFormat.mBitsPerChannel;

	// Convert to native endian samples, high-aligned if necessary
	switch(mFormat.mBytesPerFrame) {
		case 1:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				char *pullBuffer = static_cast<char *>(mBufferList->mBuffers[channel].mData);

				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = static_cast<char>(buffer[channel][sample] << shift);
				
				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * sizeof(char));
			}

			break;
		}

		case 2:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				short *pullBuffer = static_cast<short *>(mBufferList->mBuffers[channel].mData);
				
				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = static_cast<short>(buffer[channel][sample] << shift);
				
				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * sizeof(short));
			}
			
			break;
		}

		case 3:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				unsigned char *pullBuffer = static_cast<unsigned char *>(mBufferList->mBuffers[channel].mData);

				FLAC__int32 value;
				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample) {
					value = buffer[channel][sample] << shift;
#if __BIG_ENDIAN__
					*pullBuffer++ = static_cast<unsigned char>((value >> 16) & 0xff);
					*pullBuffer++ = static_cast<unsigned char>((value >> 8) & 0xff);
					*pullBuffer++ = static_cast<unsigned char>(value & 0xff);
#elif __LITTLE_ENDIAN__
					*pullBuffer++ = static_cast<unsigned char>(value & 0xff);
					*pullBuffer++ = static_cast<unsigned char>((value >> 8) & 0xff);
					*pullBuffer++ = static_cast<unsigned char>((value >> 16) & 0xff);
#else
#  error Unknown OS byte order
#endif
				}

				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * 3 * sizeof(unsigned char));
			}

			break;
		}

		case 4:
		{
			for(unsigned channel = 0; channel < frame->header.channels; ++channel) {
				int *pullBuffer = static_cast<int *>(mBufferList->mBuffers[channel].mData);
				
				for(unsigned sample = 0; sample < frame->header.blocksize; ++sample)
					*pullBuffer++ = static_cast<int>(buffer[channel][sample] << shift);
				
				mBufferList->mBuffers[channel].mNumberChannels		= 1;
				mBufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(frame->header.blocksize * sizeof(int));
			}
			
			break;
		}
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
	
	log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioDecoder.FLAC");
	LOG4CXX_WARN(logger, "FLAC error: " << FLAC__StreamDecoderErrorStatusString[status]);
}
