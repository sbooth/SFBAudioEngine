//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <atomic>
#import <cmath>
#import <mutex>
#import <queue>

#import <objc/objc-runtime.h>

#import <AVAudioFormat+SFBFormatTransformation.h>

#import <SFBUnfairLock.hpp>

#import "SFBAudioPlayer.h"

#import "SFBAudioDecoder.h"
#import "SFBCStringForOSType.h"
#import "SFBStringDescribingAVAudioFormat.h"
#import "SFBTimeUtilities.hpp"

namespace {

/// Objective-C associated object key indicating if a decoder has been canceled
const char _decoderIsCanceledKey = '\0';

using DecoderQueue = std::queue<id <SFBPCMDecoding>>;
const os_log_t _audioPlayerLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

enum AudioPlayerFlags : unsigned int {
	eAudioPlayerFlagRenderingImminent				= 1u << 0,
	eAudioPlayerFlagHavePendingDecoder				= 1u << 1,
	eAudioPlayerFlagPendingDecoderBecameActive		= 1u << 2,
};

#if !TARGET_OS_IPHONE
/// Returns the name of `audioUnit.deviceID`
///
/// This is the value of `kAudioObjectPropertyName` in the output scope on the main element
NSString * _Nullable AudioDeviceName(AUAudioUnit * _Nonnull audioUnit) noexcept
{
	NSCParameterAssert(audioUnit != nil);

	AudioObjectPropertyAddress address = {
		.mSelector = kAudioObjectPropertyName,
		.mScope = kAudioObjectPropertyScopeOutput,
		.mElement = kAudioObjectPropertyElementMain
	};
	CFStringRef name = nullptr;
	UInt32 dataSize = sizeof(name);
	OSStatus result = AudioObjectGetPropertyData(audioUnit.deviceID, &address, 0, nullptr, &dataSize, &name);
	if(result != noErr) {
		os_log_error(_audioPlayerLog, "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: %d", result);
		return nil;
	}
	return (__bridge_transfer NSString *)name;
}
#endif // !TARGET_OS_IPHONE

} // namespace

@interface SFBAudioPlayer ()
{
@private
	/// The underlying `AVAudioEngine` instance
	AVAudioEngine 			*_engine;
	/// The dispatch queue used to access `_engine`
	dispatch_queue_t		_engineQueue;
	/// Cached value of `_engine`.isRunning
	std::atomic_bool		_engineIsRunning;
	/// The player driving the audio processing graph
	SFBAudioPlayerNode		*_playerNode;
	/// The lock used to protect access to `_queuedDecoders`
	SFB::UnfairLock			_queueLock;
	/// Decoders enqueued for non-gapless playback
	DecoderQueue 			_queuedDecoders;
	/// The lock used to protect access to `_nowPlaying`
	SFB::UnfairLock			_nowPlayingLock;
	/// The currently rendering decoder
	id <SFBPCMDecoding> 	_nowPlaying;
	/// Flags
	std::atomic_uint		_flags;
}
/// Returns true if the internal queue of decoders is empty
- (BOOL)internalDecoderQueueIsEmpty;
/// Removes all decoders from the internal decoder queue
- (void)clearInternalDecoderQueue;
/// Inserts `decoder` at the end of the internal decoder queue
- (void)pushDecoderToInternalQueue:(id <SFBPCMDecoding>)decoder;
/// Removes and returns the first decoder from the internal decoder queue
- (nullable id <SFBPCMDecoding>)popDecoderFromInternalQueue;
/// Called to process `AVAudioEngineConfigurationChangeNotification`
- (void)handleAudioEngineConfigurationChange:(NSNotification *)notification;
#if TARGET_OS_IPHONE
/// Called to process `AVAudioSessionInterruptionNotification`
- (void)handleAudioSessionInterruption:(NSNotification *)notification;
#endif // TARGET_OS_IPHONE
/// Configures the player to render audio from `decoder` and enqueues `decoder` on the player node
/// - parameter forImmediatePlayback: If `YES` the internal decoder queue is cleared and the player node is reset
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the player was successfully configured
- (BOOL)configureForAndEnqueueDecoder:(nonnull id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error;
/// Configures the audio processing graph for playback of audio with `format`, replacing the audio player node if necessary
///
/// This method does nothing if the current rendering format is equal to `format`
/// - important: This stops the audio engine if reconfiguration is necessary
/// - parameter format: The desired audio format
/// - parameter forceUpdate: Whether the graph should be rebuilt even if the current rendering format is equal to `format`
/// - returns: `YES` if the processing graph was successfully configured
- (BOOL)configureProcessingGraphForFormat:(nonnull AVAudioFormat *)format forceUpdate:(BOOL)forceUpdate;
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	if((self = [super init])) {
		_engineQueue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayer.AVAudioEngineIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_engineQueue) {
			os_log_error(_audioPlayerLog, "Unable to create AVAudioEngine isolation dispatch queue: dispatch_queue_create failed");
			return nil;
		}

		// Create the audio processing graph
		_engine = [[AVAudioEngine alloc] init];
		if(![self configureProcessingGraphForFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2] forceUpdate:NO]) {
			os_log_error(_audioPlayerLog, "Unable to create audio processing graph for 44.1 kHz stereo");
			return nil;
		}

		// Register for configuration change notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioEngineConfigurationChange:) name:AVAudioEngineConfigurationChangeNotification object:_engine];

