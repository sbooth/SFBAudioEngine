/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include "MonkeysAudioDecoder.h"
#include "CreateChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/IO.h>

namespace {

	void RegisterMonkeysAudioDecoder() __attribute__ ((constructor));
	void RegisterMonkeysAudioDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MonkeysAudioDecoder>();
	}

#pragma mark IO Interface

	// ========================================
	// The I/O interface for MAC
	// ========================================
	class APEIOInterface : public CIO
	{
	public:
		APEIOInterface(SFB::InputSource& inputSource)
			: mInputSource(inputSource)
		{}

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
			SInt64 bytesRead = mInputSource.Read(pBuffer, nBytesToRead);
			if(-1 == bytesRead)
				return ERROR_IO_READ;

			*pBytesRead = (unsigned int)bytesRead;

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
			if(!mInputSource.SupportsSeeking())
				return ERROR_IO_READ;

			SInt64 offset = nDistance;
			switch(nMoveMode) {
				case SEEK_SET:
					// offset remains unchanged
					break;
				case SEEK_CUR:
					offset += mInputSource.GetOffset();
					break;
				case SEEK_END:
					offset += mInputSource.GetLength();
					break;
			}

			return (!mInputSource.SeekToOffset(offset));
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
			SInt64 offset = mInputSource.GetOffset();
			return (int)offset;
		}

		virtual int GetSize()
		{
			SInt64 length = mInputSource.GetLength();
			return (int)length;
		}

		virtual int GetName(wchar_t * pBuffer)
		{
#pragma unused(pBuffer)
			return ERROR_SUCCESS;
		}
		
	private:
		SFB::InputSource& mInputSource;
	};

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MonkeysAudioDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ape") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MonkeysAudioDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/monkeys-audio"), CFSTR("audio/x-monkeys-audio") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MonkeysAudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ape"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool SFB::Audio::MonkeysAudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/monkeys-audio"), kCFCompareCaseInsensitive))
		return true;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-monkeys-audio"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MonkeysAudioDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MonkeysAudioDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MonkeysAudioDecoder::MonkeysAudioDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mDecompressor(nullptr)
{}

SFB::Audio::MonkeysAudioDecoder::~MonkeysAudioDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::MonkeysAudioDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.MonkeysAudio", "Open() called on a Decoder that is already open");		
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	auto ioInterface = 	std::unique_ptr<APEIOInterface>(new APEIOInterface(GetInputSource()));

	auto decompressor = std::unique_ptr<IAPEDecompress>(CreateIAPEDecompressEx(ioInterface.get(), nullptr));
	if(!decompressor) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}
		
		return false;
	}

	mDecompressor = std::move(decompressor);
	mIOInterface = std::move(ioInterface);

	// The file format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	
	mFormat.mBitsPerChannel		= (UInt32)mDecompressor->GetInfo(APE_INFO_BITS_PER_SAMPLE);
	mFormat.mSampleRate			= mDecompressor->GetInfo(APE_INFO_SAMPLE_RATE);
	mFormat.mChannelsPerFrame	= (UInt32)mDecompressor->GetInfo(APE_INFO_CHANNELS);
	
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

bool SFB::Audio::MonkeysAudioDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.MonkeysAudio", "Close() called on a Decoder that hasn't been opened");
		return true;
	}

	mIOInterface.reset();
	mDecompressor.reset();

	mIsOpen = false;
	return true;
}

CFStringRef SFB::Audio::MonkeysAudioDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									nullptr, 
									CFSTR("Monkey's Audio, %u channels, %u Hz"), 
									mSourceFormat.mChannelsPerFrame, 
									(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 SFB::Audio::MonkeysAudioDecoder::GetTotalFrames() const
{
	if(!IsOpen())
		return -1;

	return mDecompressor->GetInfo(APE_DECOMPRESS_TOTAL_BLOCKS);
}

SInt64 SFB::Audio::MonkeysAudioDecoder::GetCurrentFrame() const
{
	if(!IsOpen())
		return -1;

	return mDecompressor->GetInfo(APE_DECOMPRESS_CURRENT_BLOCK);
}

SInt64 SFB::Audio::MonkeysAudioDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	if(ERROR_SUCCESS != mDecompressor->Seek((int)frame))
		return -1;

	return this->GetCurrentFrame();
}

UInt32 SFB::Audio::MonkeysAudioDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	int blocksRead = 0;
	if(ERROR_SUCCESS != mDecompressor->GetData((char *)bufferList->mBuffers[0].mData, (int)frameCount, &blocksRead)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.MonkeysAudio", "Monkey's Audio invalid checksum");
		return 0;
	}

	bufferList->mBuffers[0].mDataByteSize = (UInt32)blocksRead * mFormat.mBytesPerFrame;
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)blocksRead;
}
