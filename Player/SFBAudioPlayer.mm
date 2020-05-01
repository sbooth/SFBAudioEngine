/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <queue>

#import <os/log.h>

#import "SFBAudioPlayer.h"

#import "AVAudioFormat+SFBFormatTransformation.h"
#import "SFBAudioDecoder.h"

const NSNotificationName SFBAudioPlayerAVAudioEngineConfigurationChangeNotification = @"org.sbooth.AudioEngine.AudioPlayer.AVAudioEngineConfigurationChangeNotification";

namespace {
	using DecoderQueue = std::queue<id <SFBPCMDecoding>>;
	os_log_t _audioPlayerLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");
}

@interface SFBAudioPlayer ()
{
@private
	AVAudioEngine 			*_engine;			///< The underlying \c AVAudioEngine instance
	dispatch_queue_t		_engineQueue;		///< The dispatch queue used to access \c _engine
	SFBAudioOutputDevice 	*_outputDevice; 	///< The current output device for \c _engine.outputNode
	SFBAudioPlayerNode		*_playerNode;		///< The player driving the audio processing graph
	dispatch_queue_t		_queue;				///< The dispatch queue used to access \c _queuedDecoders
	DecoderQueue 			_queuedDecoders;	///< Decoders enqueued for non-gapless playback
}
- (void)handleInterruption:(NSNotification *)notification;
- (void)setupEngineForGaplessPlaybackOfFormat:(AVAudioFormat *)format forceUpdate:(BOOL)forceUpdate;
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	if((self = [super init])) {
		_engineQueue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayer.AVAudioEngineIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_engineQueue) {
			os_log_error(_audioPlayerLog, "dispatch_queue_create failed");
			return nil;
		}

		_queue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayer.DecoderQueueIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_queue) {
			os_log_error(_audioPlayerLog, "dispatch_queue_create failed");
			return nil;
		}

		// Create the audio processing graph
		_engine = [[AVAudioEngine alloc] init];
		[self setupEngineForGaplessPlaybackOfFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2] forceUpdate:NO];

		_outputDevice = [[SFBAudioOutputDevice alloc] initWithAudioObjectID:_engine.outputNode.AUAudioUnit.deviceID];

		// Register for configuration change notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleInterruption:) name:AVAudioEngineConfigurationChangeNotification object:_engine];
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

	dispatch_sync(_engineQueue, ^{
		[_engine pause];
		[_engine reset];
		[_playerNode reset];

		// If the current SFBAudioPlayerNode doesn't support the decoder's format (required for gapless join),
		// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
		if(![_playerNode supportsFormat:decoder.processingFormat])
			[self setupEngineForGaplessPlaybackOfFormat:decoder.processingFormat forceUpdate:NO];
	});

	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});

	if(![_playerNode resetAndEnqueueDecoder:decoder error:error])
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
	if(![_playerNode supportsFormat:decoder.processingFormat]) {
		dispatch_sync(_queue, ^{
			_queuedDecoders.push(decoder);
		});
		return YES;
	}

	// Enqueuing will only succeed if the formats match
	return [_playerNode enqueueDecoder:decoder error:error];
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return [_playerNode supportsFormat:format];
}

- (void)skipToNext
{
	if(!_playerNode.queueIsEmpty)
		[_playerNode skipToNext];
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
	[_playerNode clearQueue];
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
	return empty && _playerNode.queueIsEmpty;
}

#pragma mark - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	__block BOOL startedSuccessfully;
	__block NSError *err = nil;
	dispatch_sync(_engineQueue, ^{
		startedSuccessfully = [_engine startAndReturnError:&err];
		if(startedSuccessfully)
			[_playerNode play];
	});

	if(!startedSuccessfully) {
		os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", err);
		if(error)
			*error = err;
	}

	return startedSuccessfully;
}

- (void)pause
{
	[_playerNode pause];
}

- (void)stop
{
	dispatch_sync(_engineQueue, ^{
		[_engine stop];
		[_playerNode stop];
	});

	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});
}

- (BOOL)playPauseReturningError:(NSError **)error
{
	if(_playerNode.isPlaying) {
		[self pause];
		return YES;
	}
	else
		return [self playReturningError:error];
}

- (void)reset
{
	dispatch_sync(_engineQueue, ^{
		[_engine reset];
		[_playerNode reset];
	});

	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});
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
	return _playerNode.isPlaying;
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	if(_engine.isRunning)
		return _playerNode.isPlaying ? SFBAudioPlayerPlaybackStatePlaying : SFBAudioPlayerPlaybackStatePaused;
	else
		return SFBAudioPlayerPlaybackStateStopped;
}

- (BOOL)isPlaying
{
	return _engine.isRunning && _playerNode.isPlaying;
}

