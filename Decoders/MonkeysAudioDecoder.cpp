/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <AudioToolbox/AudioFormat.h>

#define PLATFORM_APPLE

#include <MAC/All.h>
#include <MAC/IO.h>
#include <MAC/MACLib.h>

#undef PLATFORM_APPLE

#include "CFErrorUtilities.h"
#include "MonkeysAudioDecoder.h"

namespace {

	void RegisterMonkeysAudioDecoder() __attribute__ ((constructor));
	void RegisterMonkeysAudioDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MonkeysAudioDecoder>();
	}

}

#pragma mark IO Interface

// The I/O interface for MAC
class SFB::Audio::MonkeysAudioDecoder::APEIOInterface : public APE::CIO
{
public:
	explicit APEIOInterface(SFB::InputSource& inputSource)
		: mInputSource(inputSource)
	{}

	inline virtual int Open(const wchar_t * pName, bool bOpenReadOnly)
	{
#pragma unused(pName)
#pragma unused(bOpenReadOnly)

		return ERROR_INVALID_INPUT_FILE;
	}

	inline virtual int Close()
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

	inline virtual int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten)
	{
#pragma unused(pBuffer)
#pragma unused(nBytesToWrite)
#pragma unused(pBytesWritten)

		return ERROR_IO_WRITE;
	}

	virtual APE::int64 PerformSeek()
	{
		if(!mInputSource.SupportsSeeking())
			return ERROR_IO_READ;

		SInt64 offset = m_nSeekPosition;
		switch(m_nSeekMethod) {
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

	inline virtual int Create(const wchar_t * pName)
	{
#pragma unused(pName)
		return ERROR_IO_WRITE;
	}

	inline virtual int Delete()
	{
		return ERROR_IO_WRITE;
	}

	inline virtual int SetEOF()
	{
		return ERROR_IO_WRITE;
	}

	inline virtual APE::int64 GetPosition()
	{
		return mInputSource.GetOffset();
	}

	inline virtual APE::int64 GetSize()
	{
		return mInputSource.GetLength();
	}

	inline virtual int GetName(wchar_t * pBuffer)
	{
#pragma unused(pBuffer)
		return ERROR_SUCCESS;
	}

private:

	SFB::InputSource& mInputSource;
};

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
	: Decoder(std::move(inputSource))
{}

#pragma mark Functionality

bool SFB::Audio::MonkeysAudioDecoder::_Open(CFErrorRef *error)
{
	auto ioInterface = 	std::unique_ptr<APEIOInterface>(new APEIOInterface(GetInputSource()));

	auto decompressor = std::unique_ptr<APE::IAPEDecompress>(CreateIAPEDecompressEx(ioInterface.get(), nullptr));
	if(!decompressor) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	mDecompressor = std::move(decompressor);
	mIOInterface = std::move(ioInterface);

	// The file format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;

	mFormat.mBitsPerChannel		= (UInt32)mDecompressor->GetInfo(APE::APE_INFO_BITS_PER_SAMPLE);
	mFormat.mSampleRate			= mDecompressor->GetInfo(APE::APE_INFO_SAMPLE_RATE);
	mFormat.mChannelsPerFrame	= (UInt32)mDecompressor->GetInfo(APE::APE_INFO_CHANNELS);

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMonkeysAudio;

	mSourceFormat.mSampleRate			= mFormat.mSampleRate;
	mSourceFormat.mChannelsPerFrame		= mFormat.mChannelsPerFrame;
	mSourceFormat.mBitsPerChannel		= mFormat.mBitsPerChannel;

	switch(mFormat.mChannelsPerFrame) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
	}

	return true;
}

bool SFB::Audio::MonkeysAudioDecoder::_Close(CFErrorRef */*error*/)
{
	mIOInterface.reset();
	mDecompressor.reset();

	return true;
}

SFB::CFString SFB::Audio::MonkeysAudioDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("Monkey's Audio, %u channels, %u Hz"),
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MonkeysAudioDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	int64_t blocksRead = 0;
	if(ERROR_SUCCESS != mDecompressor->GetData((char *)bufferList->mBuffers[0].mData, (int64_t)frameCount, &blocksRead)) {
		os_log_error(OS_LOG_DEFAULT, "Monkey's Audio invalid checksum");
		return 0;
	}

	bufferList->mBuffers[0].mDataByteSize = (UInt32)blocksRead * mFormat.mBytesPerFrame;
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)blocksRead;
}

SInt64 SFB::Audio::MonkeysAudioDecoder::_GetTotalFrames() const
{
	return mDecompressor->GetInfo(APE::APE_DECOMPRESS_TOTAL_BLOCKS);
}

SInt64 SFB::Audio::MonkeysAudioDecoder::_GetCurrentFrame() const
{
	return mDecompressor->GetInfo(APE::APE_DECOMPRESS_CURRENT_BLOCK);
}

SInt64 SFB::Audio::MonkeysAudioDecoder::_SeekToFrame(SInt64 frame)
{
	if(ERROR_SUCCESS != mDecompressor->Seek(frame)) {
		os_log_error(OS_LOG_DEFAULT, "mDecompressor->Seek() failed");
		return -1;
	}

	return this->GetCurrentFrame();
}
