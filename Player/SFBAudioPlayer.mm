/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <queue>

#import <os/log.h>

#import "SFBAudioPlayer.h"

#import "AVAudioFormat+SFBFormatTransformation.h"
#import "SFBAudioDecoder.h"

namespace {
	using DecoderQueue = std::queue<id <SFBPCMDecoding>>;
}

@interface SFBAudioPlayer ()
{
@private
	AVAudioEngine 			*_engine;			///< The underlying \c AVAudioEngine instance
	dispatch_queue_t		_engineQueue;		///< The dispatch queue used to access \c _engine
	SFBAudioOutputDevice 	*_outputDevice; 	///< The current output device for \c _engine.outputNode
	SFBAudioPlayerNode		*_player;			///< The player driving the audio processing graph
	dispatch_queue_t		_queue;				///< The dispatch queue used to access \c _queuedDecoders
	DecoderQueue 			_queuedDecoders;	///< Decoders enqueued for non-gapless playback
}
- (void)audioEngineConfigurationChanged:(NSNotification *)notification;
- (void)setupEngineForGaplessPlaybackOfFormat:(AVAudioFormat *)format;
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	if((self = [super init])) {
		_engineQueue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayer.AVAudioEngineIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_engineQueue) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		_queue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayer.DecoderQueueIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_queue) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		// Create the audio processing graph
		_engine = [[AVAudioEngine alloc] init];
		[self setupEngineForGaplessPlaybackOfFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2]];

		_outputDevice = [[SFBAudioOutputDevice alloc] initWithAudioObjectID:_engine.outputNode.AUAudioUnit.deviceID];

		// Register for configuration change notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(audioEngineConfigurationChanged:) name:AVAudioEngineConfigurationChangeNotification object:_engine];
	}
	return self;
}

#pragma mark - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self playDecoder:decoder error:error];
}

- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	[_player stop];
	dispatch_sync(_engineQueue, ^{
		[_engine pause];
		[_engine reset];

		// If the current SFBAudioPlayerNode doesn't support the decoder's format (required for gapless join),
		// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
		if(![_player supportsFormat:decoder.processingFormat])
			[self setupEngineForGaplessPlaybackOfFormat:decoder.processingFormat];
	});

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

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	// If the current SFBAudioPlayerNode doesn't support the decoder's format,
	// add the decoder to our queue
	if(![_player supportsFormat:decoder.processingFormat]) {
		dispatch_sync(_queue, ^{
			_queuedDecoders.push(decoder);
		});
		return YES;
	}

	// Enqueuing will only succeed if the formats match
	return [_player enqueueDecoder:decoder error:error];
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return [_player supportsFormat:format];
}

- (void)skipToNext
{
	if(!_player.queueIsEmpty)
		[_player skipToNext];
	else {
		__block id <SFBPCMDecoding> decoder = nil;
		dispatch_sync(_queue, ^{
			if(!_queuedDecoders.empty()) {
				decoder = _queuedDecoders.front();
				_queuedDecoders.pop();
			}
		});

		if(decoder)
			[self playDecoder:decoder error:nil];
	}
}

- (void)clearQueue
{
	[_player clearQueue];
	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});
}

- (BOOL)queueIsEmpty
{
	__block bool empty = true;
	dispatch_sync(_queue, ^{
		empty = _queuedDecoders.empty();
	});
	return empty && _player.queueIsEmpty;
}

#pragma mark - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	__block BOOL startedSuccessfully;
	__block NSError *err;
	dispatch_sync(_engineQueue, ^{
		startedSuccessfully = [_engine startAndReturnError:&err];
	});

	if(!startedSuccessfully) {
		if(error)
			*error = err;
		return NO;
	}

	[_player play];
	return YES;
}

- (void)pause
{
	[_player pause];
}

- (void)stop
{
	[self clearQueue];
	[_player stop];
	dispatch_sync(_engineQueue, ^{
		[_engine stop];
	});
}

- (BOOL)playPauseReturningError:(NSError **)error
{
	if(_player.isPlaying) {
		[self pause];
		return YES;
	}
	else
		return [self playReturningError:error];
}

#pragma mark - Player State

- (BOOL)engineIsRunning
{
	// I assume this function is thread-safe, but it isn't documented either way
	// This assumption is based on the fact that AUGraphIsRunning() is thread-safe
	return _engine.isRunning;
}

