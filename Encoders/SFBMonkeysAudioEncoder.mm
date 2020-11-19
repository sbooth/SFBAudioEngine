/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#define PLATFORM_APPLE

#include <MAC/All.h>
#include <MAC/IO.h>
#include <MAC/MACLib.h>

#undef PLATFORM_APPLE

#import "SFBMonkeysAudioEncoder.h"

namespace {

	// The I/O interface for MAC
	class APEIOInterface : public APE::CIO
	{
	public:
		explicit APEIOInterface(SFBOutputSource *outputSource)
			: mOutputSource(outputSource)
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
			NSInteger bytesRead;
			if(![mOutputSource readBytes:pBuffer length:nBytesToRead bytesRead:&bytesRead error:nil])
				return ERROR_IO_READ;

			*pBytesRead = (unsigned int)bytesRead;

			return ERROR_SUCCESS;
		}

		inline virtual int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten)
		{
			NSInteger bytesWritten;
			if(![mOutputSource writeBytes:pBuffer length:(NSInteger)nBytesToWrite bytesWritten:&bytesWritten error:nil] || bytesWritten != nBytesToWrite)
				return ERROR_IO_WRITE;

			*pBytesWritten = (unsigned int)bytesWritten;

			return ERR_SUCCESS;
		}

		virtual APE::int64 PerformSeek()
		{
			if(!mOutputSource.supportsSeeking)
				return ERROR_IO_READ;

			NSInteger offset = m_nSeekPosition;
			switch(m_nSeekMethod) {
				case SEEK_SET:
					// offset remains unchanged
					break;
				case SEEK_CUR: {
					NSInteger inputSourceOffset;
					if([mOutputSource getOffset:&inputSourceOffset error:nil])
						offset += inputSourceOffset;
					break;
				}
				case SEEK_END: {
					NSInteger inputSourceLength;
					if([mOutputSource getLength:&inputSourceLength error:nil])
						offset += inputSourceLength;
					break;
				}
			}

			return ![mOutputSource seekToOffset:offset error:nil];
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
			NSInteger offset;
			if(![mOutputSource getOffset:&offset error:nil])
				return -1;
			return offset;
		}

		inline virtual APE::int64 GetSize()
		{
			NSInteger length;
			if(![mOutputSource getLength:&length error:nil])
				return -1;
			return length;
		}

		inline virtual int GetName(wchar_t * pBuffer)
		{
#pragma unused(pBuffer)
			return ERROR_SUCCESS;
		}

	private:

		SFBOutputSource *mOutputSource;
	};

}

@interface SFBMonkeysAudioEncoder ()
{
@private
	std::unique_ptr<APEIOInterface> _ioInterface;
	std::unique_ptr<APE::IAPECompress> _compressor;
	AVAudioFramePosition _framePosition;
}
@end

@implementation SFBMonkeysAudioEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"ape"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/monkeys-audio", @"audio/x-monkeys-audio"]];
}

- (BOOL)encodingIsLossless
{
	return YES;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.streamDescription->mFormatFlags & kAudioFormatFlagIsFloat || sourceFormat.channelCount < 1 || sourceFormat.channelCount > 32)
		return nil;

	APE::WAVEFORMATEX wve;
	auto result = FillWaveFormatEx(&wve, WAVE_FORMAT_PCM, (int)sourceFormat.sampleRate, (int)sourceFormat.streamDescription->mBitsPerChannel, (int)sourceFormat.channelCount);
	if(result != ERROR_SUCCESS) {
		os_log_error(gSFBAudioEncoderLog, "FillWaveFormatEx() failed: %d", result);
		return nil;
	}

	// Set up the processing format
	AudioStreamBasicDescription streamDescription{};

	streamDescription.mFormatID				= kAudioFormatLinearPCM;
	streamDescription.mFormatFlags			= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

	streamDescription.mSampleRate			= wve.nSamplesPerSec;
	streamDescription.mChannelsPerFrame		= wve.nChannels;
	streamDescription.mBitsPerChannel		= wve.wBitsPerSample;

	streamDescription.mBytesPerPacket		= ((wve.wBitsPerSample + 7) / 8) * streamDescription.mChannelsPerFrame;
	streamDescription.mFramesPerPacket		= 1;
	streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket * streamDescription.mFramesPerPacket;

	// FIXME: Use WAVEFORMATEX channel ordering
	AVAudioChannelLayout *channelLayout = [[AVAudioChannelLayout alloc] initWithLayoutTag:(kAudioChannelLayoutTag_DiscreteInOrder | sourceFormat.channelCount)];
	return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	int result;
	auto compressor = CreateIAPECompress(&result);
	if(!compressor) {
		os_log_error(gSFBAudioEncoderLog, "CreateIAPECompress() failed: %d", result);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	_compressor = std::unique_ptr<APE::IAPECompress>(compressor);
	_ioInterface = std::make_unique<APEIOInterface>(_outputSource);

	int compressionLevel = MAC_COMPRESSION_LEVEL_NORMAL;
	NSNumber *level = [_settings objectForKey:SFBAudioEncodingSettingsKeyAPECompressionLevel];
	if(level) {
		auto intValue = level.intValue;
		switch(intValue) {
			case SFBAudioEncoderAPECompressionLevelFast:
			case SFBAudioEncoderAPECompressionLevelNormal:
			case SFBAudioEncoderAPECompressionLevelHigh:
			case SFBAudioEncoderAPECompressionLevelExtraHigh:
			case SFBAudioEncoderAPECompressionLevelInsane:
				compressionLevel = intValue;
				break;
			default:
				os_log_info(gSFBAudioEncoderLog, "Invalid APE compression level: %d", intValue);
				break;
		}
	}

	APE::WAVEFORMATEX wve;
	result = FillWaveFormatEx(&wve, WAVE_FORMAT_PCM, (int)_sourceFormat.sampleRate, (int)_sourceFormat.streamDescription->mBitsPerChannel, (int)_sourceFormat.channelCount);
	if(result != ERROR_SUCCESS) {
		os_log_error(gSFBAudioEncoderLog, "FillWaveFormatEx() failed: %d", result);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	result = _compressor->StartEx(_ioInterface.get(), &wve, MAX_AUDIO_BYTES_UNKNOWN, compressionLevel);
	if(result != ERROR_SUCCESS) {
		os_log_error(gSFBAudioEncoderLog, "_compressor->StartEx() failed: %d", result);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_ioInterface.reset();
	_compressor.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _compressor != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioEncoderLog, "-encodeFromBuffer:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	auto bytesToWrite = frameLength * _processingFormat.streamDescription->mBytesPerFrame;
	auto result = _compressor->AddData((unsigned char *)buffer.audioBufferList->mBuffers[0].mData, bytesToWrite);
	if(result != ERROR_SUCCESS) {
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	_framePosition += frameLength;

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	auto result = _compressor->Finish(nullptr, 0, 0);
	if(result != ERROR_SUCCESS) {
		os_log_error(gSFBAudioEncoderLog, "_compressor->Finish() failed: %d", result);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}
	return YES;
}

@end
