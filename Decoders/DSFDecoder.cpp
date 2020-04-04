/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <CoreFoundation/CoreFoundation.h>

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "DSFDecoder.h"
#include "SFBCStringForOSType.h"

namespace {

	void RegisterDSFDecoder() __attribute__ ((constructor));
	void RegisterDSFDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::DSFDecoder>();
	}

	// Read a four byte chunk ID as a uint32_t
	bool ReadChunkID(SFB::InputSource& inputSource, uint32_t& chunkID)
	{
		char chunkIDBytes [4];
		auto bytesRead = inputSource.Read(chunkIDBytes, 4);
		if(4 != bytesRead) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read chunk ID");
			return false;
		}

		chunkID = (uint32_t)((chunkIDBytes[0] << 24u) | (chunkIDBytes[1] << 16u) | (chunkIDBytes[2] << 8u) | chunkIDBytes[3]);
		return true;
	}

	CFErrorRef CreateInvalidDSFFileError(CFURLRef url) CF_RETURNS_RETAINED
	{
		SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSF file."), ""));
		SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSF file"), ""));
		SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

		return CreateErrorForURL(SFB::Audio::Decoder::ErrorDomain, SFB::Audio::Decoder::InputOutputError, description, url, failureReason, recoverySuggestion);
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::DSFDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("dsf") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::DSFDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/dsf") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::DSFDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("dsf"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::DSFDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/dsf"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DSFDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new DSFDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::DSFDecoder::DSFDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mTotalFrames(-1), mCurrentFrame(0), mBlockByteSizePerChannel(0)
{}

SFB::Audio::DSFDecoder::~DSFDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::DSFDecoder::_Open(CFErrorRef *error)
{
#pragma unused(error)

	// Read the 'DSD ' chunk
	uint32_t chunkID;
	if(!ReadChunkID(GetInputSource(), chunkID) || 'DSD ' != chunkID) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	uint64_t chunkSize, fileSize, metadataOffset;
	// Unlike normal IFF, the chunkSize includes the size of the chunk ID and size
	if(!GetInputSource().ReadLE<uint64_t>(chunkSize) || 28 != chunkSize) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected 'DSD ' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint64_t>(fileSize)) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read file size in 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint64_t>(metadataOffset)) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read metadata offset in 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}


	// Read the 'fmt ' chunk
	if(!ReadChunkID(GetInputSource(), chunkID) || 'fmt ' != chunkID) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read 'fmt ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint64_t>(chunkSize)) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected 'fmt ' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	uint32_t formatVersion, formatID, channelType, channelNum, samplingFrequency, bitsPerSample;
	uint64_t sampleCount;
	uint32_t blockSizePerChannel, reserved;

	if(!GetInputSource().ReadLE<uint32_t>(formatVersion) || 1 != formatVersion) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected format version in 'fmt ': %u", formatVersion);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(formatID) || 0 != formatID) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected format ID in 'fmt ': %{public}.4s", SFBCStringForOSType(formatID));
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(channelType) || (1 > channelType || 7 < channelType)) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected channel type in 'fmt ': %u", channelType);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(channelNum) || (1 > channelNum || 6 < channelNum)) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected channel count in 'fmt ': %u", channelNum);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(samplingFrequency) || (2822400 != samplingFrequency && 5644800 != samplingFrequency)) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected sample rate in 'fmt ': %u", samplingFrequency);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(bitsPerSample) || (1 != bitsPerSample && 8 != bitsPerSample)) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected bits per sample in 'fmt ': %u", bitsPerSample);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint64_t>(sampleCount) ) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read sample count in 'fmt ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(blockSizePerChannel) || 4096 != blockSizePerChannel) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected block size per channel in 'fmt ': %u", blockSizePerChannel);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint32_t>(reserved) || 0 != reserved) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected non-zero value for reserved in 'fmt ': %u", reserved);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	// Read the 'data' chunk
	if(!ReadChunkID(GetInputSource(), chunkID) || 'data' != chunkID) {
		os_log_error(OS_LOG_DEFAULT, "Unable to read 'data' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	if(!GetInputSource().ReadLE<uint64_t>(chunkSize) ) {
		os_log_error(OS_LOG_DEFAULT, "Unexpected 'data' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(mInputSource->GetURL());
		return false;
	}

	mBlockByteSizePerChannel = blockSizePerChannel;

	mAudioOffset = GetInputSource().GetOffset();
	mTotalFrames = (SInt64)sampleCount;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatDirectStreamDigital;
	mSourceFormat.mSampleRate			= (Float64)samplingFrequency;
	mSourceFormat.mChannelsPerFrame		= (UInt32)channelNum;

	// The output format is raw DSD
	mFormat.mFormatID			= kAudioFormatDirectStreamDigital;
	mFormat.mFormatFlags		= kAudioFormatFlagIsNonInterleaved | (8 == bitsPerSample ? kAudioFormatFlagIsBigEndian : 0);

	mFormat.mSampleRate			= (Float64)samplingFrequency;
	mFormat.mChannelsPerFrame	= (UInt32)channelNum;
	mFormat.mBitsPerChannel		= 1;

	mFormat.mBytesPerPacket		= 1;
	mFormat.mFramesPerPacket	= 8;
	mFormat.mBytesPerFrame		= 0;

	mFormat.mReserved			= 0;

	// Channel layouts are defined in the DSF file format specification
	switch(channelType) {
		case 1:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);		break;
		case 3:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_3_0_A);	break;
		case 4:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 5:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_ITU_2_2);		break;
		case 6:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_A);	break;
		case 7:		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);	break;
	}

	// Metadata chunk is ignored

	// Allocate buffers
	mBufferList.Allocate(mFormat, (UInt32)mFormat.ByteCountToFrameCount(mBlockByteSizePerChannel));
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return true;
}