#if TARGET_OS_IPHONE
		// Register for audio session interruption notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioSessionInterruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];
#endif // TARGET_OS_IPHONE
	}
	return self;
}

#pragma mark - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	if(![self enqueueURL:url forImmediatePlayback:YES error:error])
		return NO;
	return [self playReturningError:error];
}

- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	if(![self enqueueDecoder:decoder forImmediatePlayback:YES error:error])
		return NO;
	return [self playReturningError:error];
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	return [self enqueueURL:url forImmediatePlayback:NO error:error];
}

- (BOOL)enqueueURL:(NSURL *)url forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return [self enqueueDecoder:decoder forImmediatePlayback:forImmediatePlayback error:error];
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	return [self enqueueDecoder:decoder forImmediatePlayback:NO error:error];
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	// Reconfigure the audio processing graph for the decoder's processing format if requested
	if(forImmediatePlayback)
		return [self configureForAndEnqueueDecoder:decoder forImmediatePlayback:YES error:error];
	// To preserve the order of enqueued decoders, when the internal queue is not empty
	// enqueue all decoders there regardless of format compability with _playerNode
	// This prevents incorrect playback order arising from the scenario where
	// decoders A and AA have formats supported by _playerNode and decoder B does not;
	// bypassing the internal queue for supported formats when enqueueing A, B, AA
	// would result in playback order A, AA, B
	else if(self.internalDecoderQueueIsEmpty && [_playerNode supportsFormat:decoder.processingFormat]) {
		// Enqueuing is expected to succeed since the formats are compatible
		return [_playerNode enqueueDecoder:decoder error:error];
	}
	// If the internal queue is not empty or _playerNode doesn't support
	// the decoder's processing format add the decoder to our internal queue
	else {
		[self pushDecoderToInternalQueue:decoder];
		return YES;
	}
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return [_playerNode supportsFormat:format];
}

- (void)clearQueue
{
	[_playerNode clearQueue];
	[self clearInternalDecoderQueue];
}

- (BOOL)queueIsEmpty
{
	return _playerNode.queueIsEmpty && self.internalDecoderQueueIsEmpty;
}

#pragma mark - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	if(self.isPlaying)
		return YES;

	__block NSError *err = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		_engineIsRunning = [_engine startAndReturnError:&err];
		if(_engineIsRunning)
			[_playerNode play];
	});

	if(!_engineIsRunning) {
		os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", err);
		if(error)
			*error = err;
		return NO;
	}

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePlaying, @"Incorrect playback state in -playReturningError:");
#endif // DEBUG

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];

	return YES;
}

- (void)pause
{
	if(!self.isPlaying)
		return;

	[_playerNode pause];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePaused, @"Incorrect playback state in -pause");
#endif // DEBUG

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
}

