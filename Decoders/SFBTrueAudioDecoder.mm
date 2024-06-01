//
// Copyright (c) 2011 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <memory>

#import <os/log.h>

#import <tta-cpp/libtta.h>

#import "SFBTrueAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameTrueAudio = @"org.sbooth.AudioEngine.Decoder.TrueAudio";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioFormat = @"format";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioNumberChannels = @"nch";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioBitsPerSample = @"bps";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSampleRate = @"sps";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSamples = @"samples";

namespace {

struct TTACallbacks : TTA_io_callback
{
	SFBAudioDecoder *mDecoder;
};

TTAint32 read_callback(struct _tag_TTA_io_callback *io, TTAuint8 *buffer, TTAuint32 size)
{
	TTACallbacks *iocb = static_cast<TTACallbacks *>(io);

	NSInteger bytesRead;
	if(![iocb->mDecoder->_inputSource readBytes:buffer length:size bytesRead:&bytesRead error:nil])
		return -1;
	return (TTAint32)bytesRead;
}

TTAint64 seek_callback(struct _tag_TTA_io_callback *io, TTAint64 offset)
{
	TTACallbacks *iocb = static_cast<TTACallbacks *>(io);

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

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameTrueAudio;
}

- (BOOL)decodingIsLossless
{
	return YES;
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
		_decoder = std::make_unique<tta::tta_decoder>(static_cast<TTA_io_callback *>(_callbacks.get()));
		_decoder->init_get_info(&streamInfo, 0);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioDecoderLog, "Error creating True Audio decoder: %d", e.code());
		return NO;
	}

	if(!_decoder) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
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
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | streamInfo.nch)];
			break;
	}

	AudioStreamBasicDescription processingStreamDescription{};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	processingStreamDescription.mSampleRate			= streamInfo.sps;
	processingStreamDescription.mChannelsPerFrame	= streamInfo.nch;
	processingStreamDescription.mBitsPerChannel		= streamInfo.bps;

	processingStreamDescription.mBytesPerPacket		= ((streamInfo.bps + 7) / 8) * processingStreamDescription.mChannelsPerFrame;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

	processingStreamDescription.mReserved			= 0;

	// True Audio supports 16 to 24 bits per sample
	switch(streamInfo.bps) {
		case 16:
		case 24:
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsPacked;
			break;

		case 17 ... 23:
			// Align high because Apple's AudioConverter doesn't handle low alignment
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
			break;

		default:
		{
			os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %d", streamInfo.bps);

			_decoder.reset();

			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
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

	sourceStreamDescription.mFormatID			= kSFBAudioFormatTrueAudio;

	sourceStreamDescription.mSampleRate			= streamInfo.sps;
	sourceStreamDescription.mChannelsPerFrame	= streamInfo.nch;
	sourceStreamDescription.mBitsPerChannel		= streamInfo.bps;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	// Populate codec properties
	_properties = @{
		SFBAudioDecodingPropertiesKeyTrueAudioFormat: @(streamInfo.format),
		SFBAudioDecodingPropertiesKeyTrueAudioNumberChannels: @(streamInfo.nch),
		SFBAudioDecodingPropertiesKeyTrueAudioBitsPerSample: @(streamInfo.bps),
		SFBAudioDecodingPropertiesKeyTrueAudioSampleRate: @(streamInfo.sps),
		SFBAudioDecodingPropertiesKeyTrueAudioSamples: @(streamInfo.samples),
	};

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
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	try {
		while(_framesToSkip > 0) {
			auto framesToSkip = std::min(_framesToSkip, frameLength);
			auto bytesToSkip = framesToSkip * _processingFormat.streamDescription->mBytesPerFrame;
			auto framesSkipped = _decoder->process_stream(static_cast<TTAuint8 *>(buffer.audioBufferList->mBuffers[0].mData), bytesToSkip);

			// EOS reached finishing seek
			if(framesSkipped == 0)
				return YES;

			_framesToSkip -= static_cast<TTAuint32>(framesSkipped);
		}

		auto bytesToRead = frameLength * _processingFormat.streamDescription->mBytesPerFrame;
		auto framesRead = static_cast<AVAudioFrameCount>(_decoder->process_stream(static_cast<TTAuint8 *>(buffer.audioBufferList->mBuffers[0].mData), bytesToRead));

		// EOS
		if(framesRead == 0)
			return YES;

		buffer.frameLength = framesRead;
		_framePosition += framesRead;

		return YES;
	}
	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioDecoderLog, "True Audio decoding error: %d", e.code());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	TTAuint32 seconds = static_cast<TTAuint32>(frame / _processingFormat.sampleRate);
	TTAuint32 frame_start = 0;

	try {
		_decoder->set_position(seconds, &frame_start);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioDecoderLog, "True Audio seek error: %d", e.code());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}

	_framePosition = frame;

	// We need to skip some samples from start of the frame if required
	_framesToSkip = static_cast<UInt32>((seconds - frame_start) * _processingFormat.sampleRate + 0.5);

	return YES;
}

@end
