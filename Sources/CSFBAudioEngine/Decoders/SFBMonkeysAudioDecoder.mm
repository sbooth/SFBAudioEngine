//
// Copyright (c) 2011-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import <os/log.h>

#define PLATFORM_APPLE

#include <MAC/All.h>
#include <MAC/IO.h>
#include <MAC/MACLib.h>

#undef PLATFORM_APPLE

#import "SFBMonkeysAudioDecoder.h"

#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameMonkeysAudio = @"org.sbooth.AudioEngine.Decoder.MonkeysAudio";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion = @"APE_INFO_FILE_VERSION";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel = @"APE_INFO_COMPRESSION_LEVEL";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags = @"APE_INFO_FORMAT_FLAGS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate = @"APE_INFO_SAMPLE_RATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample = @"APE_INFO_BITS_PER_SAMPLE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample = @"APE_INFO_BYTES_PER_SAMPLE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioChannels = @"APE_INFO_CHANNELS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment = @"APE_INFO_BLOCK_ALIGN";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame = @"APE_INFO_BLOCKS_PER_FRAME";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks = @"APE_INFO_FINAL_FRAME_BLOCKS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames = @"APE_INFO_TOTAL_FRAMES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes = @"APE_INFO_WAV_HEADER_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes = @"APE_INFO_WAV_TERMINATING_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes = @"APE_INFO_WAV_DATA_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes = @"APE_INFO_WAV_TOTAL_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes = @"APE_INFO_APE_TOTAL_BYTES";
//SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks = @"APE_INFO_TOTAL_BLOCKS";
//SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds = @"APE_INFO_LENGTH_MS";
//SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate = @"APE_INFO_AVERAGE_BITRATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate = @"APE_INFO_DECOMPRESSED_BITRATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPL = @"APE_INFO_APL";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks = @"APE_DECOMPRESS_TOTAL_BLOCKS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds = @"APE_DECOMPRESS_LENGTH_MS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate = @"APE_DECOMPRESS_AVERAGE_BITRATE";

namespace {

// The I/O interface for MAC
class APEIOInterface : public APE::CIO
{
public:
	explicit APEIOInterface(SFBInputSource *inputSource)
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
		NSInteger bytesRead;
		if(![mInputSource readBytes:pBuffer length:nBytesToRead bytesRead:&bytesRead error:nil])
			return ERROR_IO_READ;

		*pBytesRead = static_cast<unsigned int>(bytesRead);

		return ERROR_SUCCESS;
	}

	inline virtual int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten)
	{
#pragma unused(pBuffer)
#pragma unused(nBytesToWrite)
#pragma unused(pBytesWritten)

		return ERROR_IO_WRITE;
	}

	virtual int Seek(APE::int64 nPosition, APE::SeekMethod nMethod)
	{
		if(!mInputSource.supportsSeeking)
			return ERROR_IO_READ;

		NSInteger offset = nPosition;
		switch(nMethod) {
			case APE::SeekFileBegin:
				// offset remains unchanged
				break;
			case APE::SeekFileCurrent: {
				NSInteger inputSourceOffset;
				if([mInputSource getOffset:&inputSourceOffset error:nil])
					offset += inputSourceOffset;
				break;
			}
			case APE::SeekFileEnd: {
				NSInteger inputSourceLength;
				if([mInputSource getLength:&inputSourceLength error:nil])
					offset += inputSourceLength;
				break;
			}
		}

		return ![mInputSource seekToOffset:offset error:nil];
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

	inline virtual int SetReadWholeFile()
	{
		return ERROR_IO_READ;
	}

	inline virtual void SetReadToBuffer()
	{
	}

	inline virtual unsigned char * GetBuffer(int * pnBufferBytes)
	{
#pragma unused(pnBufferBytes)
		return nullptr;
	}

	inline virtual APE::int64 GetPosition()
	{
		NSInteger offset;
		if(![mInputSource getOffset:&offset error:nil])
			return -1;
		return offset;
	}

	inline virtual APE::int64 GetSize()
	{
		NSInteger length;
		if(![mInputSource getLength:&length error:nil])
			return -1;
		return length;
	}

	inline virtual int GetName(wchar_t * pBuffer)
	{
#pragma unused(pBuffer)
		return ERROR_SUCCESS;
	}

private:

	SFBInputSource *mInputSource;
};

}

