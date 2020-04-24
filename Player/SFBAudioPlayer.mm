/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioPlayer.h"

#import "SFBAudioDecoder.h"

@interface SFBAudioPlayer ()
{
@private
	AVAudioEngine 		*_engine;			//!< The underlying AVAudioEngine instance
	SFBAudioPlayerNode	*_player;			//!< The player
}
- (void)audioEngineConfigurationChanged:(NSNotification *)notification;
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	if((self = [super init])) {
		// Create the audio processing graph
		_engine = [[AVAudioEngine alloc] init];
		[self setupEngineForGaplessPlaybackOfFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2]];

		// Register for configuration change notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(audioEngineConfigurationChanged:) name:AVAudioEngineConfigurationChangeNotification object:_engine];
	}
	return self;
}

#pragma mark Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self playDecoder:decoder error:error];
}

- (BOOL)playDecoder:(id <SFBPCMDecoding> )decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	[_player stop];
	[_engine pause];
	[_engine reset];

	// Determine if the current player node supports the decoder's format for gapless join
	if(![_player supportsFormat:decoder.processingFormat])
		[self setupEngineForGaplessPlaybackOfFormat:decoder.processingFormat];

	if(![_player playDecoder:decoder error:error])
		return NO;

	return [self playReturningError:error];
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self enqueueDecoder:decoder error:error];
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding> )decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	// Enqueuing will only succeed if the formats match
	return [_player enqueueDecoder:decoder error:error];
}

- (void)skipToNext
{
	[_player skipToNext];
}

- (void)clearQueue
{
	[_player clearQueue];
}

#pragma mark Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	if(![_engine startAndReturnError:error])
		return NO;

	[_player play];
	return YES;
}

- (void)pause
{
	[_player pause];
}

- (void)stop
{
	[_player stop];
	[_engine stop];
}

- (BOOL)playPauseReturningError:(NSError **)error
{
	if(self.isPlaying) {
		[self pause];
		return YES;
	}
	else
		return [self playReturningError:error];
}

#pragma mark Player State

- (BOOL)isRunning
{
	return _engine.isRunning;
}

- (BOOL)isPlaying
{
	return _player.isPlaying;
}

- (NSURL *)url
{
	return _player.url;
}

- (id)representedObject
{
	return _player.representedObject;
}

#pragma mark Playback Properties

- (AVAudioFramePosition)framePosition
{
	return self.playbackPosition.framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return self.playbackPosition.frameLength;
}

- (SFBAudioPlayerPlaybackPosition)playbackPosition
{
	return _player.playbackPosition;
}

- (NSTimeInterval)currentTime
{
	return self.playbackTime.currentTime;
}

- (NSTimeInterval)totalTime
{
	return self.playbackTime.totalTime;
}

- (SFBAudioPlayerPlaybackTime)playbackTime
{
	return _player.playbackTime;
}

- (BOOL)getPlaybackPosition:(SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(SFBAudioPlayerPlaybackTime *)playbackTime
{
	return [_player getPlaybackPosition:playbackPosition andTime:playbackTime];
}

#pragma mark Seeking

- (BOOL)seekForward
{
	return [self seekForward:3];
}

- (BOOL)seekBackward
{
	return [self seekBackward:3];
}

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return [_player seekForward:secondsToSkip];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return [_player seekBackward:secondsToSkip];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return [_player seekToTime:timeInSeconds];
}

- (BOOL)seekToPosition:(float)position
{
	return [_player seekToPosition:position];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return [_player seekToFrame:frame];
}

- (BOOL)supportsSeeking
{
	return _player.supportsSeeking;
}

#pragma mark AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	block(_engine);
}

- (void)audioEngineConfigurationChanged:(NSNotification *)notification
{
	os_log_debug(OS_LOG_DEFAULT, "Received AVAudioEngineConfigurationChangeNotification");

	AVAudioEngine *engine = [notification object];

	if(engine != _engine)
		return;

	[_player stop];
	[self setupEngineForGaplessPlaybackOfFormat:_player.renderingFormat];
}

