/*
 *  Copyright (C) 2011 Stephen F. Booth <me@sbooth.org>
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

#include "TrueAudioDecoder.h"
#include "CreateChannelLayout.h"
#include "CreateDisplayNameForURL.h"
#include "Logger.h"

#define BUFFER_SIZE_FRAMES 2048

#pragma mark Callbacks

typedef struct {
	TTA_io_callback iocb;
	TrueAudioDecoder *decoder;
} TTA_io_callback_wrapper;

static TTAint32 read_callback(struct _tag_TTA_io_callback *io, TTAuint8 *buffer, TTAuint32 size)
{
	TTA_io_callback_wrapper *iocb = (TTA_io_callback_wrapper *)io;
	return (TTAint32)iocb->decoder->GetInputSource()->Read(buffer, size);
}

static TTAint64 seek_callback(struct _tag_TTA_io_callback *io, TTAint64 offset)
{
	TTA_io_callback_wrapper *iocb = (TTA_io_callback_wrapper *)io;
	return iocb->decoder->GetInputSource()->SeekToOffset(offset);
}

#pragma mark Static Methods

CFArrayRef TrueAudioDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("tta") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef TrueAudioDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/x-tta") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool TrueAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(NULL == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("tta"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool TrueAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-tta"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

TrueAudioDecoder::TrueAudioDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), /*mDecoder(NULL), */mCurrentFrame(0), seek_skip(0)
{}

TrueAudioDecoder::~TrueAudioDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool TrueAudioDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "Open() called on an AudioDecoder that is already open");		
		return true;
	}
	
	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	TTA_io_callback_wrapper callbacks = {
		.iocb.read	= read_callback,
		.iocb.write	= NULL,
		.iocb.seek	= seek_callback,
		.decoder	= this
	};

//	try {
//		mDecoder = new tta::tta_decoder((TTA_io_callback *)&callbacks);
//		mDecoder->init_get_info(&mStreamInfo, 0);
//	}
//	catch(tta::tta_exception e) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "Error creating True Audio decoder: " << e.code());
//		if(mDecoder)
//			delete mDecoder, mDecoder = NULL;
//	}
//
//	if(NULL == mDecoder) {
//		if(error) {
//			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
//																			   0,
//																			   &kCFTypeDictionaryKeyCallBacks,
//																			   &kCFTypeDictionaryValueCallBacks);
//
//			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
//			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
//															   NULL, 
//															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), ""), 
//															   displayName);
//
//			CFDictionarySetValue(errorDictionary, 
//								 kCFErrorLocalizedDescriptionKey, 
//								 errorString);
//
//			CFDictionarySetValue(errorDictionary, 
//								 kCFErrorLocalizedFailureReasonKey, 
//								 CFCopyLocalizedString(CFSTR("Not a True Audio file"), ""));
//
//			CFDictionarySetValue(errorDictionary, 
//								 kCFErrorLocalizedRecoverySuggestionKey, 
//								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
//
//			CFRelease(errorString), errorString = NULL;
//			CFRelease(displayName), displayName = NULL;
//
//			*error = CFErrorCreate(kCFAllocatorDefault, 
//								   AudioDecoderErrorDomain, 
//								   AudioDecoderInputOutputError, 
//								   errorDictionary);
//
//			CFRelease(errorDictionary), errorDictionary = NULL;
//		}
//
//		return false;
//	}

	tta_decoder_new((TTA_io_callback *)&callbacks);
	if(0 != tta_decoder_init_get_info(&mStreamInfo)) {
		tta_decoder_done();
		return false;
	}
	
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;
	
	mFormat.mSampleRate			= mStreamInfo.sps;
	mFormat.mChannelsPerFrame	= mStreamInfo.nch;
	mFormat.mBitsPerChannel		= mStreamInfo.bps;
	
	mFormat.mBytesPerPacket		= ((mStreamInfo.bps + 7) / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// Support 4 to 32 bits per sample (True Audio may support more or less, but the documentation didn't say)
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
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "Unsupported bit depth: " << mFormat.mBitsPerChannel)

			if(error) {
				CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																				   0,
																				   &kCFTypeDictionaryKeyCallBacks,
																				   &kCFTypeDictionaryValueCallBacks);

				CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
				CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																   NULL, 
																   CFCopyLocalizedString(CFSTR("The file “%@” is not a supported True Audio file."), ""), 
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

//			delete mDecoder, mDecoder = NULL;

			return false;
		}
	}

	// Set up the source format
	mSourceFormat.mFormatID				= 'TTA ';

	mSourceFormat.mSampleRate			= mStreamInfo.sps;
	mSourceFormat.mChannelsPerFrame		= mStreamInfo.nch;
	mSourceFormat.mBitsPerChannel		= mStreamInfo.bps;

	// Setup the channel layout
	mChannelLayout = CreateChannelLayoutWithTag(mStreamInfo.nch);

//	mBuffer = static_cast<TTAuint8 *>(calloc(BUFFER_SIZE_FRAMES * mFormat.mChannelsPerFrame * mFormat.mBytesPerFrame, sizeof(TTAuint8)));
//
//	if(NULL == mBuffer) {
//		if(error)
//			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
//
//		return false;		
//	}

	mIsOpen = true;
	return true;
}

bool TrueAudioDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}
	
//	if(mDecoder)
//		delete mDecoder, mDecoder = NULL;
//
//	if(mBuffer)
//		free(mBuffer), mBuffer = NULL;

	tta_decoder_done();

	memset(&mStreamInfo, 0, sizeof(mStreamInfo));

	mIsOpen = false;
	return true;
}

CFStringRef TrueAudioDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return NULL;
	
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("True Audio, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 TrueAudioDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	TTAuint32 seconds = static_cast<TTAuint32>(frame * mStreamInfo.sps);
	TTAuint32 frame_start = 0;

	if(!tta_decoder_set_position(seconds, &frame_start))
		return -1;
//	try {
//		mDecoder->set_position(seconds, &frame_start);
//	}
//	catch(tta::tta_exception e) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "True Audio seek error: " << e.code());
//		return -1;
//	}

	// We need to skip some samples from start of the frame if required
	seek_skip = UInt32((seconds - frame_start) * mStreamInfo.sps + 0.5);

	return mCurrentFrame;
}

UInt32 TrueAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || NULL == bufferList || bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame || 0 == frameCount)
		return 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	UInt32 framesRead = 0;
	bool eos = false;
	
//	try {
		while(seek_skip && !eos) {
			if(seek_skip >= frameCount) {
//				framesRead = mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount);
				framesRead = tta_decoder_process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount, NULL);
				seek_skip -= framesRead;
			}
			else {
//				framesRead = mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, seek_skip);
				framesRead = tta_decoder_process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, seek_skip, NULL);
				seek_skip = 0;
			}

			if(0 == framesRead)
				eos = true;
		}
		
		if(!eos) {
//			framesRead = mDecoder->process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount);
			framesRead = tta_decoder_process_stream((TTAuint8 *)bufferList->mBuffers[0].mData, frameCount, NULL);
			if(0 == framesRead)
				eos = true;
		}
//	}
//	catch(tta::tta_exception e) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.TrueAudio", "True Audio decoding error: " << e.code());
//		return -1;
//	}

	if(eos)
		return 0;

	bufferList->mBuffers[0].mDataByteSize = static_cast<UInt32>(framesRead * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	mCurrentFrame += framesRead;
	return framesRead;
}
