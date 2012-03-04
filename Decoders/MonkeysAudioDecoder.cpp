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

#include <AudioToolbox/AudioFormat.h>
#include <stdexcept>

#include "MonkeysAudioDecoder.h"
#include "CFErrorUtilities.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/IO.h>

#pragma mark IO Interface

// ========================================
// The I/O interface for MAC
// ========================================
class APEIOInterface : public CIO
{
public:
    APEIOInterface(InputSource *inputSource)
		: mInputSource(inputSource)
	{}

    virtual ~APEIOInterface()
	{
		mInputSource = nullptr;
	};
	
    virtual int Open(const wchar_t * pName)
	{
#pragma unused(pName)
		return ERROR_INVALID_INPUT_FILE;
	}
    
	virtual int Close()
	{
		return ERROR_SUCCESS;
	}
    
    virtual int Read(void * pBuffer, unsigned int nBytesToRead, unsigned int * pBytesRead)
	{
		SInt64 bytesRead = mInputSource->Read(pBuffer, nBytesToRead);
		if(-1 == bytesRead)
			return ERROR_IO_READ;

		*pBytesRead = static_cast<unsigned int>(bytesRead);

		return ERROR_SUCCESS;
	}

    virtual int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten)
	{
#pragma unused(pBuffer)
#pragma unused(nBytesToWrite)
#pragma unused(pBytesWritten)
		return ERROR_IO_WRITE;
	}
    
    virtual int Seek(int nDistance, unsigned int nMoveMode)
	{
		if(!mInputSource->SupportsSeeking())
			return ERROR_IO_READ;
		
		SInt64 offset = nDistance;
		switch(nMoveMode) {
			case SEEK_SET:
				// offset remains unchanged
				break;
			case SEEK_CUR:
				offset += mInputSource->GetOffset();
				break;
			case SEEK_END:
				offset += mInputSource->GetLength();
				break;
		}
		
		return (!mInputSource->SeekToOffset(offset));
	}
    
    virtual int Create(const wchar_t * pName)
	{
#pragma unused(pName)
		return ERROR_IO_WRITE;
	}

    virtual int Delete()
	{
		return ERROR_IO_WRITE;
	}
	
    virtual int SetEOF()
	{
		return ERROR_IO_WRITE;
	}
	
    virtual int GetPosition()
	{
		SInt64 offset = mInputSource->GetOffset();
		return static_cast<int>(offset);
	}

    virtual int GetSize()
	{
		SInt64 length = mInputSource->GetLength();
		return static_cast<int>(length);
	}

    virtual int GetName(wchar_t * pBuffer)
	{
#pragma unused(pBuffer)
		return ERROR_SUCCESS;
	}

private:
	InputSource *mInputSource;
};

#pragma mark Static Methods

CFArrayRef MonkeysAudioDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ape") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MonkeysAudioDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/monkeys-audio") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MonkeysAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ape"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MonkeysAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/monkeys-audio"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MonkeysAudioDecoder::MonkeysAudioDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mDecompressor(nullptr)
{}

MonkeysAudioDecoder::~MonkeysAudioDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool MonkeysAudioDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MonkeysAudio", "Open() called on an AudioDecoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	mIOInterface = new APEIOInterface(GetInputSource());

	int errorCode;
	mDecompressor = CreateIAPEDecompressEx(mIOInterface, &errorCode);
	
	if(nullptr == mDecompressor) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// The file format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	
	mFormat.mBitsPerChannel		= static_cast<UInt32>(mDecompressor->GetInfo(APE_INFO_BITS_PER_SAMPLE));
	mFormat.mSampleRate			= mDecompressor->GetInfo(APE_INFO_SAMPLE_RATE);
	mFormat.mChannelsPerFrame	= static_cast<UInt32>(mDecompressor->GetInfo(APE_INFO_CHANNELS));
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// Set up the source format
	mSourceFormat.mFormatID				= 'APE ';
	
	mSourceFormat.mSampleRate			= mFormat.mSampleRate;
	mSourceFormat.mChannelsPerFrame		= mFormat.mChannelsPerFrame;
	
	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 4:		mChannelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}

	mIsOpen = true;
	return true;
}

bool MonkeysAudioDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MonkeysAudio", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	if(mIOInterface)
		delete mIOInterface, mIOInterface = nullptr;

	if(mDecompressor)
		delete mDecompressor, mDecompressor = nullptr;

	mIsOpen = false;
	return true;
}

CFStringRef MonkeysAudioDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("Monkey's Audio, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MonkeysAudioDecoder::GetTotalFrames() const
{
	if(!IsOpen())
		return -1;

	return mDecompressor->GetInfo(APE_DECOMPRESS_TOTAL_BLOCKS);
}

SInt64 MonkeysAudioDecoder::GetCurrentFrame() const
{
	if(!IsOpen())
		return -1;

	return mDecompressor->GetInfo(APE_DECOMPRESS_CURRENT_BLOCK);
}

SInt64 MonkeysAudioDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	if(ERROR_SUCCESS != mDecompressor->Seek(static_cast<int>(frame)))
		return -1;

	return this->GetCurrentFrame();
}

UInt32 MonkeysAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	int blocksRead = 0;
	if(ERROR_SUCCESS != mDecompressor->GetData(reinterpret_cast<char *>(bufferList->mBuffers[0].mData), frameCount, &blocksRead)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.MonkeysAudio", "Monkey's Audio invalid checksum");
		return 0;
	}

	return blocksRead;
}
