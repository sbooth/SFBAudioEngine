//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <exception>

#import "SFBAudioPlayerNode+Internal.h"

NSErrorDomain const 		SFBAudioPlayerNodeErrorDomain 	= @"org.sbooth.AudioEngine.AudioPlayerNode";

namespace {

/// The default ring buffer capacity in frames
constexpr AVAudioFrameCount kDefaultRingBufferFrameCapacity = 16384;

} /* namespace */

@implementation SFBAudioPlayerNode

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioPlayerNodeErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBAudioPlayerNodeErrorCodeInternalError:
					return NSLocalizedString(@"An internal player error occurred.", @"");
				case SFBAudioPlayerNodeErrorCodeFormatNotSupported:
					return NSLocalizedString(@"The format is invalid, unknown, or unsupported.", @"");
			}
		}
		return nil;
	}];
}

- (instancetype)init
{
	return [self initWithSampleRate:44100 channels:2];
}

- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels
{
	return [self initWithFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:sampleRate channels:channels]];
}

- (instancetype)initWithFormat:(AVAudioFormat *)format
{
	return [self initWithFormat:format ringBufferSize:kDefaultRingBufferFrameCapacity];
}

- (instancetype)initWithFormat:(AVAudioFormat *)format ringBufferSize:(uint32_t)ringBufferSize
{
	NSParameterAssert(format != nil);
	NSParameterAssert(format.isStandard);

	std::unique_ptr<SFB::AudioPlayerNode> node;

	try {
		node = std::make_unique<SFB::AudioPlayerNode>(format, ringBufferSize);
	}
	catch(const std::exception& e) {
		os_log_error(SFB::AudioPlayerNode::sLog, "Unable to create std::unique_ptr<AudioPlayerNode>: %{public}s", e.what());
		return nil;
	}

	if((self = [super initWithFormat:format renderBlock:node->mRenderBlock])) {
		_node = std::move(node);
		_node->mNode = self;
	}

	return self;
}

- (void)dealloc
{
	_node.reset();
}

// MARK: - Format Information

- (AVAudioFormat *)renderingFormat
{
	return _node->RenderingFormat();
}

- (BOOL)supportsFormat:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return _node->SupportsFormat(format);
}

// MARK: - Queue Management

- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _node->EnqueueDecoder(decoder, true, error);
}

- (BOOL)resetAndEnqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _node->EnqueueDecoder(decoder, true, error);
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _node->EnqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _node->EnqueueDecoder(decoder, false, error);
}

- (id<SFBPCMDecoding>)dequeueDecoder
{
	return _node->DequeueDecoder();
}

- (BOOL)removeDecoderFromQueue:(id<SFBPCMDecoding>)decoder
{
	NSParameterAssert(decoder != nil);
	return _node->RemoveDecoderFromQueue(decoder);
}

- (void)clearQueue
{
	_node->ClearQueue();
}

- (BOOL)queueIsEmpty
{
	return _node->QueueIsEmpty();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _node->CurrentDecoder();
}

- (void)cancelCurrentDecoder
{
	_node->CancelActiveDecoders(false);
}

- (void)cancelActiveDecoders
{
	_node->CancelActiveDecoders(true);
}

// AVAudioNode override
- (void)reset
{
	[super reset];
	_node->Reset();
}

// MARK: - Playback Control

- (void)play
{
	_node->Play();
}

- (void)pause
{
	_node->Pause();
}

- (void)stop
{
	_node->Stop();
	// Stop() calls Reset() internally so there is no need for [self reset]
	[super reset];
}

- (void)togglePlayPause
{
	_node->TogglePlayPause();
}

// MARK: - Playback State

- (BOOL)isPlaying
{
	return _node->IsPlaying();
}

- (BOOL)isReady
{
	return _node->IsReady();
}

// MARK: - Playback Properties

- (SFBPlaybackPosition)playbackPosition
{
	return _node->PlaybackPosition();
}

- (SFBPlaybackTime)playbackTime
{
	return _node->PlaybackTime();
}

- (BOOL)getPlaybackPosition:(SFBPlaybackPosition *)playbackPosition andTime:(SFBPlaybackTime *)playbackTime
{
	return _node->GetPlaybackPositionAndTime(playbackPosition, playbackTime);
}

// MARK: - Seeking

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return _node->SeekForward(secondsToSkip);
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return _node->SeekBackward(secondsToSkip);
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return _node->SeekToTime(timeInSeconds);
}

- (BOOL)seekToPosition:(double)position
{
	return _node->SeekToPosition(position);
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return _node->SeekToFrame(frame);
}

- (BOOL)supportsSeeking
{
	return _node->SupportsSeeking();
}

// MARK: - Event Notification

- (SFBAudioPlayerNodeDecodingStartedBlock)decodingStartedBlock
{
	return _node->mDecodingStartedBlock;
}
- (void)setDecodingStartedBlock:(SFBAudioPlayerNodeDecodingStartedBlock)decodingStartedBlock
{
	_node->mDecodingStartedBlock = decodingStartedBlock;
}

- (SFBAudioPlayerNodeDecodingCompleteBlock)decodingCompleteBlock
{
	return _node->mDecodingCompleteBlock;
}
- (void)setDecodingCompleteBlock:(SFBAudioPlayerNodeDecodingCompleteBlock)decodingCompleteBlock
{
	_node->mDecodingCompleteBlock = decodingCompleteBlock;
}

- (SFBAudioPlayerNodeRenderingWillStartBlock)renderingWillStartBlock
{
	return _node->mRenderingWillStartBlock;
}
- (void)setRenderingWillStartBlock:(SFBAudioPlayerNodeRenderingWillStartBlock)renderingWillStartBlock
{
	_node->mRenderingWillStartBlock = renderingWillStartBlock;
}

- (SFBAudioPlayerNodeRenderingDecoderWillChangeBlock)renderingDecoderWillChangeBlock
{
	return _node->mRenderingDecoderWillChangeBlock;
}
- (void)setRenderingDecoderWillChangeBlock:(SFBAudioPlayerNodeRenderingDecoderWillChangeBlock)renderingDecoderWillChangeBlock
{
	_node->mRenderingDecoderWillChangeBlock = renderingDecoderWillChangeBlock;
}

- (SFBAudioPlayerNodeRenderingWillCompleteBlock)renderingWillCompleteBlock
{
	return _node->mRenderingWillCompleteBlock;
}
- (void)setRenderingWillCompleteBlock:(SFBAudioPlayerNodeRenderingWillCompleteBlock)renderingWillCompleteBlock
{
	_node->mRenderingWillCompleteBlock = renderingWillCompleteBlock;
}

- (SFBAudioPlayerNodeDecoderCanceledBlock)decoderCanceledBlock
{
	return _node->mDecoderCanceledBlock;
}
- (void)setDecoderCanceledBlock:(SFBAudioPlayerNodeDecoderCanceledBlock)decoderCanceledBlock
{
	_node->mDecoderCanceledBlock = decoderCanceledBlock;
}

- (SFBAudioPlayerNodeAsynchronousErrorBlock)asynchronousErrorBlock
{
	return _node->mAsynchronousErrorBlock;
}
- (void)setAsynchronousErrorBlock:(SFBAudioPlayerNodeAsynchronousErrorBlock)asynchronousErrorBlock
{
	_node->mAsynchronousErrorBlock = asynchronousErrorBlock;
}

@end