- (void)resume
{
	if(!self.isPaused)
		return;

	[_playerNode play];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePlaying, @"Incorrect playback state in -resume");
#endif // DEBUG

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
}

- (void)stop
{
	if(self.isStopped)
		return;

	dispatch_async_and_wait(_engineQueue, ^{
		[_engine stop];
		_engineIsRunning = false;
		[_playerNode stop];
	});

	[self clearInternalDecoderQueue];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStateStopped, @"Incorrect playback state in -stop");
#endif // DEBUG

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
}

- (BOOL)togglePlayPauseReturningError:(NSError **)error
{
	switch(self.playbackState) {
		case SFBAudioPlayerPlaybackStatePlaying:
			[self pause];
			return YES;
		case SFBAudioPlayerPlaybackStatePaused:
			[self resume];
			return YES;
		case SFBAudioPlayerPlaybackStateStopped:
			return [self playReturningError:error];
	}
}

- (void)reset
{
	dispatch_async_and_wait(_engineQueue, ^{
		[_playerNode reset];
		[_engine reset];
	});

	[self clearInternalDecoderQueue];
}

#pragma mark - Player State

- (BOOL)engineIsRunning
{
	__block BOOL isRunning;
	dispatch_async_and_wait(_engineQueue, ^{
		isRunning = _engine.isRunning;
#if DEBUG
		NSAssert(_engineIsRunning == isRunning, @"Cached value for _engine.isRunning invalid");
#endif // DEBUG
	});
	return isRunning;
}

- (BOOL)playerNodeIsPlaying
{
	return _playerNode.isPlaying;
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	if(_engineIsRunning)
		return _playerNode.isPlaying ? SFBAudioPlayerPlaybackStatePlaying : SFBAudioPlayerPlaybackStatePaused;
	else
		return SFBAudioPlayerPlaybackStateStopped;
}

- (BOOL)isPlaying
{
	return _engineIsRunning && _playerNode.isPlaying;
}

- (BOOL)isPaused
{
	return _engineIsRunning && !_playerNode.isPlaying;
}

- (BOOL)isStopped
{
	return !_engineIsRunning;
}

- (BOOL)isReady
{
	return _playerNode.isReady;
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _playerNode.currentDecoder;
}

- (id<SFBPCMDecoding>)nowPlaying
{
	std::lock_guard<SFB::UnfairLock> lock(_nowPlayingLock);
	return _nowPlaying;
}

- (void)setNowPlaying:(id<SFBPCMDecoding>)nowPlaying
{
	{
		std::lock_guard<SFB::UnfairLock> lock(_nowPlayingLock);
#if DEBUG
		NSAssert(_nowPlaying != nowPlaying, @"Unnecessary _nowPlaying change to %@", nowPlaying);
#endif // DEBUG
		_nowPlaying = nowPlaying;
	}

	os_log_debug(_audioPlayerLog, "Now playing changed to %{public}@", nowPlaying);

	if([_delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:)])
		[_delegate audioPlayer:self nowPlayingChanged:nowPlaying];
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

- (BOOL)seekToPosition:(double)position
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

#if !TARGET_OS_IPHONE

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
	__block float volume = std::nanf("1");
	dispatch_async_and_wait(_engineQueue, ^{
		AudioUnitParameterValue channelVolume;
		OSStatus result = AudioUnitGetParameter(_engine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &channelVolume);
		if(result != noErr) {
			os_log_error(_audioPlayerLog, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
			return;
		}

		volume = channelVolume;
	});

	return volume;
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error
{
	os_log_info(_audioPlayerLog, "Setting volume for channel %u to %g", channel, volume);

	__block BOOL success = NO;
	__block NSError *err = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		AudioUnitParameterValue channelVolume = volume;
		OSStatus result = AudioUnitSetParameter(_engine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, channelVolume, 0);
		if(result != noErr) {
			os_log_error(_audioPlayerLog, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
			err = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return;
		}

		success = YES;
	});

	if(!success && error)
		*error = err;

	return success;
}

#pragma mark - Output Device

