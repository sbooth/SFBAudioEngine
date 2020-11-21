/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#define PLATFORM_APPLE

#include <MAC/All.h>
#include <MAC/IO.h>
#include <MAC/MACLib.h>

#undef PLATFORM_APPLE

#import "SFBMonkeysAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"

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
			if(!mInputSource.supportsSeeking)
				return ERROR_IO_READ;

			NSInteger offset = m_nSeekPosition;
			switch(m_nSeekMethod) {
				case SEEK_SET:
					// offset remains unchanged
					break;
				case SEEK_CUR: {
					NSInteger inputSourceOffset;
					if([mInputSource getOffset:&inputSourceOffset error:nil])
						offset += inputSourceOffset;
					break;
				}
				case SEEK_END: {
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
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Monkey's Audio file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a Monkey's Audio file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	_decompressor = std::move(decompressor);
	_ioInterface = std::move(ioInterface);

	AVAudioChannelLayout *channelLayout = nil;
	switch(_decompressor->GetInfo(APE::APE_INFO_CHANNELS)) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | (UInt32)_decompressor->GetInfo(APE::APE_INFO_CHANNELS))];
			break;
	}

	// The file format
	AudioStreamBasicDescription processingStreamDescription{};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;

	processingStreamDescription.mBitsPerChannel		= (UInt32)_decompressor->GetInfo(APE::APE_INFO_BITS_PER_SAMPLE);
	processingStreamDescription.mSampleRate			= _decompressor->GetInfo(APE::APE_INFO_SAMPLE_RATE);
	processingStreamDescription.mChannelsPerFrame	= (UInt32)_decompressor->GetInfo(APE::APE_INFO_CHANNELS);

	processingStreamDescription.mBytesPerPacket		= (processingStreamDescription.mBitsPerChannel / 8) * processingStreamDescription.mChannelsPerFrame;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

	processingStreamDescription.mReserved			= 0;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDMonkeysAudio;

	sourceStreamDescription.mBitsPerChannel		= (UInt32)_decompressor->GetInfo(APE::APE_INFO_BITS_PER_SAMPLE);
	sourceStreamDescription.mSampleRate			= _decompressor->GetInfo(APE::APE_INFO_SAMPLE_RATE);
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)_decompressor->GetInfo(APE::APE_INFO_CHANNELS);

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

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
	return _decompressor->GetInfo(APE::APE_DECOMPRESS_CURRENT_BLOCK);
}

- (AVAudioFramePosition)frameLength
{
	return _decompressor->GetInfo(APE::APE_DECOMPRESS_TOTAL_BLOCKS);
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	int64_t blocksRead = 0;
	if(_decompressor->GetData((char *)buffer.audioBufferList->mBuffers[0].mData, (int64_t)frameLength, &blocksRead)) {
		os_log_error(gSFBAudioDecoderLog, "Monkey's Audio invalid checksum");
		return NO;
	}

	buffer.frameLength = (AVAudioFrameCount)blocksRead;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	return _decompressor->Seek(frame) == ERROR_SUCCESS;
}

@end