- (BOOL)playerNodeIsPlaying
{
	return _player.isPlaying;
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	if(_engine.isRunning)
		return _player.isPlaying ? SFBAudioPlayerPlaybackStatePlaying : SFBAudioPlayerPlaybackStatePaused;
	else
		return SFBAudioPlayerPlaybackStateStopped;
}

- (BOOL)isPlaying
{
	return _engine.isRunning && _player.isPlaying;
}

- (BOOL)isPaused
{
	return _engine.isRunning && !_player.isPlaying;
}

- (BOOL)isStopped
{
	return !_engine.isRunning;
}

- (NSURL *)url
{
	return _player.url;
}

- (id<SFBPCMDecoding>)decoder
{
	return _player.decoder;
}

#pragma mark - Playback Properties

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

#pragma mark - Seeking

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

#pragma mark - Volume Control

- (float)volume
{
	return [self volumeForChannel:0];
}

- (BOOL)setVolume:(float)volume error:(NSError **)error
{
	return [self setVolume:volume forChannel:0 error:error];
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel
{
	__block float volume = nanf("1");
	dispatch_sync(_engineQueue, ^{
		AudioUnit au = _engine.outputNode.audioUnit;
		if(!au)
			return;

		AudioUnitParameterValue channelVolume;
		OSStatus result = AudioUnitGetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &channelVolume);
		if(result != noErr) {
			os_log_debug(OS_LOG_DEFAULT, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d", channel, result);
			return;
		}

		volume = channelVolume;
	});

	return volume;
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error
{
	__block BOOL success = NO;
	__block NSError *err = nil;
	dispatch_sync(_engineQueue, ^{
		AudioUnit au = _engine.outputNode.audioUnit;
		if(!au)
			return;

		AudioUnitParameterValue channelVolume = volume;
		OSStatus result = AudioUnitSetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, channelVolume, 0);
		if(result != noErr) {
			os_log_debug(OS_LOG_DEFAULT, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d", channel, result);
			err = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return;
		}

		success = YES;
	});

	if(!success && err && error)
		*error = err;

	return success;
}

#pragma mark - Player Event Callbacks

// Most callbacks are passed directly to the underlying SFBAudioPlayerNode
// The rendering finished callback is modified to provide continuous but non-gapless playback

- (SFBAudioDecoderEventBlock)decodingStartedNotificationHandler
{
	return _player.decodingStartedNotificationHandler;
}

- (void)setDecodingStartedNotificationHandler:(SFBAudioDecoderEventBlock)decodingStartedNotificationHandler
{
	_player.decodingStartedNotificationHandler = decodingStartedNotificationHandler;
}

- (SFBAudioDecoderEventBlock)decodingFinishedNotificationHandler
{
	return _player.decodingFinishedNotificationHandler;
}

- (void)setDecodingFinishedNotificationHandler:(SFBAudioDecoderEventBlock)decodingFinishedNotificationHandler
{
	_player.decodingFinishedNotificationHandler = decodingFinishedNotificationHandler;
}

- (SFBAudioDecoderEventBlock)decodingCanceledNotificationHandler
{
	return _player.decodingCanceledNotificationHandler;
}

- (void)setDecodingCanceledNotificationHandler:(SFBAudioDecoderEventBlock)decodingCanceledNotificationHandler
{
	_player.decodingCanceledNotificationHandler = decodingCanceledNotificationHandler;
}

- (SFBAudioDecoderEventBlock)renderingStartedNotificationHandler
{
	return _player.renderingStartedNotificationHandler;
}

- (void)setRenderingStartedNotificationHandler:(SFBAudioDecoderEventBlock)renderingStartedNotificationHandler
{
	_player.renderingStartedNotificationHandler = renderingStartedNotificationHandler;
}

#pragma mark - Output Device

- (BOOL)setOutputDevice:(SFBAudioOutputDevice *)outputDevice error:(NSError **)error
{
	NSParameterAssert(outputDevice != nil);

	os_log_debug(OS_LOG_DEFAULT, "Setting output device to %{public}@", outputDevice);

	__block BOOL result;
	__block NSError *err = nil;
	dispatch_sync(_engineQueue, ^{
		result = [_engine.outputNode.AUAudioUnit setDeviceID:outputDevice.deviceID error:&err];
	});

	if(result)
		_outputDevice = outputDevice;
	else if(err && error)
		*error = err;

	return result;
}

#pragma mark - AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_sync(_engineQueue, ^{
		block(_engine);
	});
}

#pragma mark - Internals