- (AUAudioObjectID)outputDeviceID
{
	__block AUAudioObjectID objectID = kAudioObjectUnknown;
	dispatch_async_and_wait(_engineQueue, ^{
		objectID = _engine.outputNode.AUAudioUnit.deviceID;
	});
	return objectID;
}

- (BOOL)setOutputDeviceID:(AUAudioObjectID)outputDeviceID error:(NSError **)error
{
	os_log_info(_audioPlayerLog, "Setting output device to 0x%x", outputDeviceID);

	__block BOOL result;
	__block NSError *err = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		result = [_engine.outputNode.AUAudioUnit setDeviceID:outputDeviceID error:&err];
	});

	if(!result) {
		os_log_error(_audioPlayerLog, "Error setting output device: %{public}@", err);
		if(error)
			*error = err;
	}

	return result;
}

#endif // !TARGET_OS_IPHONE

#pragma mark - AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_async_and_wait(_engineQueue, ^{
		block(_engine);
		// SFBAudioPlayer requires that the mixer node be connected to the output node
		NSAssert([_engine inputConnectionPointForNode:_engine.outputNode inputBus:0].node == _engine.mainMixerNode, @"Illegal AVAudioEngine configuration");
		NSAssert(_engine.isRunning == _engineIsRunning, @"AVAudioEngine may not be started or stopped outside of SFBAudioPlayer");
	});
}

#pragma mark - Debugging

-(void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type
{
	dispatch_async(_engineQueue, ^{
		NSMutableString *string = [NSMutableString stringWithString:@"Audio processing graph:\n"];

		AVAudioFormat *inputFormat = _playerNode.renderingFormat;
		[string appendFormat:@"↓ rendering\n    %@\n", SFB::StringDescribingAVAudioFormat(inputFormat)];

		AVAudioFormat *outputFormat = [_playerNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", _playerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", _playerNode];

		AVAudioConnectionPoint *connectionPoint = [[_engine outputConnectionPointsForNode:_playerNode outputBus:0] firstObject];
		while(connectionPoint.node != _engine.mainMixerNode) {
			inputFormat = [connectionPoint.node inputFormatForBus:connectionPoint.bus];
			outputFormat = [connectionPoint.node outputFormatForBus:connectionPoint.bus];
			if(![outputFormat isEqual:inputFormat])
				[string appendFormat:@"→ %@\n    %@\n", connectionPoint.node, SFB::StringDescribingAVAudioFormat(outputFormat)];

			else
				[string appendFormat:@"→ %@\n", connectionPoint.node];

			connectionPoint = [[_engine outputConnectionPointsForNode:connectionPoint.node outputBus:0] firstObject];
		}

		inputFormat = [_engine.mainMixerNode inputFormatForBus:0];
		outputFormat = [_engine.mainMixerNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", _engine.mainMixerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", _engine.mainMixerNode];

		inputFormat = [_engine.outputNode inputFormatForBus:0];
		outputFormat = [_engine.outputNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@]", _engine.outputNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@", _engine.outputNode];

#if !TARGET_OS_IPHONE
		[string appendFormat:@"\n↓ \"%@\"", AudioDeviceName(_engine.outputNode.AUAudioUnit)];
#endif // !TARGET_OS_IPHONE

		os_log_with_type(log, type, "%{public}@", string);
	});
}

#pragma mark - Decoder Queue

- (BOOL)internalDecoderQueueIsEmpty
{
	std::lock_guard<SFB::UnfairLock> lock(_queueLock);
	return _queuedDecoders.empty();
}

- (void)clearInternalDecoderQueue
{
	std::lock_guard<SFB::UnfairLock> lock(_queueLock);
	while(!_queuedDecoders.empty())
		_queuedDecoders.pop();
}

- (void)pushDecoderToInternalQueue:(id <SFBPCMDecoding>)decoder
{
	std::lock_guard<SFB::UnfairLock> lock(_queueLock);
	_queuedDecoders.push(decoder);
}

