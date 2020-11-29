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

#import "SFBCStringForOSType.h"

SFBAudioEncoderName const SFBAudioEncoderNameMonkeysAudio = @"org.sbooth.AudioEngine.Encoder.MonkeysAudio";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel = @"Compression Level";

SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelFast = @"Fast";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelNormal = @"Normal";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelHigh = @"High";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelExtraHigh = @"Extra High";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelInsane = @"Insane";

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

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameMonkeysAudio;
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
	streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

	// Use WAVFORMATEX channel order
	AVAudioChannelLayout *channelLayout = nil;

	UInt32 channelBitmap = 0;
	UInt32 propertySize = sizeof(channelBitmap);
	AudioChannelLayoutTag layoutTag = sourceFormat.channelLayout.layoutTag;
	result = AudioFormatGetProperty(kAudioFormatProperty_BitmapForLayoutTag, sizeof(layoutTag), &layoutTag, &propertySize, &channelBitmap);
	if(result == noErr) {
		AudioChannelLayout acl = {
			.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
			.mChannelBitmap = channelBitmap,
			.mNumberChannelDescriptions = 0
		};
		channelLayout = [[AVAudioChannelLayout alloc] initWithLayout:&acl];
	}
	else
		os_log_info(gSFBAudioEncoderLog, "AudioFormatGetProperty(kAudioFormatProperty_BitmapForLayoutTag), layoutTag = %d failed: %d '%{public}.4s'", layoutTag, result, SFBCStringForOSType(result));

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
	SFBAudioEncodingSettingsValue level = [_settings objectForKey:SFBAudioEncodingSettingsKeyAPECompressionLevel];
	if(level != nil) {
		if(level == SFBAudioEncodingSettingsValueAPECompressionLevelFast)				compressionLevel = MAC_COMPRESSION_LEVEL_FAST;
		else if(level == SFBAudioEncodingSettingsValueAPECompressionLevelNormal)		compressionLevel = MAC_COMPRESSION_LEVEL_NORMAL;
		else if(level == SFBAudioEncodingSettingsValueAPECompressionLevelHigh)			compressionLevel = MAC_COMPRESSION_LEVEL_HIGH;
		else if(level == SFBAudioEncodingSettingsValueAPECompressionLevelExtraHigh)		compressionLevel = MAC_COMPRESSION_LEVEL_EXTRA_HIGH;
		else if(level == SFBAudioEncodingSettingsValueAPECompressionLevelInsane)		compressionLevel = MAC_COMPRESSION_LEVEL_INSANE;
		else
			os_log_info(gSFBAudioEncoderLog, "Ignoring unknown APE compression level: %{public}@", level);
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

	AudioStreamBasicDescription outputStreamDescription{};
	outputStreamDescription.mFormatID			= SFBAudioFormatIDMonkeysAudio;
	outputStreamDescription.mBitsPerChannel		= wve.wBitsPerSample;
	outputStreamDescription.mSampleRate			= wve.nSamplesPerSec;
	outputStreamDescription.mChannelsPerFrame	= wve.nChannels;
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

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
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
		return NO;
	}

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	auto bytesToWrite = frameLength * _processingFormat.streamDescription->mBytesPerFrame;
	auto result = _compressor->AddData((unsigned char *)buffer.audioBufferList->mBuffers[0].mData, bytesToWrite);
	if(result != ERROR_SUCCESS) {
		os_log_error(gSFBAudioEncoderLog, "_compressor->AddData() failed: %lld", result);
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
