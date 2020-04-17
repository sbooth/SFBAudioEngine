/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#include <tta++/libtta.h>

#import "SFBTrueAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"

namespace {

	struct TTACallbacks: TTA_io_callback
	{
		SFBAudioDecoder *mDecoder;
	};

	TTAint32 read_callback(struct _tag_TTA_io_callback *io, TTAuint8 *buffer, TTAuint32 size)
	{
		TTACallbacks *iocb = (TTACallbacks *)io;

		NSInteger bytesRead;
		if(![iocb->mDecoder->_inputSource readBytes:buffer length:size bytesRead:&bytesRead error:nil])
			return -1;
		return (TTAint32)bytesRead;
	}

	TTAint64 seek_callback(struct _tag_TTA_io_callback *io, TTAint64 offset)
	{
		TTACallbacks *iocb = (TTACallbacks *)io;

		if(![iocb->mDecoder->_inputSource seekToOffset:offset error:nil])
			return -1;
		return offset;
	}

}

@interface SFBTrueAudioDecoder ()
{
@private
	std::unique_ptr<tta::tta_decoder> _decoder;
	std::unique_ptr<TTACallbacks> _callbacks;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
	TTAuint32 _framesToSkip;
}
@end

@implementation SFBTrueAudioDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"tta"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-tta"];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	_callbacks				= std::make_unique<TTACallbacks>();
	_callbacks->read		= read_callback;
	_callbacks->write		= nullptr;
	_callbacks->seek		= seek_callback;
	_callbacks->mDecoder	= self;

	TTA_info streamInfo;

	try {
		_decoder = std::make_unique<tta::tta_decoder>((TTA_io_callback *)_callbacks.get());
		_decoder->init_get_info(&streamInfo, 0);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "Error creating True Audio decoder: %d", e.code());
		return NO;
	}

	if(!_decoder) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid True Audio file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a True Audio file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	AVAudioChannelLayout *channelLayout = nil;
	switch(streamInfo.nch) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
	}

	AudioStreamBasicDescription processingStreamDescription;

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	processingStreamDescription.mSampleRate			= streamInfo.sps;
	processingStreamDescription.mChannelsPerFrame	= streamInfo.nch;
	processingStreamDescription.mBitsPerChannel		= streamInfo.bps;

	processingStreamDescription.mBytesPerPacket		= ((streamInfo.bps + 7) / 8) * processingStreamDescription.mChannelsPerFrame;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

	processingStreamDescription.mReserved			= 0;

	// Support 4 to 32 bits per sample (True Audio may support more or less, but the documentation didn't say)
	switch(streamInfo.bps) {
		case 8:
		case 16:
		case 24:
		case 32:
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsPacked;
			break;

		case 4 ... 7:
		case 9 ... 15:
		case 17 ... 23:
		case 25 ... 31:
			// Align high because Apple's AudioConverter doesn't handle low alignment
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
			break;

		default:
		{
			os_log_error(OS_LOG_DEFAULT, "Unsupported bit depth: %d", streamInfo.bps);

			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported True Audio file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Bit depth not supported", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];

			return NO;
		}
	}

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	_frameLength = streamInfo.samples;

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDTrueAudio;

	sourceStreamDescription.mSampleRate			= streamInfo.sps;
	sourceStreamDescription.mChannelsPerFrame	= streamInfo.nch;
	sourceStreamDescription.mBitsPerChannel		= streamInfo.bps;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_decoder.reset();
	_callbacks.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _decoder != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(OS_LOG_DEFAULT, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRead = 0;
	bool eos = false;

	try {
		while(_framesToSkip && !eos) {
			if(_framesToSkip >= frameLength) {
				framesRead = (AVAudioFrameCount)_decoder->process_stream((TTAuint8 *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
				_framesToSkip -= framesRead;
			}
			else {
				framesRead = (AVAudioFrameCount)_decoder->process_stream((TTAuint8 *)buffer.audioBufferList->mBuffers[0].mData, _framesToSkip);
				_framesToSkip = 0;
			}

			if(framesRead == 0)
				eos = true;
		}

		if(!eos) {
			framesRead = (UInt32)_decoder->process_stream((TTAuint8 *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			if(framesRead == 0)
				eos = true;
		}
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "True Audio decoding error: %d", e.code());
		return 0;
	}

	if(eos)
		return YES;

	buffer.frameLength = framesRead;
	_framePosition += framesRead;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	TTAuint32 seconds = (TTAuint32)(frame / _processingFormat.sampleRate);
	TTAuint32 frame_start = 0;

	try {
		_decoder->set_position(seconds, &frame_start);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(OS_LOG_DEFAULT, "True Audio seek error: %d", e.code());
		return NO;
	}

	_framePosition = frame;

	// We need to skip some samples from start of the frame if required
	_framesToSkip = UInt32((seconds - frame_start) * _processingFormat.sampleRate + 0.5);

	return YES;
}

@end