- (void)setupEngineForGaplessPlaybackOfFormat:(AVAudioFormat *)format
{
	if(_player) {
		[_engine disconnectNodeInput:_engine.mainMixerNode];
		[_engine disconnectNodeOutput:_engine.mainMixerNode];
		[_engine detachNode:_player];
	}

	SFBAudioPlayerNode *player = [[SFBAudioPlayerNode alloc] initWithFormat:format];

	[player setRenderingStartedNotificationHandler:^(id<SFBPCMDecoding> obj) {
		if(self->_renderingStartedNotificationHandler)
			self->_renderingStartedNotificationHandler(obj);
	}];

	[player setRenderingFinishedNotificationHandler:^(id<SFBPCMDecoding> obj) {
		if(self->_renderingFinishedNotificationHandler)
			self->_renderingFinishedNotificationHandler(obj);
	}];

	_player = player;
	[_engine attachNode:_player];

	AVAudioOutputNode *output = _engine.outputNode;
	AVAudioMixerNode *mixer = _engine.mainMixerNode;

	AVAudioFormat *outputFormat = [output outputFormatForBus:0];

	[_engine connect:mixer to:output format:outputFormat];
	[_engine connect:_player to:mixer format:format];

#if 1
	// AVAudioMixerNode handles sample rate conversion, but it may require input buffer sizes
	// (maximum frames per slice) greater than the default for AVAudioSourceNode (1156).
	//
	// For high sample rates, the sample rate conversion can require more rendered frames than are available by default.
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the mixer don't match, adjust
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	if(format.sampleRate > outputFormat.sampleRate) {
		os_log_debug(OS_LOG_DEFAULT, "AVAudioMixerNode input sample rate (%.2f Hz) and output sample rate (%.2f Hz) don't match", format.sampleRate, outputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		double ratio = format.sampleRate / outputFormat.sampleRate;
		AVAudioFrameCount maximumFramesToRender = (AVAudioFrameCount)ceil(512 * ratio);

		AUAudioUnit *audioUnit = _player.AUAudioUnit;
		if(audioUnit.maximumFramesToRender < maximumFramesToRender) {
			BOOL renderResourcesAllocated = audioUnit.renderResourcesAllocated;
			if(renderResourcesAllocated)
				[audioUnit deallocateRenderResources];

			os_log_debug(OS_LOG_DEFAULT, "Adjusting SFBAudioPlayerNode's maximumFramesToRender to %u", maximumFramesToRender);
			audioUnit.maximumFramesToRender = maximumFramesToRender;

			NSError *error;
			if(renderResourcesAllocated && ![audioUnit allocateRenderResourcesAndReturnError:&error]) {
				os_log_error(OS_LOG_DEFAULT, "Error allocating AUAudioUnit render resources for SFBAudioPlayerNode: %{public}@",error);
			}
		}
	}
#endif

#if DEBUG
	os_log_debug(OS_LOG_DEFAULT, "SFBAudioPlayerNode rendering format: %{public}@", _player.renderingFormat);
	if(![[_player outputFormatForBus:0] isEqual:_player.renderingFormat])
		os_log_debug(OS_LOG_DEFAULT, "← player out: %{public}@", [_player outputFormatForBus:0]);

	if(![[_engine.mainMixerNode inputFormatForBus:0] isEqual:[_player outputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "→ mixer in:   %{public}@", [_engine.mainMixerNode inputFormatForBus:0]);

	if(![[_engine.mainMixerNode outputFormatForBus:0] isEqual:[_engine.mainMixerNode inputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "← mixer out:  %{public}@", [_engine.mainMixerNode outputFormatForBus:0]);

	if(![[_engine.outputNode inputFormatForBus:0] isEqual:[_engine.mainMixerNode outputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "← output in:  %{public}@", [_engine.outputNode inputFormatForBus:0]);

	if(![[_engine.outputNode outputFormatForBus:0] isEqual:[_engine.outputNode inputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "→ output out: %{public}@", [_engine.outputNode outputFormatForBus:0]);
#endif

	[_engine prepare];
}

@end
