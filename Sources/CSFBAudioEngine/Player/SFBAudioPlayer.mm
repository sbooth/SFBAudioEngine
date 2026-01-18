//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <exception>

#import "SFBAudioPlayer+Internal.h"

NSErrorDomain const SFBAudioPlayerErrorDomain = @"org.sbooth.AudioEngine.AudioPlayer";

@implementation SFBAudioPlayer

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioPlayerErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		switch(err.code) {
			case SFBAudioPlayerErrorCodeInternalError:
				if([userInfoKey isEqualToString:NSLocalizedFailureReasonErrorKey])
					return NSLocalizedString(@"Internal player error", @"");
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"An internal audio player error occurred.", @"");
				break;

			case SFBAudioPlayerErrorCodeFormatNotSupported:
				if([userInfoKey isEqualToString:NSLocalizedFailureReasonErrorKey])
					return NSLocalizedString(@"Unsupported format", @"");
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"The format is invalid, unknown, or unsupported.", @"");
				break;
		}

		return nil;
	}];
}

- (instancetype)init
{
	std::unique_ptr<sfb::AudioPlayer> player;

	try {
		player = std::make_unique<sfb::AudioPlayer>();
	} catch(const std::exception& e) {
		os_log_error(sfb::AudioPlayer::log_, "Unable to create std::unique_ptr<AudioPlayer>: %{public}s", e.what());
		return nil;
	}

	if((self = [super init])) {
		_player = std::move(player);
		_player->player_ = self;
	}

	return self;
}

// MARK: - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	if(!_player->enqueueDecoder(decoder, true, error))
		return NO;
	return _player->play(error);
}

- (BOOL)playDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	if(!_player->enqueueDecoder(decoder, true, error))
		return NO;
	return _player->play(error);
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _player->enqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueURL:(NSURL *)url forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _player->enqueueDecoder(decoder, forImmediatePlayback, error);
}

- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _player->enqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _player->enqueueDecoder(decoder, forImmediatePlayback, error);
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return _player->formatWillBeGaplessIfEnqueued(format);
}

- (void)clearQueue
{
	_player->clearDecoderQueue();
}

- (BOOL)queueIsEmpty
{
	return _player->decoderQueueIsEmpty();
}

// MARK: - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	return _player->play(error);
}

- (BOOL)pause
{
	return _player->pause();
}

- (BOOL)resume
{
	return _player->resume();
}

- (void)stop
{
	_player->stop();
}

- (BOOL)togglePlayPauseReturningError:(NSError **)error
{
	return _player->togglePlayPause(error);
}

- (void)reset
{
	_player->reset();
}

// MARK: - Player State

- (BOOL)engineIsRunning
{
	return _player->engineIsRunning();
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	return _player->playbackState();
}

- (BOOL)isPlaying
{
	return _player->isPlaying();
}

- (BOOL)isPaused
{
	return _player->isPaused();
}

- (BOOL)isStopped
{
	return _player->isStopped();
}

- (BOOL)isReady
{
	return _player->isReady();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _player->currentDecoder();
}

- (id<SFBPCMDecoding>)nowPlaying
{
	return _player->nowPlaying();
}

// MARK: - Playback Properties

- (AVAudioFramePosition)framePosition
{
	return _player->playbackPosition().framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _player->playbackPosition().frameLength;
}

- (SFBPlaybackPosition)playbackPosition
{
	return _player->playbackPosition();
}

- (NSTimeInterval)currentTime
{
	return _player->playbackTime().currentTime;
}

- (NSTimeInterval)totalTime
{
	return _player->playbackTime().totalTime;
}

- (SFBPlaybackTime)playbackTime
{
	return _player->playbackTime();
}

- (BOOL)getPlaybackPosition:(SFBPlaybackPosition *)playbackPosition andTime:(SFBPlaybackTime *)playbackTime
{
	return _player->getPlaybackPositionAndTime(playbackPosition, playbackTime);
}

// MARK: - Seeking

- (BOOL)seekForward
{
	return _player->seekInTime(3);
}

- (BOOL)seekBackward
{
	return _player->seekInTime(-3);
}

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return _player->seekInTime(secondsToSkip);
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return _player->seekInTime(-secondsToSkip);
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return _player->seekToTime(timeInSeconds);
}

- (BOOL)seekToPosition:(double)position
{
	return _player->seekToPosition(position);
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return _player->seekToFrame(frame);
}

- (BOOL)supportsSeeking
{
	return _player->supportsSeeking();
}

#if !TARGET_OS_IPHONE
// MARK: - Volume Control

- (float)volume
{
	return _player->volumeForChannel(0);
}

- (BOOL)setVolume:(float)volume error:(NSError **)error
{
	return _player->setVolumeForChannel(volume, 0, error);
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel
{
	return _player->volumeForChannel(channel);
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error
{
	return _player->setVolumeForChannel(volume, channel, error);
}

// MARK: - Output Device

- (AUAudioObjectID)outputDeviceID
{
	return _player->outputDeviceID();
}

- (BOOL)setOutputDeviceID:(AUAudioObjectID)outputDeviceID error:(NSError **)error
{
	return _player->setOutputDeviceID(outputDeviceID, error);
}
#endif /* !TARGET_OS_IPHONE */

// MARK: - AVAudioEngine

- (void)modifyProcessingGraph:(SFBAudioPlayerAVAudioEngineBlock)block
{
	NSParameterAssert(block != nil);
	_player->modifyProcessingGraph(block);
}

- (AVAudioSourceNode *)sourceNode
{
	return _player->sourceNode();
}

- (AVAudioMixerNode *)mainMixerNode
{
	return _player->mainMixerNode();
}

- (AVAudioOutputNode *)outputNode
{
	return _player->outputNode();
}

// MARK: - Debugging

-(void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type
{
	_player->logProcessingGraphDescription(log, type);
}

@end