bool SFB::Audio::DSFDecoder::_Close(CFErrorRef */*error*/)
{
	return true;
}

SFB::CFString SFB::Audio::DSFDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("DSD Stream File, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::DSFDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// Only multiples of 8 frames can be read (8 frames equals one byte)
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 != frameCount % 8) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 fileFramesRemaining = (UInt32)(mTotalFrames - mCurrentFrame);
	UInt32 framesToRead = std::min(frameCount, fileFramesRemaining);
	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		UInt32	framesRemaining	= framesToRead - framesRead;
		UInt32	framesToSkip	= (UInt32)mFormat.ByteCountToFrameCount(bufferList->mBuffers[0].mDataByteSize);
		UInt32	framesInBuffer	= (UInt32)mFormat.ByteCountToFrameCount(mBufferList->mBuffers[0].mDataByteSize);
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);

		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			uint8_t *dst = (uint8_t *)bufferList->mBuffers[i].mData;
			memcpy(dst + mFormat.FrameCountToByteCount(framesToSkip), mBufferList->mBuffers[i].mData, mFormat.FrameCountToByteCount(framesToCopy));
			bufferList->mBuffers[i].mDataByteSize += mFormat.FrameCountToByteCount(framesToCopy);

			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				dst = (uint8_t *)mBufferList->mBuffers[i].mData;
				memmove(dst, dst + mFormat.FrameCountToByteCount(framesToCopy), mFormat.FrameCountToByteCount(framesInBuffer - framesToCopy));
			}

			mBufferList->mBuffers[i].mDataByteSize -= (UInt32)mFormat.FrameCountToByteCount(framesToCopy);
		}

		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == framesToRead)
			break;

		// Read and deinterleave the next block
		if(!ReadAndDeinterleaveDSDBlock())
			break;
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::DSFDecoder::_SeekToFrame(SInt64 frame)
{
	// Round down to nearest multiple of 8 frames
	frame = (frame / 8) * 8;

	// Seek to the start of the block containing frame
	auto blockSizePerChannelInFrames = mFormat.ByteCountToFrameCount(mBlockByteSizePerChannel);
	auto blockNumber = (size_t)frame / blockSizePerChannelInFrames;
	auto blockOffset = blockNumber * mBlockByteSizePerChannel * mFormat.mChannelsPerFrame;

	if(!GetInputSource().SeekToOffset(mAudioOffset + (SInt64)blockOffset)) {
		os_log_debug(OS_LOG_DEFAULT, "_SeekToFrame() failed for offset: %lld", mAudioOffset + (SInt64)blockOffset);
		return -1;
	}

	if(!ReadAndDeinterleaveDSDBlock())
		return -1;

	// Skip to the specified frame
	UInt32	framesToSkip	= (UInt32)frame % blockSizePerChannelInFrames;
	UInt32	framesInBuffer	= (UInt32)mFormat.ByteCountToFrameCount(mBufferList->mBuffers[0].mDataByteSize);

	// Copy data from the buffer to output
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		uint8_t *dst = (uint8_t *)mBufferList->mBuffers[i].mData;
		memmove(dst, dst + mFormat.FrameCountToByteCount(framesToSkip), mFormat.FrameCountToByteCount(framesInBuffer - framesToSkip));
		mBufferList->mBuffers[i].mDataByteSize -= (UInt32)mFormat.FrameCountToByteCount(framesToSkip);
	}

	mCurrentFrame = frame;

	return _GetCurrentFrame();
}

// Read interleaved input, grouped as 8 one bit samples per frame (a single channel byte) into
// a clustered frame of the specified blocksize (4096 bytes per channel for DSF version 1)
bool SFB::Audio::DSFDecoder::ReadAndDeinterleaveDSDBlock()
{
	auto bufsize = mFormat.mChannelsPerFrame * mBlockByteSizePerChannel;
	uint8_t buf [bufsize];

	auto bytesRead = GetInputSource().Read(buf, bufsize);
	if(bytesRead != bufsize) {
		os_log_debug(OS_LOG_DEFAULT, "Error reading audio block: requested %d bytes, got %lld", bufsize, bytesRead);
		return false;
	}

	auto bytesReadPerChannel = bytesRead / mFormat.mChannelsPerFrame;

	// Deinterleave the clustered frames and copy to the internal buffer
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		memcpy(mBufferList->mBuffers[i].mData, buf + (bytesReadPerChannel * i), (size_t)bytesReadPerChannel);

		mBufferList->mBuffers[i].mNumberChannels	= 1;
		mBufferList->mBuffers[i].mDataByteSize		= (UInt32)bytesReadPerChannel;
	}

	return true;
}
