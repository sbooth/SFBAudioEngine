//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <exception>

#import "SFBAudioPlayerNode+Internal.h"

const NSTimeInterval 		SFBUnknownTime 					= -1;
const SFBPlaybackPosition 	SFBInvalidPlaybackPosition 		= { .framePosition =  SFBUnknownFramePosition, .frameLength = SFBUnknownFrameLength};
const SFBPlaybackTime 		SFBInvalidPlaybackTime 			= { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime};
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

	std::unique_ptr<SFB::AudioPlayerNode> impl;

	try {
		impl = std::make_unique<SFB::AudioPlayerNode>(format, ringBufferSize);
	}
	catch(const std::exception& e) {
		os_log_error(SFB::AudioPlayerNode::sLog, "Unable to create std::unique_ptr<AudioPlayerNode>: %{public}s", e.what());
		return nil;
	}

	if((self = [super initWithFormat:format renderBlock:impl->mRenderBlock])) {
		_impl = std::move(impl);
		_impl->mNode = self;
	}

	return self;
}

- (void)dealloc
{
	_impl.reset();
}

#pragma mark - Format Information

- (AVAudioFormat *)renderingFormat
{
	return _impl->RenderingFormat();
}

- (BOOL)supportsFormat:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return _impl->SupportsFormat(format);
}

#pragma mark - Queue Management

- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _impl->EnqueueDecoder(decoder, true, error);
}

- (BOOL)resetAndEnqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _impl->EnqueueDecoder(decoder, true, error);
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _impl->EnqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _impl->EnqueueDecoder(decoder, false, error);
}

- (id<SFBPCMDecoding>)dequeueDecoder
{
	return _impl->DequeueDecoder();
}

- (void)clearQueue
{
	_impl->ClearQueue();
}

- (BOOL)queueIsEmpty
{
	return _impl->QueueIsEmpty();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _impl->CurrentDecoder();
}

- (void)cancelCurrentDecoder
{
	_impl->CancelActiveDecoders(false);
}

- (void)cancelActiveDecoders
{
	_impl->CancelActiveDecoders(true);
}

- (BOOL)cancelActiveDecoder:(id<SFBPCMDecoding>)decoder
{
	NSParameterAssert(decoder != nil);
	return _impl->CancelActiveDecoder(decoder);
}

// AVAudioNode override
- (void)reset
{
	[super reset];
	_impl->Reset();
}

#pragma mark - Playback Control

- (void)play
{
	_impl->Play();
}

- (void)pause
{
	_impl->Pause();
}

- (void)stop
{
	_impl->Stop();
	// Stop() calls Reset() internally so there is no need for [self reset]
	[super reset];
}

- (void)togglePlayPause
{
	_impl->TogglePlayPause();
}

#pragma mark - State

- (BOOL)isPlaying
{
	return _impl->IsPlaying();
}

- (BOOL)isReady
{
	return _impl->IsReady();
}

#pragma mark - Playback Properties

- (SFBPlaybackPosition)playbackPosition
{
	return _impl->PlaybackPosition();
}

- (SFBPlaybackTime)playbackTime
{
	return _impl->PlaybackTime();
}

- (BOOL)getPlaybackPosition:(SFBPlaybackPosition *)playbackPosition andTime:(SFBPlaybackTime *)playbackTime
{
	return _impl->GetPlaybackPositionAndTime(playbackPosition, playbackTime);
}

#pragma mark - Seeking

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return _impl->SeekForward(secondsToSkip);
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return _impl->SeekBackward(secondsToSkip);
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return _impl->SeekToTime(timeInSeconds);
}

- (BOOL)seekToPosition:(double)position
{
	return _impl->SeekToPosition(position);
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return _impl->SeekToFrame(frame);
}

- (BOOL)supportsSeeking
{
	return _impl->SupportsSeeking();
}

@end
