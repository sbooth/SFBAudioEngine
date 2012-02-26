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

#include "LibsndfileDecoder.h"
#include "CreateChannelLayout.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

#pragma mark Callbacks

static sf_count_t
my_sf_vio_get_filelen(void *user_data)
{
	assert(nullptr != user_data);

	LibsndfileDecoder *decoder = static_cast<LibsndfileDecoder *>(user_data);
	return decoder->GetInputSource()->GetLength();
}

static sf_count_t
my_sf_vio_seek(sf_count_t offset, int whence, void *user_data)
{
	assert(nullptr != user_data);
	
	LibsndfileDecoder *decoder = static_cast<LibsndfileDecoder *>(user_data);
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

	return inputSource->GetOffset();
}

static sf_count_t
my_sf_vio_read(void *ptr, sf_count_t count, void *user_data)
{
	assert(nullptr != user_data);

	LibsndfileDecoder *decoder = static_cast<LibsndfileDecoder *>(user_data);
	return decoder->GetInputSource()->Read(ptr, count);
}

static sf_count_t
my_sf_vio_tell(void *user_data)
{
	assert(nullptr != user_data);

	LibsndfileDecoder *decoder = static_cast<LibsndfileDecoder *>(user_data);
	return decoder->GetInputSource()->GetOffset();
}

#pragma mark Static Methods

CFArrayRef LibsndfileDecoder::CreateSupportedFileExtensions()
{
	int majorCount = 0;
	sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, majorCount, &kCFTypeArrayCallBacks);

	// Loop through each major mode
	for(int i = 0; i < majorCount; ++i) {	
		SF_FORMAT_INFO formatInfo;
		formatInfo.format = i;
		if(0 == sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
			CFStringRef extension = CFStringCreateWithCString(kCFAllocatorDefault, formatInfo.extension, kCFStringEncodingUTF8);
			if(extension) {
				CFArrayAppendValue(supportedExtensions, extension);
				CFRelease(extension), extension = nullptr;
			}
		}
		else
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "sf_command (SFC_GET_FORMAT_MAJOR) " << i << "failed");
	}

	return supportedExtensions;
}

CFArrayRef LibsndfileDecoder::CreateSupportedMIMETypes()
{
	return CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks);
}

bool LibsndfileDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	CFArrayRef supportedExtensions = CreateSupportedFileExtensions();

	if(nullptr == supportedExtensions)
		return false;
	
	bool extensionIsSupported = false;
	
	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedExtensions, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive)) {
			extensionIsSupported = true;
			break;
		}
	}
		
	CFRelease(supportedExtensions), supportedExtensions = nullptr;
	
	return extensionIsSupported;
}

bool LibsndfileDecoder::HandlesMIMEType(CFStringRef /*mimeType*/)
{
	return false;
}

#pragma mark Creation and Destruction

LibsndfileDecoder::LibsndfileDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mFile(nullptr), mReadMethod(eUnknown)
{
	memset(&mFileInfo, 0, sizeof(SF_INFO));
}

