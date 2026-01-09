//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <exception>

#import "SFBAudioPlayer.h"
#import "AudioPlayer.h"
#import <SFBAudioEngine/SFBAudioEngineErrors.h>

@interface SFBAudioPlayer ()
{
@private
	SFB::AudioPlayer::unique_ptr _player;
}
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	std::unique_ptr<SFB::AudioPlayer> player;

	try {
		player = std::make_unique<SFB::AudioPlayer>();
	} catch(const std::exception& e) {
		os_log_error(SFB::AudioPlayer::log_, "Unable to create std::unique_ptr<AudioPlayer>: %{public}s", e.what());
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
	if(!_player->EnqueueDecoder(decoder, true, error))
		return NO;
	return _player->Play(error);
}

- (BOOL)playDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	if(!_player->EnqueueDecoder(decoder, true, error))
		return NO;
	return _player->Play(error);
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _player->EnqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueURL:(NSURL *)url forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _player->EnqueueDecoder(decoder, forImmediatePlayback, error);
}

- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _player->EnqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _player->EnqueueDecoder(decoder, forImmediatePlayback, error);
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return _player->FormatWillBeGaplessIfEnqueued(format);
}

- (void)clearQueue
{
	_player->ClearDecoderQueue();
}

- (BOOL)queueIsEmpty
{
	return _player->DecoderQueueIsEmpty();
}

// MARK: - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	return _player->Play(error);
}

- (BOOL)pause
{
	return _player->Pause();
}

- (BOOL)resume
{
	return _player->Resume();
}

- (void)stop
{
	_player->Stop();
}

- (BOOL)togglePlayPauseReturningError:(NSError **)error
{
	return _player->TogglePlayPause(error);
}

- (void)reset
{
	_player->Reset();
}

// MARK: - Player State

- (BOOL)engineIsRunning
{
	return _player->EngineIsRunning();
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	return _player->PlaybackState();
}

- (BOOL)isPlaying
{
	return _player->IsPlaying();
}

- (BOOL)isPaused
{
	return _player->IsPaused();
}

- (BOOL)isStopped
{
	return _player->IsStopped();
}

- (BOOL)isReady
{
	return _player->IsReady();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _player->CurrentDecoder();
}

- (id<SFBPCMDecoding>)nowPlaying
{
	return _player->NowPlaying();
}

// MARK: - Playback Properties

- (AVAudioFramePosition)framePosition
{
	return _player->PlaybackPosition().framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _player->PlaybackPosition().frameLength;
}

- (SFBPlaybackPosition)playbackPosition
{
	return _player->PlaybackPosition();
}

- (NSTimeInterval)currentTime
{
	return _player->PlaybackTime().currentTime;
}

- (NSTimeInterval)totalTime
{
	return _player->PlaybackTime().totalTime;
}

- (SFBPlaybackTime)playbackTime
{
	return _player->PlaybackTime();
}

- (BOOL)getPlaybackPosition:(SFBPlaybackPosition *)playbackPosition andTime:(SFBPlaybackTime *)playbackTime
{
	return _player->GetPlaybackPositionAndTime(playbackPosition, playbackTime);
}

// MARK: - Seeking

- (BOOL)seekForward
{
	return _player->SeekInTime(3);
}

- (BOOL)seekBackward
{
	return _player->SeekInTime(-3);
}

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return _player->SeekInTime(secondsToSkip);
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return _player->SeekInTime(-secondsToSkip);
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return _player->SeekToTime(timeInSeconds);
}

- (BOOL)seekToPosition:(double)position
{
	return _player->SeekToPosition(position);
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return _player->SeekToFrame(frame);
}

- (BOOL)supportsSeeking
{
	return _player->SupportsSeeking();
}

#if !TARGET_OS_IPHONE

// MARK: - Volume Control

- (float)volume
{
	return _player->VolumeForChannel(0);
}

- (BOOL)setVolume:(float)volume error:(NSError **)error
{
	return _player->SetVolumeForChannel(volume, 0, error);
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel
{
	return _player->VolumeForChannel(channel);
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error
{
	return _player->SetVolumeForChannel(volume, channel, error);
}

// MARK: - Output Device

- (AUAudioObjectID)outputDeviceID
{
	return _player->OutputDeviceID();
}

- (BOOL)setOutputDeviceID:(AUAudioObjectID)outputDeviceID error:(NSError **)error
{
	return _player->SetOutputDeviceID(outputDeviceID, error);
}

#endif /* !TARGET_OS_IPHONE */

// MARK: - AVAudioEngine

- (void)modifyProcessingGraph:(SFBAudioPlayerAVAudioEngineBlock)block
{
	NSParameterAssert(block != nil);
	_player->ModifyProcessingGraph(block);
}

- (AVAudioSourceNode *)sourceNode
{
	return _player->SourceNode();
}

- (AVAudioMixerNode *)mainMixerNode
{
	return _player->MainMixerNode();
}

- (AVAudioOutputNode *)outputNode
{
	return _player->OutputNode();
}

// MARK: - Debugging

-(void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type
{
	_player->LogProcessingGraphDescription(log, type);
}

@end