@interface SFBMonkeysAudioDecoder ()
{
@private
	std::unique_ptr<APEIOInterface> _ioInterface;
	std::unique_ptr<APE::IAPEDecompress> _decompressor;
}
@end

@implementation SFBMonkeysAudioDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"ape"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/monkeys-audio", @"audio/x-monkeys-audio"]];
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameMonkeysAudio;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [inputSource readHeaderOfLength:4 skipID3v2Tag:YES error:error];
	if(!header)
		return NO;

	if([header isAPEHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)decodingIsLossless
{
	return YES;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	auto ioInterface = 	std::make_unique<APEIOInterface>(_inputSource);
	auto decompressor = std::unique_ptr<APE::IAPEDecompress>(CreateIAPEDecompressEx(ioInterface.get(), nullptr));
	if(!decompressor) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Monkey's Audio file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a Monkey's Audio file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	_decompressor = std::move(decompressor);
	_ioInterface = std::move(ioInterface);

	AVAudioChannelLayout *channelLayout = nil;
	switch(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS)) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS)))];
			break;
	}

	// The file format
	AudioStreamBasicDescription processingStreamDescription{};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;

	processingStreamDescription.mBitsPerChannel		= static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE));
	processingStreamDescription.mSampleRate			= _decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE);
	processingStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS));

	processingStreamDescription.mBytesPerPacket		= (processingStreamDescription.mBitsPerChannel / 8) * processingStreamDescription.mChannelsPerFrame;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

	processingStreamDescription.mReserved			= 0;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= kSFBAudioFormatMonkeysAudio;

	sourceStreamDescription.mBitsPerChannel		= static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE));
	sourceStreamDescription.mSampleRate			= _decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE);
	sourceStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS));

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	// Populate codec properties
	_properties = @{
		SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FILE_VERSION)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_COMPRESSION_LEVEL)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FORMAT_FLAGS)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BYTES_PER_SAMPLE)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioChannels: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BLOCK_ALIGN)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BLOCKS_PER_FRAME)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FINAL_FRAME_BLOCKS)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_TOTAL_FRAMES)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_HEADER_BYTES)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_TERMINATING_BYTES)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_DATA_BYTES)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_TOTAL_BYTES)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_APE_TOTAL_BYTES)),
//		SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_TOTAL_BLOCKS)),
//		SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_LENGTH_MS)),
//		SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_AVERAGE_BITRATE)),
		// APE_INFO_FRAME_BITRATE
		SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_DECOMPRESSED_BITRATE)),
		// APE_INFO_PEAK_LEVEL
		// APE_INFO_SEEK_BIT
		// APE_INFO_SEEK_BYTE
		// APE_INFO_WAV_HEADER_DATA
		// APE_INFO_WAV_TERMINATING_DATA
		// APE_INFO_WAVEFORMATEX
		// APE_INFO_IO_SOURCE
		// APE_INFO_FRAME_BYTES
		// APE_INFO_FRAME_BLOCKS
		// APE_INFO_TAG
		SFBAudioDecodingPropertiesKeyMonkeysAudioAPL: _decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_APL) ? @YES : @NO,

		// APE_DECOMPRESS_CURRENT_BLOCK
		// APE_DECOMPRESS_CURRENT_MS
		SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_TOTAL_BLOCKS)),
		SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_LENGTH_MS)),
		// APE_DECOMPRESS_CURRENT_BITRATE
		SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate: @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_AVERAGE_BITRATE)),
		// APE_DECOMPRESS_CURRENT_FRAME
	};

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_ioInterface.reset();
	_decompressor.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _decompressor != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_CURRENT_BLOCK);
}

- (AVAudioFramePosition)frameLength
{
	return _decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_TOTAL_BLOCKS);
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	int64_t blocksRead = 0;
	if(_decompressor->GetData(static_cast<unsigned char *>(buffer.audioBufferList->mBuffers[0].mData), static_cast<int64_t>(frameLength), &blocksRead)) {
		os_log_error(gSFBAudioDecoderLog, "Monkey's Audio invalid checksum");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}

	buffer.frameLength = static_cast<AVAudioFrameCount>(blocksRead);

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	return _decompressor->Seek(frame) == ERROR_SUCCESS;
}

@end