LibsndfileDecoder::~LibsndfileDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool LibsndfileDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	// Set up the virtual IO function pointers
	SF_VIRTUAL_IO virtualIO;
	virtualIO.get_filelen	= my_sf_vio_get_filelen;
	virtualIO.seek			= my_sf_vio_seek;
	virtualIO.read			= my_sf_vio_read;
	virtualIO.write			= nullptr;
	virtualIO.tell			= my_sf_vio_tell;

	// Open the input file
	mFile = sf_open_virtual(&virtualIO, SFM_READ, &mFileInfo, this);

	if(nullptr == mFile) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "sf_open_virtual failed: " << sf_error(nullptr));

		if(nullptr != error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	// Generate interleaved PCM output
	mFormat.mFormatID			= kAudioFormatLinearPCM;

	mFormat.mSampleRate			= mFileInfo.samplerate;
	mFormat.mChannelsPerFrame	= mFileInfo.channels;

	int subFormat = SF_FORMAT_SUBMASK & mFileInfo.format;

	// 8-bit PCM will be high-aligned in shorts
	if(SF_FORMAT_PCM_U8 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsAlignedHigh;

		mFormat.mBitsPerChannel		= 8;

		mFormat.mBytesPerPacket		= static_cast<UInt32>(sizeof(short) * mFormat.mChannelsPerFrame);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eShort;
	}
	else if(SF_FORMAT_PCM_S8 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;
		
		mFormat.mBitsPerChannel		= 8;
		
		mFormat.mBytesPerPacket		= static_cast<UInt32>(sizeof(short) * mFormat.mChannelsPerFrame);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eShort;
	}
	// 16-bit PCM
	else if(SF_FORMAT_PCM_16 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

		mFormat.mBitsPerChannel		= 16;

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eShort;
	}
	// 24-bit PCM will be high-aligned in ints
	else if(SF_FORMAT_PCM_24 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;

		mFormat.mBitsPerChannel		= 24;

		mFormat.mBytesPerPacket		= static_cast<UInt32>(sizeof(int) * mFormat.mChannelsPerFrame);
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eInt;
	}
	// 32-bit PCM
	else if(SF_FORMAT_PCM_32 == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

		mFormat.mBitsPerChannel		= 32;

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eInt;
	}
	// Floating point formats
	else if(SF_FORMAT_FLOAT == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(float);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eFloat;
	}
	else if(SF_FORMAT_DOUBLE == subFormat) {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(double);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eDouble;
	}
	// Everything else will be converted to 32-bit float
	else {
		mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;

		mFormat.mBitsPerChannel		= 8 * sizeof(float);

		mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
		mFormat.mFramesPerPacket	= 1;
		mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

		mReadMethod					= eFloat;
	}

	mFormat.mReserved			= 0;

	// Set up the channel layout
	switch(mFileInfo.channels) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 3:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_3_0_A);		break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_A);		break;
		case 6:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);		break;
	}

	// Set up the source format
	mSourceFormat.mFormatID				= 'SNDF';
	
	mSourceFormat.mSampleRate			= mFileInfo.samplerate;
	mSourceFormat.mChannelsPerFrame		= mFileInfo.channels;

	switch(subFormat) {
		case SF_FORMAT_PCM_U8:
			mSourceFormat.mBitsPerChannel = 8;
			break;

		case SF_FORMAT_PCM_S8:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 8;
			break;
		
		case SF_FORMAT_PCM_16:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 16;
			break;
		
		case SF_FORMAT_PCM_24:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 24;
			break;

		case SF_FORMAT_PCM_32:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			mSourceFormat.mBitsPerChannel = 32;
			break;
		
		case SF_FORMAT_FLOAT:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsFloat;
			mSourceFormat.mBitsPerChannel = 32;
			break;

		case SF_FORMAT_DOUBLE:
			mSourceFormat.mFormatFlags = kAudioFormatFlagIsFloat;
			mSourceFormat.mBitsPerChannel = 64;
			break;
	}

	mIsOpen = true;
	return true;
}

bool LibsndfileDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(mFile) {
		int result = sf_close(mFile);
		if(0 != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "sf_close failed: " << result);

		mFile = nullptr;
	}

	memset(&mFileInfo, 0, sizeof(SF_INFO));
	mReadMethod = eUnknown;

	mIsOpen = false;
	return true;
}

CFStringRef LibsndfileDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	SF_FORMAT_INFO formatInfo;
	formatInfo.format = mFileInfo.format;

	if(0 != sf_command(nullptr, SFC_GET_FORMAT_INFO, &formatInfo, sizeof(formatInfo))) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libsndfile", "sf_command (SFC_GET_FORMAT_INFO) failed");
		return nullptr;
	}
	
	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("%s, %u channels, %u Hz"), 
									formatInfo.name,
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 LibsndfileDecoder::GetTotalFrames() const
{
	if(!IsOpen())
		return -1;

	return mFileInfo.frames;
}

SInt64 LibsndfileDecoder::GetCurrentFrame() const
{
	if(!IsOpen())
		return -1;

	return sf_seek(mFile, 0, SEEK_CUR);
}

SInt64 LibsndfileDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	return sf_seek(mFile, frame, SEEK_SET);
}

UInt32 LibsndfileDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	sf_count_t framesRead = 0;
	switch(mReadMethod) {
		case eUnknown:	/* Do nothing */																						break;
		case eShort:	framesRead = sf_readf_short(mFile, static_cast<short *>(bufferList->mBuffers[0].mData), frameCount);	break;
		case eInt:		framesRead = sf_readf_int(mFile, static_cast<int *>(bufferList->mBuffers[0].mData), frameCount);		break;
		case eFloat:	framesRead = sf_readf_float(mFile, static_cast<float *>(bufferList->mBuffers[0].mData), frameCount);	break;
		case eDouble:	framesRead = sf_readf_double(mFile, static_cast<double *>(bufferList->mBuffers[0].mData), frameCount);	break;
	}

	bufferList->mBuffers[0].mDataByteSize = static_cast<UInt32>(framesRead * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return static_cast<UInt32>(framesRead);
}