- (id <SFBPCMDecoding>)popDecoderFromInternalQueue
{
	std::lock_guard<SFB::UnfairLock> lock(_queueLock);
	id <SFBPCMDecoding> decoder = nil;
	if(!_queuedDecoders.empty()) {
		decoder = _queuedDecoders.front();
		_queuedDecoders.pop();
	}
	return decoder;
}

#pragma mark - Internals

- (void)handleAudioEngineConfigurationChange:(NSNotification *)notification
{
	NSAssert([notification object] == _engine, @"AVAudioEngineConfigurationChangeNotification received for incorrect AVAudioEngine instance");
	os_log_debug(_audioPlayerLog, "Received AVAudioEngineConfigurationChangeNotification");

	// AVAudioEngine stops itself when interrupted and there is no way to determine if the engine was
	// running before this notification was issued unless the state is cached
	const bool engineWasRunning = _engineIsRunning;
	_engineIsRunning = false;

	// Attempt to preserve the playback state
	const BOOL playerNodeWasPlaying = _playerNode.isPlaying;

	// AVAudioEngine posts this notification from a dedicated queue
	__block BOOL success;
	__block NSError *error = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		[_playerNode pause];

		// Force an update of the audio processing graph
		success = [self configureProcessingGraphForFormat:_playerNode.renderingFormat forceUpdate:YES];
		if(!success) {
			os_log_error(_audioPlayerLog, "Unable to create audio processing graph for %{public}@", SFB::StringDescribingAVAudioFormat(_playerNode.renderingFormat));
			error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			return;
		}

		// Restart AVAudioEngine if previously running
		if(engineWasRunning) {
			_engineIsRunning = [_engine startAndReturnError:&error];
			if(!_engineIsRunning) {
				os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", error);
				return;
			}

			// Restart the player node if needed
			if(playerNodeWasPlaying)
				[_playerNode play];
		}
	});

	// Success in this context means the graph is in a working state, not that the engine was restarted successfully
	if(!success) {
		if([_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
			[_delegate audioPlayer:self encounteredError:error];
		return;
	}

	if((engineWasRunning != _engineIsRunning || playerNodeWasPlaying != _playerNode.isPlaying) && [_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:self.playbackState];

	if([_delegate respondsToSelector:@selector(audioPlayerAVAudioEngineConfigurationChange:)])
		[_delegate audioPlayerAVAudioEngineConfigurationChange:self];
}

#if TARGET_OS_IPHONE
- (void)handleAudioSessionInterruption:(NSNotification *)notification
{
	NSUInteger interruptionType = [[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
	switch(interruptionType) {
		case AVAudioSessionInterruptionTypeBegan:
			os_log_debug(_audioPlayerLog, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeBegan)");
			[self pause];
			break;

		case AVAudioSessionInterruptionTypeEnded:
			os_log_debug(_audioPlayerLog, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

			// AVAudioEngine stops itself when AVAudioSessionInterruptionNotification is received
			// However, _engineIsRunning isn't updated and will indicate if the engine was running before the interruption
			if(_engineIsRunning) {
				dispatch_async_and_wait(_engineQueue, ^{
					NSError *error = nil;
					_engineIsRunning = [_engine startAndReturnError:&error];
					if(!_engineIsRunning)
						os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", error);
				});
			}
			break;

		default:
			os_log_error(_audioPlayerLog, "Unknown value %lu for AVAudioSessionInterruptionTypeKey", static_cast<unsigned long>(interruptionType));
			break;
	}
}
#endif // TARGET_OS_IPHONE

- (BOOL)configureForAndEnqueueDecoder:(id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	_flags.fetch_or(eAudioPlayerFlagHavePendingDecoder);

	// Attempt to preserve the playback state
	const bool engineWasRunning = _engineIsRunning;
	const BOOL playerNodeWasPlaying = _playerNode.isPlaying;

	__block BOOL success = YES;

	// If the current SFBAudioPlayerNode doesn't support the decoder's format (required for gapless join),
	// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
	if(auto format = decoder.processingFormat; ![_playerNode supportsFormat:format])
		dispatch_async_and_wait(_engineQueue, ^{
			success = [self configureProcessingGraphForFormat:format forceUpdate:NO];
		});

	if(!success) {
		if(error)
			*error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
		_flags.fetch_and(~eAudioPlayerFlagHavePendingDecoder);
		if(self.nowPlaying)
			self.nowPlaying = nil;
		return NO;
	}

	if(forImmediatePlayback) {
		[self clearInternalDecoderQueue];
		success = [_playerNode resetAndEnqueueDecoder:decoder error:error];
	}
	else
		success = [_playerNode enqueueDecoder:decoder error:error];

	// Failure is unlikely since the audio processing graph was reconfigured for the decoder's processing format
	if(!success) {
		_flags.fetch_and(~eAudioPlayerFlagHavePendingDecoder);
		if(self.nowPlaying)
			self.nowPlaying = nil;
		return NO;
	}

	// AVAudioEngine may have been stopped in `-configureProcessingGraphForFormat:forceUpdate:`
	// If this is the case and it was previously running, restart it and the player node
	// as appropriate
	if(engineWasRunning && !_engineIsRunning) {
		__block NSError *err = nil;
		dispatch_async_and_wait(_engineQueue, ^{
			_engineIsRunning = [_engine startAndReturnError:&err];
			if(_engineIsRunning && playerNodeWasPlaying)
				[_playerNode play];
		});

		if(!_engineIsRunning) {
			os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return NO;
		}
	}

#if DEBUG
	NSAssert(engineWasRunning == _engineIsRunning && playerNodeWasPlaying == _playerNode.isPlaying, @"Incorrect playback state in -configureForAndEnqueueDecoder:forImmediatePlayback:error:");
#endif // DEBUG

	return YES;
}

- (BOOL)configureProcessingGraphForFormat:(AVAudioFormat *)format forceUpdate:(BOOL)forceUpdate
{
	NSParameterAssert(format != nil);

	// SFBAudioPlayerNode requires the standard format
	if(!format.isStandard) {
		AVAudioFormat *standardEquivalentFormat = [format standardEquivalent];
		if(!standardEquivalentFormat) {
			os_log_error(_audioPlayerLog, "Unable to convert format %{public}@ to standard equivalent", SFB::StringDescribingAVAudioFormat(format));
			return NO;
		}
		format = standardEquivalentFormat;
	}

	const BOOL formatsEqual = [format isEqual:_playerNode.renderingFormat];
	if(formatsEqual && !forceUpdate)
		return YES;

	// Even if the engine isn't running, call stop to force release of any render resources
	// Empirically this is necessary when transitioning between formats with different
	// channel counts, although it seems that it shouldn't be
	[_engine stop];
	_engineIsRunning = false;

	if(_playerNode.isPlaying)
		[_playerNode stop];

	// Avoid creating a new SFBAudioPlayerNode if not necessary
	SFBAudioPlayerNode *playerNode = nil;
	if(!formatsEqual) {
		playerNode = [[SFBAudioPlayerNode alloc] initWithFormat:format];
		if(!playerNode) {
			os_log_error(_audioPlayerLog, "Unable to create SFBAudioPlayerNode with format %{public}@", SFB::StringDescribingAVAudioFormat(format));
			return NO;
		}

		playerNode.delegate = self;
	}

	AVAudioOutputNode *outputNode = _engine.outputNode;
	AVAudioMixerNode *mixerNode = _engine.mainMixerNode;

	// SFBAudioPlayer requires that the main mixer node be connected to the output node
	NSAssert([_engine inputConnectionPointForNode:outputNode inputBus:0].node == mixerNode, @"Illegal AVAudioEngine configuration");

	AVAudioFormat *outputNodeOutputFormat = [outputNode outputFormatForBus:0];
	AVAudioFormat *mixerNodeOutputFormat = [mixerNode outputFormatForBus:0];

	const auto outputFormatsMismatch = outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount || outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate;
	if(outputFormatsMismatch) {
		os_log_debug(_audioPlayerLog,
					 "Mismatch between output formats for main mixer and output nodes:\n    mainMixerNode: %{public}@\n       outputNode: %{public}@",
					 SFB::StringDescribingAVAudioFormat(mixerNodeOutputFormat),
					 SFB::StringDescribingAVAudioFormat(outputNodeOutputFormat));

		[_engine disconnectNodeInput:outputNode bus:0];

		// Reconnect the mixer and output nodes using the output node's output format
		[_engine connect:mixerNode to:outputNode format:outputNodeOutputFormat];
	}

	if(playerNode) {
		AVAudioConnectionPoint *playerNodeOutputConnectionPoint = nil;
		if(_playerNode) {
			playerNodeOutputConnectionPoint = [[_engine outputConnectionPointsForNode:_playerNode outputBus:0] firstObject];
			[_engine detachNode:_playerNode];

			// When an audio player node is deallocated the destructor synchronously waits
			// for decoder cancelation (if there is an active decoder) and then for any
			// final events to be processed and delegate messages sent.
			// The potential therefore exists to block the calling thread for a perceptible amount
			// of time, especially if the delegate callouts take longer than ideal.
			//
			// In my measurements the baseline with an empty delegate implementation of
			// -audioPlayer:decodingCanceled:framesRendered: seems to be around 100 µsec
			//
			// Assuming there are no external references to the audio player node,
			// setting it to nil here sends -dealloc
			_playerNode = nil;
		}

		_playerNode = playerNode;
		[_engine attachNode:_playerNode];

		// Reconnect the player node to the next node in the processing chain
		// This is the mixer node in the default configuration, but additional nodes may
		// have been inserted between the player and mixer nodes. In this case allow the delegate
		// to make any necessary adjustments based on the format change if desired.
		if(playerNodeOutputConnectionPoint && playerNodeOutputConnectionPoint.node != mixerNode) {
			if([_delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
				AVAudioNode *node = [_delegate audioPlayer:self reconfigureProcessingGraph:_engine withFormat:format];
				// Ensure the delegate returned a valid node
				NSAssert(node != nil, @"nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
				[_engine connect:_playerNode to:node format:format];
			}
			else
				[_engine connect:_playerNode to:playerNodeOutputConnectionPoint.node format:format];
		}
		else
			[_engine connect:_playerNode to:mixerNode format:format];
	}

	// AVAudioMixerNode handles sample rate conversion, but it may require input buffer sizes
	// (maximum frames per slice) greater than the default for AVAudioSourceNode (1156).
	//
	// For high sample rates, the sample rate conversion can require more rendered frames than are available by default.
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the mixer don't match, adjust
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	if(format.sampleRate > outputNodeOutputFormat.sampleRate) {
		os_log_debug(_audioPlayerLog, "AVAudioMixerNode input sample rate (%g Hz) and output sample rate (%g Hz) don't match", format.sampleRate, outputNodeOutputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		const double ratio = format.sampleRate / outputNodeOutputFormat.sampleRate;
		const auto maximumFramesToRender = static_cast<AUAudioFrameCount>(std::ceil(512 * ratio));

		if(auto audioUnit = _playerNode.AUAudioUnit; audioUnit.maximumFramesToRender < maximumFramesToRender) {
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

#if DEBUG
	[self logProcessingGraphDescription:_audioPlayerLog type:OS_LOG_TYPE_DEBUG];
#endif // DEBUG

	[_engine prepare];
	return YES;
}

#pragma mark - SFBAudioPlayerNodeDelegate

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingStarted:(id<SFBPCMDecoding>)decoder
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decodingStarted:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)])
		[_delegate audioPlayer:self decodingStarted:decoder];

	if(const auto flags = _flags.load(); (flags & eAudioPlayerFlagHavePendingDecoder) && !self.isPlaying) {
		_flags.fetch_or(eAudioPlayerFlagPendingDecoderBecameActive);
		self.nowPlaying = decoder;
	}
	_flags.fetch_and(~eAudioPlayerFlagHavePendingDecoder);
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingComplete:(id<SFBPCMDecoding>)decoder
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decodingComplete:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)])
		[_delegate audioPlayer:self decodingComplete:decoder];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingCanceled:(id<SFBPCMDecoding>)decoder framesRendered:(AVAudioFramePosition)framesRendered
{
	// It is not an error in this case if the player nodes don't match because when the
	// audio processing graph is reconfigured the existing player node may be replaced,
	// but any pending events will still be delivered before the instance is deallocated
#if 0
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decodingCanceled:framesRendered:");
		return;
	}
#endif

	objc_setAssociatedObject(decoder, &_decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

	if([_delegate respondsToSelector:@selector(audioPlayer:decodingCanceled:framesRendered:)])
		[_delegate audioPlayer:self decodingCanceled:decoder framesRendered:framesRendered];

	if(audioPlayerNode == _playerNode) {
		_flags.fetch_and(~eAudioPlayerFlagRenderingImminent & ~eAudioPlayerFlagPendingDecoderBecameActive);
		if(const auto flags = _flags.load(); !(flags & eAudioPlayerFlagHavePendingDecoder) && self.isStopped) {
			if(self.nowPlaying)
				self.nowPlaying = nil;
		}
	}
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:renderingWillStart:atHostTime:");
		return;
	}

	_flags.fetch_or(eAudioPlayerFlagRenderingImminent);

	dispatch_after(hostTime, audioPlayerNode.delegateQueue, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingWillStart:atHostTime:", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "Rendering started notification for %{public}@ arrived %.2f msec %s", decoder, static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif // DEBUG

		if(audioPlayerNode != self->_playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:renderingWillStart:atHostTime:");
			return;
		}

		if(!(self->_flags.load() & eAudioPlayerFlagPendingDecoderBecameActive))
			self.nowPlaying = decoder;
		self->_flags.fetch_and(~eAudioPlayerFlagRenderingImminent & ~eAudioPlayerFlagPendingDecoderBecameActive);

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[self->_delegate audioPlayer:self renderingStarted:decoder];
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[_delegate audioPlayer:self renderingWillStart:decoder atHostTime:hostTime];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillComplete:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:renderingWillComplete:atHostTime:");
		return;
	}

	dispatch_after(hostTime, audioPlayerNode.delegateQueue, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingWillComplete:atHostTime:", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "Rendering complete notification for %{public}@ arrived %.2f msec %s", decoder, static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif // DEBUG

		if(audioPlayerNode != self->_playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:renderingWillComplete:atHostTime:");
			return;
		}

		if(const auto flags = self->_flags.load(); !(flags & eAudioPlayerFlagRenderingImminent) && !(flags & eAudioPlayerFlagHavePendingDecoder) && self.internalDecoderQueueIsEmpty) {
			if(self.nowPlaying)
				self.nowPlaying = nil;
		}

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[self->_delegate audioPlayer:self renderingComplete:decoder];
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[_delegate audioPlayer:self renderingWillComplete:decoder atHostTime:hostTime];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode audioWillEndAtHostTime:(uint64_t)hostTime
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:audioWillEndAtHostTime:");
		return;
	}

	dispatch_after(hostTime, audioPlayerNode.delegateQueue, ^{
#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "End of audio notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif // DEBUG

		if(audioPlayerNode != self->_playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:audioWillEndAtHostTime:");
			return;
		}

		if(const auto flags = self->_flags.load(); (flags & eAudioPlayerFlagRenderingImminent) || (flags & eAudioPlayerFlagHavePendingDecoder))
			return;

		// Dequeue the next decoder
		if(id <SFBPCMDecoding> decoder = [self popDecoderFromInternalQueue]; decoder) {
			NSError *error = nil;
			if(![self configureForAndEnqueueDecoder:decoder forImmediatePlayback:NO error:&error]) {
				if(error && [self->_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
					[self->_delegate audioPlayer:self encounteredError:error];
			}
		}
		else if([self->_delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)])
			[self->_delegate audioPlayerEndOfAudio:self];
		else
			[self stop];
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:audioWillEndAtHostTime:)])
		[_delegate audioPlayer:self audioWillEndAtHostTime:hostTime];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode encounteredError:(NSError *)error
{
	if(audioPlayerNode != _playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:encounteredError:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
		[_delegate audioPlayer:self encounteredError:error];
}

@end