- (BOOL)isPaused
{
	return _engine.isRunning && !_playerNode.isPlaying;
}

- (BOOL)isStopped
{
	return !_engine.isRunning;
}

- (BOOL)isReady
{
	return _playerNode.isReady;
}

- (id<SFBPCMDecoding>)decoder
{
	return _playerNode.decoder;
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
	return _playerNode.playbackPosition;
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
	return _playerNode.playbackTime;
}

- (BOOL)getPlaybackPosition:(SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(SFBAudioPlayerPlaybackTime *)playbackTime
{
	return [_playerNode getPlaybackPosition:playbackPosition andTime:playbackTime];
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
	return [_playerNode seekForward:secondsToSkip];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return [_playerNode seekBackward:secondsToSkip];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return [_playerNode seekToTime:timeInSeconds];
}

- (BOOL)seekToPosition:(float)position
{
	return [_playerNode seekToPosition:position];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return [_playerNode seekToFrame:frame];
}

- (BOOL)supportsSeeking
{
	return _playerNode.supportsSeeking;
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
		AudioUnitParameterValue channelVolume;
		OSStatus result = AudioUnitGetParameter(_engine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &channelVolume);
		if(result != noErr) {
			os_log_error(_audioPlayerLog, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d", channel, result);
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
		AudioUnitParameterValue channelVolume = volume;
		OSStatus result = AudioUnitSetParameter(_engine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, channelVolume, 0);
		if(result != noErr) {
			os_log_error(_audioPlayerLog, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d", channel, result);
			err = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return;
		}

		success = YES;
	});

	if(!success && error)
		*error = err;

	return success;
}

#pragma mark - Player Event Callbacks

// Most callbacks are passed directly to the underlying SFBAudioPlayerNode
// The end of audio callback is modified to provide continuous but non-gapless playback

- (SFBAudioDecoderEventBlock)decodingStartedNotificationHandler
{
	return _playerNode.decodingStartedNotificationHandler;
}

- (void)setDecodingStartedNotificationHandler:(SFBAudioDecoderEventBlock)decodingStartedNotificationHandler
{
	_playerNode.decodingStartedNotificationHandler = decodingStartedNotificationHandler;
}

- (SFBAudioDecoderEventBlock)decodingCompleteNotificationHandler
{
	return _playerNode.decodingCompleteNotificationHandler;
}

- (void)setDecodingCompleteNotificationHandler:(SFBAudioDecoderEventBlock)decodingCompleteNotificationHandler
{
	_playerNode.decodingCompleteNotificationHandler = decodingCompleteNotificationHandler;
}

- (SFBAudioDecoderEventBlock)decodingCanceledNotificationHandler
{
	return _playerNode.decodingCanceledNotificationHandler;
}

- (void)setDecodingCanceledNotificationHandler:(SFBAudioDecoderEventBlock)decodingCanceledNotificationHandler
{
	_playerNode.decodingCanceledNotificationHandler = decodingCanceledNotificationHandler;
}

- (SFBAudioDecoderEventBlock)renderingStartedNotificationHandler
{
	return _playerNode.renderingStartedNotificationHandler;
}

- (void)setRenderingStartedNotificationHandler:(SFBAudioDecoderEventBlock)renderingStartedNotificationHandler
{
	_playerNode.renderingStartedNotificationHandler = renderingStartedNotificationHandler;
}

- (SFBAudioDecoderEventBlock)renderingCompleteNotificationHandler
{
	return _playerNode.renderingCompleteNotificationHandler;
}

- (void)setRenderingCompleteNotificationHandler:(SFBAudioDecoderEventBlock)renderingCompleteNotificationHandler
{
	_playerNode.renderingCompleteNotificationHandler = renderingCompleteNotificationHandler;
}

#pragma mark - Output Device

- (BOOL)setOutputDevice:(SFBAudioOutputDevice *)outputDevice error:(NSError **)error
{
	NSParameterAssert(outputDevice != nil);

	os_log_info(_audioPlayerLog, "Setting output device to %{public}@", outputDevice);

	__block BOOL result;
	__block NSError *err = nil;
	dispatch_sync(_engineQueue, ^{
		result = [_engine.outputNode.AUAudioUnit setDeviceID:outputDevice.deviceID error:&err];
	});

	if(result)
		_outputDevice = outputDevice;
	else {
		os_log_error(_audioPlayerLog, "Error setting output device: %{public}@", err);
		if(error)
			*error = err;
	}

	return result;
}

#pragma mark - AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_sync(_engineQueue, ^{
		block(_engine);
	});

	// SFBAudioPlayer requires that the mixer node be connected to the output node
	AVAudioConnectionPoint *outputNodeInputConnectionPoint = [_engine inputConnectionPointForNode:_engine.outputNode inputBus:0];
	NSAssert(outputNodeInputConnectionPoint.node == _engine.mainMixerNode, @"Illegal AVAudioEngine configuration");
}