- (void)audioEngineConfigurationChanged:(NSNotification *)notification
{
	os_log_debug(OS_LOG_DEFAULT, "Received AVAudioEngineConfigurationChangeNotification");

	AVAudioEngine *engine = [notification object];

	if(engine != _engine)
		return;

	[_player stop];
	// AVAudioEngine posts this notification from a dedicated queue
	dispatch_sync(_engineQueue, ^{
		[self setupEngineForGaplessPlaybackOfFormat:_player.renderingFormat];
	});
}

- (void)setupEngineForGaplessPlaybackOfFormat:(AVAudioFormat *)format
{
	// SFBAudioPlayerNode requires a non-interleaved output format
	if(format.interleaved) {
		format = [format nonInterleavedEquivalent];
		if(!format) {
			os_log_error(OS_LOG_DEFAULT, "Unable to convert format %@ to non-interleaved", format);
			return;
		}
	}

	SFBAudioPlayerNode *player = [[SFBAudioPlayerNode alloc] initWithFormat:format];
	if(!player) {
		os_log_error(OS_LOG_DEFAULT, "Unable to create SFBAudioPlayerNode with format %@", format);
		return;
	}

	if(_player) {
		[_engine disconnectNodeInput:_engine.mainMixerNode];
		[_engine disconnectNodeOutput:_engine.mainMixerNode];
		[_engine detachNode:_player];
	}

	player.decodingStartedNotificationHandler = _player.decodingStartedNotificationHandler;
	player.decodingFinishedNotificationHandler = _player.decodingFinishedNotificationHandler;
	player.decodingCanceledNotificationHandler = _player.decodingCanceledNotificationHandler;

	player.renderingStartedNotificationHandler = _player.renderingStartedNotificationHandler;

	__weak typeof(self) weakSelf = self;
	player.renderingFinishedNotificationHandler = ^(id<SFBPCMDecoding> obj) {
		__strong typeof(self) strongSelf = weakSelf;

		if(!strongSelf) {
			os_log_error(OS_LOG_DEFAULT, "Weak reference to self in renderingFinishedNotificationHandler was zeroed");
			return;
		}

		if(strongSelf->_renderingFinishedNotificationHandler)
			strongSelf->_renderingFinishedNotificationHandler(obj);

		__block id <SFBPCMDecoding> decoder = nil;
		dispatch_sync(strongSelf->_queue, ^{
			if(!strongSelf->_queuedDecoders.empty()) {
				decoder = strongSelf->_queuedDecoders.front();
				strongSelf->_queuedDecoders.pop();
			}
		});

		if(decoder)
			[strongSelf playDecoder:decoder error:nil];
	};

	_player = player;
	[_engine attachNode:_player];

	AVAudioOutputNode *output = _engine.outputNode;
	AVAudioMixerNode *mixer = _engine.mainMixerNode;

	AVAudioFormat *outputFormat = [output outputFormatForBus:0];

	[_engine connect:mixer to:output format:outputFormat];
	[_engine connect:_player to:mixer format:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:format.sampleRate channels:format.channelCount]];

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
				os_log_error(OS_LOG_DEFAULT, "Error allocating AUAudioUnit render resources for SFBAudioPlayerNode: %{public}@", error);
			}
		}
	}
#endif

#if DEBUG
	os_log_debug(OS_LOG_DEFAULT, "↑ rendering: %{public}@", _player.renderingFormat);
	if(![[_player outputFormatForBus:0] isEqual:_player.renderingFormat])
		os_log_debug(OS_LOG_DEFAULT, "← player out: %{public}@", [_player outputFormatForBus:0]);

	if(![[_engine.mainMixerNode inputFormatForBus:0] isEqual:[_player outputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "→ mixer in: %{public}@", [_engine.mainMixerNode inputFormatForBus:0]);

	if(![[_engine.mainMixerNode outputFormatForBus:0] isEqual:[_engine.mainMixerNode inputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "← mixer out: %{public}@", [_engine.mainMixerNode outputFormatForBus:0]);

	if(![[_engine.outputNode inputFormatForBus:0] isEqual:[_engine.mainMixerNode outputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "← output in: %{public}@", [_engine.outputNode inputFormatForBus:0]);

	if(![[_engine.outputNode outputFormatForBus:0] isEqual:[_engine.outputNode inputFormatForBus:0]])
		os_log_debug(OS_LOG_DEFAULT, "→ output out: %{public}@", [_engine.outputNode outputFormatForBus:0]);
#endif

	[_engine prepare];
}

@end