#pragma mark - Internals

- (void)handleInterruption:(NSNotification *)notification
{
	os_log_debug(_audioPlayerLog, "Received AVAudioEngineConfigurationChangeNotification");

	AVAudioEngine *engine = [notification object];
	if(engine != _engine)
		return;

	// AVAudioEngine posts this notification from a dedicated queue
	dispatch_sync(_engineQueue, ^{
		[_playerNode stop];
		[self setupEngineForGaplessPlaybackOfFormat:_playerNode.renderingFormat forceUpdate:YES];
	});

	[[NSNotificationCenter defaultCenter] postNotificationName:SFBAudioPlayerAVAudioEngineConfigurationChangeNotification object:self];
}

- (void)setupEngineForGaplessPlaybackOfFormat:(AVAudioFormat *)format forceUpdate:(BOOL)forceUpdate
{
	// SFBAudioPlayerNode requires a non-interleaved output format
	if(format.interleaved) {
		format = [format nonInterleavedEquivalent];
		if(!format) {
			os_log_error(_audioPlayerLog, "Unable to convert format %@ to non-interleaved", format);
			return;
		}
	}

	if([format isEqual:_playerNode.renderingFormat] && !forceUpdate)
		return;

	SFBAudioPlayerNode *playerNode = [[SFBAudioPlayerNode alloc] initWithFormat:format];
	if(!playerNode) {
		os_log_error(_audioPlayerLog, "Unable to create SFBAudioPlayerNode with format %{public}@", format);
		return;
	}

	AVAudioOutputNode *outputNode = _engine.outputNode;
	AVAudioMixerNode *mixerNode = _engine.mainMixerNode;

#if DEBUG
	// SFBAudioPlayer requires that the mixer node be connected to the output node
	AVAudioConnectionPoint *outputNodeInputConnectionPoint = [_engine inputConnectionPointForNode:outputNode inputBus:0];
	NSAssert(outputNodeInputConnectionPoint.node == mixerNode, @"Illegal AVAudioEngine configuration");
#endif

	AVAudioFormat *outputFormat = [outputNode outputFormatForBus:0];
	AVAudioFormat *previousOutputFormat = [outputNode inputFormatForBus:0];

	BOOL outputFormatChanged = outputFormat.channelCount != previousOutputFormat.channelCount || outputFormat.sampleRate != previousOutputFormat.sampleRate;
	if(outputFormatChanged)
		os_log_debug(_audioPlayerLog, "AVAudioEngine output format changed from %{public}@ to %{public}@", previousOutputFormat, outputFormat);

	AVAudioConnectionPoint *playerNodeOutputConnectionPoint = nil;
	if(_playerNode) {
		playerNodeOutputConnectionPoint = [[_engine outputConnectionPointsForNode:_playerNode outputBus:0] firstObject];
		[_engine disconnectNodeOutput:_playerNode bus:0];
		[_engine disconnectNodeInput:playerNodeOutputConnectionPoint.node bus:0];
		[_engine detachNode:_playerNode];
	}

	if(outputFormatChanged)
		[_engine disconnectNodeInput:outputNode bus:0];

	playerNode.decodingStartedNotificationHandler = _playerNode.decodingStartedNotificationHandler;
	playerNode.decodingCompleteNotificationHandler = _playerNode.decodingCompleteNotificationHandler;
	playerNode.decodingCanceledNotificationHandler = _playerNode.decodingCanceledNotificationHandler;

	playerNode.renderingStartedNotificationHandler = _playerNode.renderingStartedNotificationHandler;
	playerNode.renderingCompleteNotificationHandler = _playerNode.renderingCompleteNotificationHandler;

	__weak typeof(self) weakSelf = self;
	playerNode.outOfOfAudioNotificationHandler = ^() {
		__strong typeof(self) strongSelf = weakSelf;

		if(!strongSelf) {
			os_log_error(_audioPlayerLog, "Weak reference to self in outOfOfAudioNotificationHandler was zeroed");
			return;
		}

		// Dequeue the next decoder
		__block id <SFBPCMDecoding> decoder = nil;
		dispatch_sync(strongSelf->_queue, ^{
			if(!strongSelf->_queuedDecoders.empty()) {
				decoder = strongSelf->_queuedDecoders.front();
				strongSelf->_queuedDecoders.pop();
			}
		});

		if(decoder) {
			NSError *error = nil;
			if(!decoder.isOpen && ![decoder openReturningError:&error]) {
				os_log_error(_audioPlayerLog, "Error opening decoder: %{public}@", error);
				if(strongSelf->_errorNotificationHandler && error)
					strongSelf->_errorNotificationHandler(error);
			}

			if(decoder.isOpen) {
				dispatch_sync(strongSelf->_engineQueue, ^{
					[strongSelf->_engine pause];
					[strongSelf->_engine reset];
					[strongSelf->_playerNode reset];
					if(![strongSelf->_playerNode supportsFormat:decoder.processingFormat])
						[strongSelf setupEngineForGaplessPlaybackOfFormat:decoder.processingFormat forceUpdate:NO];
				});

				if(![strongSelf->_playerNode resetAndEnqueueDecoder:decoder error:&error]) {
					os_log_error(_audioPlayerLog, "Error enqueuing decoder: %{public}@", error);
					if(strongSelf->_errorNotificationHandler && error)
						strongSelf->_errorNotificationHandler(error);
				}

				if(![strongSelf playReturningError:&error]) {
					if(strongSelf->_errorNotificationHandler && error)
						strongSelf->_errorNotificationHandler(error);
				}
			}
		}
		else if(strongSelf->_outOfAudioNotificationHandler)
			strongSelf->_outOfAudioNotificationHandler();
	};

	_playerNode = playerNode;
	[_engine attachNode:_playerNode];

	// Reconnect the player node to its output
	AVAudioFormat *formatAsStandard = nil;
	if(format.channelLayout)
		formatAsStandard = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:format.sampleRate channelLayout:format.channelLayout];
	else
		formatAsStandard = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:format.sampleRate channels:format.channelCount];

	if(playerNodeOutputConnectionPoint)
		[_engine connect:_playerNode to:playerNodeOutputConnectionPoint.node format:formatAsStandard];
	else
		[_engine connect:_playerNode to:mixerNode format:formatAsStandard];

	// Reconnect the mixer and output nodes using the output device's format
	if(outputFormatChanged)
		[_engine connect:mixerNode to:outputNode format:outputFormat];

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
		os_log_debug(_audioPlayerLog, "AVAudioMixerNode input sample rate (%.2f Hz) and output sample rate (%.2f Hz) don't match", format.sampleRate, outputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		double ratio = format.sampleRate / outputFormat.sampleRate;
		AVAudioFrameCount maximumFramesToRender = (AVAudioFrameCount)ceil(512 * ratio);

		AUAudioUnit *audioUnit = _playerNode.AUAudioUnit;
		if(audioUnit.maximumFramesToRender < maximumFramesToRender) {
			BOOL renderResourcesAllocated = audioUnit.renderResourcesAllocated;
			if(renderResourcesAllocated)
				[audioUnit deallocateRenderResources];

			os_log_debug(_audioPlayerLog, "Adjusting SFBAudioPlayerNode's maximumFramesToRender to %u", maximumFramesToRender);
			audioUnit.maximumFramesToRender = maximumFramesToRender;

			NSError *error;
			if(renderResourcesAllocated && ![audioUnit allocateRenderResourcesAndReturnError:&error]) {
				os_log_error(_audioPlayerLog, "Error allocating AUAudioUnit render resources for SFBAudioPlayerNode: %{public}@", error);
			}
		}
	}
#endif

#if DEBUG
	os_log_debug(_audioPlayerLog, "↑ rendering: %{public}@", _playerNode.renderingFormat);
	if(![[_playerNode outputFormatForBus:0] isEqual:_playerNode.renderingFormat])
		os_log_debug(_audioPlayerLog, "← player out: %{public}@", [_playerNode outputFormatForBus:0]);

	if(![[_engine.mainMixerNode inputFormatForBus:0] isEqual:[_playerNode outputFormatForBus:0]])
		os_log_debug(_audioPlayerLog, "→ main mixer in: %{public}@", [_engine.mainMixerNode inputFormatForBus:0]);

	if(![[_engine.mainMixerNode outputFormatForBus:0] isEqual:[_engine.mainMixerNode inputFormatForBus:0]])
		os_log_debug(_audioPlayerLog, "← main mixer out: %{public}@", [_engine.mainMixerNode outputFormatForBus:0]);

	if(![[_engine.outputNode inputFormatForBus:0] isEqual:[_engine.mainMixerNode outputFormatForBus:0]])
		os_log_debug(_audioPlayerLog, "← output in: %{public}@", [_engine.outputNode inputFormatForBus:0]);

	if(![[_engine.outputNode outputFormatForBus:0] isEqual:[_engine.outputNode inputFormatForBus:0]])
		os_log_debug(_audioPlayerLog, "→ output out: %{public}@", [_engine.outputNode outputFormatForBus:0]);
#endif

	[_engine prepare];
}

@end
