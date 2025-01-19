//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <atomic>
#import <cmath>
#import <deque>
#import <mutex>

#import <objc/runtime.h>

#import <AVAudioFormat+SFBFormatTransformation.h>

#import <SFBUnfairLock.hpp>

#import "SFBAudioPlayer.h"
#import "SFBAudioPlayerNode+Internal.h"

#import "HostTimeUtilities.hpp"
#import "SFBAudioDecoder.h"
#import "SFBCStringForOSType.h"
#import "StringDescribingAVAudioFormat.h"

namespace {

/// Objective-C associated object key indicating if a decoder has been canceled
constexpr char _decoderIsCanceledKey = '\0';

using DecoderQueue = std::deque<id <SFBPCMDecoding>>;
const os_log_t _audioPlayerLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

/// Possible `SFBAudioPlayer` flag values
enum AudioPlayerFlags : unsigned int {
	/// Cached value of `_audioEngine.isRunning`
	eAudioPlayerFlagEngineIsRunning					= 1u << 0,
	/// Set if there is a decoder being enqueued on the player node that has not yet started decoding
	eAudioPlayerFlagHavePendingDecoder				= 1u << 1,
	/// Set if the pending decoder becomes active when the player is not playing
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
		os_log_error(_audioPlayerLog, "AudioObjectGetPropertyData (kAudioObjectPropertyName, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}
	return (__bridge_transfer NSString *)name;
}
#endif /* !TARGET_OS_IPHONE */

} /* namespace */

@interface SFBAudioPlayer ()
{
@private
	/// The underlying `AVAudioEngine` instance
	AVAudioEngine 			*_engine;
	/// The dispatch queue used to access `_engine`
	dispatch_queue_t		_engineQueue;
	/// The address of the player node driving the audio processing graph
	std::atomic<uintptr_t> 	_playerNodePtr;
	static_assert(std::atomic<uintptr_t>::is_always_lock_free, "Lock-free std::atomic<uintptr_t> required");
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
- (BOOL)pushDecoderToInternalQueue:(id <SFBPCMDecoding>)decoder;
/// Removes and returns the first decoder from the internal decoder queue
- (nullable id <SFBPCMDecoding>)popDecoderFromInternalQueue;
/// Called to process `AVAudioEngineConfigurationChangeNotification`
- (void)handleAudioEngineConfigurationChange:(NSNotification *)notification;
#if TARGET_OS_IPHONE
/// Called to process `AVAudioSessionInterruptionNotification`
- (void)handleAudioSessionInterruption:(NSNotification *)notification;
#endif /* TARGET_OS_IPHONE */
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
#endif /* TARGET_OS_IPHONE */
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
	if(forImmediatePlayback) {
		_flags.fetch_or(eAudioPlayerFlagHavePendingDecoder, std::memory_order_acq_rel);
		const auto result = [self configureForAndEnqueueDecoder:decoder forImmediatePlayback:YES error:error];
		if(!result)
			_flags.fetch_and(~eAudioPlayerFlagHavePendingDecoder, std::memory_order_acq_rel);
		return result;
	}

	// Obtain a reference to the player node
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));

	// To preserve the order of enqueued decoders, when the internal queue is not empty
	// enqueue all decoders there regardless of format compability with playerNode
	// This prevents incorrect playback order arising from the scenario where
	// decoders A and AA have formats supported by playerNode and decoder B does not;
	// bypassing the internal queue for supported formats when enqueueing A, B, AA
	// would result in playback order A, AA, B
	if(self.internalDecoderQueueIsEmpty && [playerNode supportsFormat:decoder.processingFormat]) {
		_flags.fetch_or(eAudioPlayerFlagHavePendingDecoder, std::memory_order_acq_rel);
		// Enqueuing is expected to succeed since the formats are compatible
		return [playerNode enqueueDecoder:decoder error:error];
	}
	// If the internal queue is not empty or playerNode doesn't support
	// the decoder's processing format add the decoder to our internal queue
	else {
		if(![self pushDecoderToInternalQueue:decoder]) {
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
			return NO;
		}
		return YES;
	}
}

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode supportsFormat:format];
}

- (void)clearQueue
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	[playerNode clearQueue];
	[self clearInternalDecoderQueue];
}

- (BOOL)queueIsEmpty
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.queueIsEmpty && self.internalDecoderQueueIsEmpty;
}

#pragma mark - Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if((_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && playerNode.isPlaying)
		return YES;

	__block BOOL engineStarted = NO;
	__block NSError *err = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		engineStarted = [_engine startAndReturnError:&err];
		if(engineStarted) {
			_flags.fetch_or(eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
			[playerNode play];
		}
		else
			_flags.fetch_and(~eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
	});

	if(!engineStarted) {
		os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", err);
		if(error)
			*error = err;
		return NO;
	}

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePlaying, @"Incorrect playback state in -playReturningError:");
#endif /* DEBUG */

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];

	return YES;
}

- (void)pause
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(!((_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && playerNode.isPlaying))
		return;

	[playerNode pause];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePaused, @"Incorrect playback state in -pause");
#endif /* DEBUG */

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
}

- (void)resume
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(!((_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && !playerNode.isPlaying))
		return;

	[playerNode play];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStatePlaying, @"Incorrect playback state in -resume");
#endif /* DEBUG */

	if([_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[_delegate audioPlayer:self playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
}

- (void)stop
{
	if(!(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning))
		return;

	dispatch_async_and_wait(_engineQueue, ^{
		[_engine stop];
		_flags.fetch_and(~eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
		[playerNode stop];
	});

	[self clearInternalDecoderQueue];

#if DEBUG
	NSAssert(self.playbackState == SFBAudioPlayerPlaybackStateStopped, @"Incorrect playback state in -stop");
#endif /* DEBUG */

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
		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
		[playerNode reset];
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
		NSAssert(static_cast<bool>(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) == isRunning, @"Cached value for _engine.isRunning invalid");
#endif /* DEBUG */
	});
	return isRunning;
}

- (BOOL)playerNodeIsPlaying
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.isPlaying;
}

- (SFBAudioPlayerPlaybackState)playbackState
{
	if(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) {
		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
		return playerNode.isPlaying ? SFBAudioPlayerPlaybackStatePlaying : SFBAudioPlayerPlaybackStatePaused;
	}
	else
		return SFBAudioPlayerPlaybackStateStopped;
}

- (BOOL)isPlaying
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return (_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && playerNode.isPlaying;
}

- (BOOL)isPaused
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return (_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && !playerNode.isPlaying;
}

- (BOOL)isStopped
{
	return !(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning);
}

- (BOOL)isReady
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.isReady;
}

- (id<SFBPCMDecoding>)currentDecoder
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.currentDecoder;
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
#endif /* DEBUG */
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

- (SFBPlaybackPosition)playbackPosition
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.playbackPosition;
}

- (NSTimeInterval)currentTime
{
	return self.playbackTime.currentTime;
}

- (NSTimeInterval)totalTime
{
	return self.playbackTime.totalTime;
}

- (SFBPlaybackTime)playbackTime
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.playbackTime;
}

- (BOOL)getPlaybackPosition:(SFBPlaybackPosition *)playbackPosition andTime:(SFBPlaybackTime *)playbackTime
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode getPlaybackPosition:playbackPosition andTime:playbackTime];
}

#pragma mark - Seeking

- (BOOL)seekForward
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekForward:3];
}

- (BOOL)seekBackward
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekBackward:3];
}

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekForward:secondsToSkip];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekBackward:secondsToSkip];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekToTime:timeInSeconds];
}

- (BOOL)seekToPosition:(double)position
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekToPosition:position];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return [playerNode seekToFrame:frame];
}

- (BOOL)supportsSeeking
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	return playerNode.supportsSeeking;
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

#endif /* !TARGET_OS_IPHONE */

#pragma mark - AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_async_and_wait(_engineQueue, ^{
		block(_engine);
		// SFBAudioPlayer requires that the mixer node be connected to the output node
		NSAssert([_engine inputConnectionPointForNode:_engine.outputNode inputBus:0].node == _engine.mainMixerNode, @"Illegal AVAudioEngine configuration");
		NSAssert(_engine.isRunning == static_cast<bool>(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning), @"AVAudioEngine may not be started or stopped outside of SFBAudioPlayer");
	});
}

- (SFBAudioPlayerNode *)playerNode
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
#if DEBUG
	NSAssert(playerNode != nil, @"Precondition failed: _playerNodePtr contains nullptr");
#endif /* DEBUG */
	return playerNode;
}

#pragma mark - Debugging

-(void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type
{
	dispatch_async(_engineQueue, ^{
		NSMutableString *string = [NSMutableString stringWithFormat:@"%@ audio processing graph:\n", self];

		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(self->_playerNodePtr.load(std::memory_order_acquire));
		AVAudioEngine *engine = self->_engine;

		AVAudioFormat *inputFormat = playerNode.renderingFormat;
		[string appendFormat:@"↓ rendering\n    %@\n", SFB::StringDescribingAVAudioFormat(inputFormat)];

		AVAudioFormat *outputFormat = [playerNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", playerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", playerNode];

		AVAudioConnectionPoint *connectionPoint = [[engine outputConnectionPointsForNode:playerNode outputBus:0] firstObject];
		while(connectionPoint.node != engine.mainMixerNode) {
			inputFormat = [connectionPoint.node inputFormatForBus:connectionPoint.bus];
			outputFormat = [connectionPoint.node outputFormatForBus:connectionPoint.bus];
			if(![outputFormat isEqual:inputFormat])
				[string appendFormat:@"→ %@\n    %@\n", connectionPoint.node, SFB::StringDescribingAVAudioFormat(outputFormat)];

			else
				[string appendFormat:@"→ %@\n", connectionPoint.node];

			connectionPoint = [[engine outputConnectionPointsForNode:connectionPoint.node outputBus:0] firstObject];
		}

		inputFormat = [engine.mainMixerNode inputFormatForBus:0];
		outputFormat = [engine.mainMixerNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", engine.mainMixerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", engine.mainMixerNode];

		inputFormat = [engine.outputNode inputFormatForBus:0];
		outputFormat = [engine.outputNode outputFormatForBus:0];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@]", engine.outputNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@", engine.outputNode];

#if !TARGET_OS_IPHONE
		[string appendFormat:@"\n↓ \"%@\"", AudioDeviceName(engine.outputNode.AUAudioUnit)];
#endif /* !TARGET_OS_IPHONE */

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
	_queuedDecoders.resize(0);
}

- (BOOL)pushDecoderToInternalQueue:(id <SFBPCMDecoding>)decoder
{
	try {
		std::lock_guard<SFB::UnfairLock> lock(_queueLock);
		_queuedDecoders.push_back(decoder);
	}
	catch(const std::exception& e) {
		os_log_error(_audioPlayerLog, "Error pushing %{public}@ to _queuedDecoders: %{public}s", decoder, e.what());
		return NO;
	}

	return YES;
}

- (id <SFBPCMDecoding>)popDecoderFromInternalQueue
{
	id <SFBPCMDecoding> decoder = nil;
	std::lock_guard<SFB::UnfairLock> lock(_queueLock);
	if(!_queuedDecoders.empty()) {
		decoder = _queuedDecoders.front();
		_queuedDecoders.pop_front();
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
	const bool engineWasRunning = _flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning;
	_flags.fetch_and(~eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);

	// Attempt to preserve the playback state
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	const BOOL playerNodeWasPlaying = playerNode.isPlaying;

	// AVAudioEngine posts this notification from a dedicated queue
	__block BOOL success;
	__block NSError *error = nil;
	dispatch_async_and_wait(_engineQueue, ^{
		[playerNode pause];

		// Force an update of the audio processing graph
		success = [self configureProcessingGraphForFormat:playerNode.renderingFormat forceUpdate:YES];
		if(!success) {
			os_log_error(_audioPlayerLog, "Unable to create audio processing graph for %{public}@", SFB::StringDescribingAVAudioFormat(playerNode.renderingFormat));
			error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			return;
		}

		// Restart AVAudioEngine if previously running
		if(engineWasRunning) {
			BOOL engineStarted = [_engine startAndReturnError:&error];
			if(!engineStarted) {
				os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", error);
				return;
			}

			_flags.fetch_or(eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);

			// Restart the player node if needed
			if(playerNodeWasPlaying)
				[playerNode play];
		}
	});

	// Success in this context means the graph is in a working state, not that the engine was restarted successfully
	if(!success) {
		if([_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
			[_delegate audioPlayer:self encounteredError:error];
		return;
	}

	if((engineWasRunning != static_cast<bool>(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) || playerNodeWasPlaying != playerNode.isPlaying) && [_delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
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
			// However, eAudioPlayerFlagEngineIsRunning indicates if the engine was running before the interruption
			if(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) {
				_flags.fetch_and(~eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
				dispatch_async_and_wait(_engineQueue, ^{
					NSError *error = nil;
					BOOL engineStarted = [_engine startAndReturnError:&error];
					if(engineStarted)
						_flags.fetch_or(eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
					else
						os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", error);
				});
			}
			break;

		default:
			os_log_error(_audioPlayerLog, "Unknown value %lu for AVAudioSessionInterruptionTypeKey", static_cast<unsigned long>(interruptionType));
			break;
	}
}
#endif /* TARGET_OS_IPHONE */

- (BOOL)configureForAndEnqueueDecoder:(id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	// Attempt to preserve the playback state
	const bool engineWasRunning = _flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning;
	SFBAudioPlayerNode *originalPlayerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	const BOOL playerNodeWasPlaying = originalPlayerNode.isPlaying;

	__block BOOL success = YES;

	// If the current SFBAudioPlayerNode doesn't support the decoder's format (required for gapless join),
	// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
	if(auto format = decoder.processingFormat; ![originalPlayerNode supportsFormat:format])
		dispatch_async_and_wait(_engineQueue, ^{
			success = [self configureProcessingGraphForFormat:format forceUpdate:NO];
		});

	if(!success) {
		if(error)
			*error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
		if(self.nowPlaying)
			self.nowPlaying = nil;
		return NO;
	}

	// The player node driving the audio processing graph may have been swapped out in -configureProcessingGraphForFormat:forceUpdate:
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));

	if(forImmediatePlayback) {
		[self clearInternalDecoderQueue];
		success = [playerNode resetAndEnqueueDecoder:decoder error:error];
	}
	else
		success = [playerNode enqueueDecoder:decoder error:error];

	// Failure is unlikely since the audio processing graph was reconfigured for the decoder's processing format
	if(!success) {
		if(self.nowPlaying)
			self.nowPlaying = nil;
		return NO;
	}

	// AVAudioEngine may have been stopped in `-configureProcessingGraphForFormat:forceUpdate:`
	// If this is the case and it was previously running, restart it and the player node
	// as appropriate
	if(engineWasRunning && !(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning)) {
		__block BOOL engineStarted = NO;
		__block NSError *err = nil;
		dispatch_async_and_wait(_engineQueue, ^{
			engineStarted = [_engine startAndReturnError:&err];
			if(engineStarted) {
				_flags.fetch_or(eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);
				if(playerNodeWasPlaying)
					[playerNode play];
			}
		});

		if(!engineStarted) {
			os_log_error(_audioPlayerLog, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return NO;
		}
	}

#if DEBUG
	NSAssert(engineWasRunning == static_cast<bool>(_flags.load(std::memory_order_acquire) & eAudioPlayerFlagEngineIsRunning) && playerNodeWasPlaying == playerNode.isPlaying, @"Incorrect playback state in -configureForAndEnqueueDecoder:forImmediatePlayback:error:");
#endif /* DEBUG */

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

	// _playerNodePtr may be nullptr since this method is called from -init
	SFBAudioPlayerNode *oldPlayerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	const auto formatsEqual = [format isEqual:oldPlayerNode.renderingFormat];
	if(formatsEqual && !forceUpdate)
		return YES;

	// Even if the engine isn't running, call stop to force release of any render resources
	// Empirically this is necessary when transitioning between formats with different
	// channel counts, although it seems that it shouldn't be
	[_engine stop];
	_flags.fetch_and(~eAudioPlayerFlagEngineIsRunning, std::memory_order_acq_rel);

	if(oldPlayerNode.isPlaying)
		[oldPlayerNode stop];

	// Avoid creating a new SFBAudioPlayerNode if not necessary
	SFBAudioPlayerNode *newPlayerNode = nil;
	if(!formatsEqual) {
		newPlayerNode = [[SFBAudioPlayerNode alloc] initWithFormat:format];
		if(!newPlayerNode) {
			os_log_error(_audioPlayerLog, "Unable to create SFBAudioPlayerNode with format %{public}@", SFB::StringDescribingAVAudioFormat(format));
			return NO;
		}

		newPlayerNode.delegate = self;
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

	if(newPlayerNode) {
		AVAudioConnectionPoint *playerNodeOutputConnectionPoint = nil;
		if(oldPlayerNode) {
			playerNodeOutputConnectionPoint = [[_engine outputConnectionPointsForNode:oldPlayerNode outputBus:0] firstObject];
			[_engine detachNode:oldPlayerNode];
		}

		[_engine attachNode:newPlayerNode];

		// In general storing the address of an ObjC object managed by GC is a Very Bad Idea™
		//
		// In this case, newPlayerNode is retained by _engine (since it is attached)
		// and before detaching a strong reference is stored in oldPlayerNode to ensure
		// an appropriate lifetime
		_playerNodePtr.exchange(reinterpret_cast<uintptr_t>((__bridge void *)newPlayerNode), std::memory_order_release);
//		_playerNodePtr.store(reinterpret_cast<uintptr_t>((__bridge void *)newPlayerNode), std::memory_order_release);

		// When an audio player node is deallocated the destructor synchronously waits
		// for decoder cancelation (if there is an active decoder) and then for any
		// final events to be processed and delegate messages sent.
		// The potential therefore exists to block the calling thread for a perceptible amount
		// of time, especially if the delegate callouts take longer than ideal.
		//
		// In my measurements the baseline with an empty delegate implementation of
		// -audioPlayer:decoderCanceled:framesRendered: seems to be around 100 µsec
		//
		// Assuming there are no external references to the audio player node,
		// setting it to nil here sends -dealloc
		oldPlayerNode = nil;

		// Reconnect the player node to the next node in the processing chain
		// This is the mixer node in the default configuration, but additional nodes may
		// have been inserted between the player and mixer nodes. In this case allow the delegate
		// to make any necessary adjustments based on the format change if desired.
		if(playerNodeOutputConnectionPoint && playerNodeOutputConnectionPoint.node != mixerNode) {
			if([_delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
				AVAudioNode *node = [_delegate audioPlayer:self reconfigureProcessingGraph:_engine withFormat:format];
				// Ensure the delegate returned a valid node
				NSAssert(node != nil, @"nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
				[_engine connect:newPlayerNode to:node format:format];
			}
			else
				[_engine connect:newPlayerNode to:playerNodeOutputConnectionPoint.node format:format];
		}
		else
			[_engine connect:newPlayerNode to:mixerNode format:format];
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

		if(auto audioUnit = newPlayerNode.AUAudioUnit; audioUnit.maximumFramesToRender < maximumFramesToRender) {
			const auto renderResourcesAllocated = audioUnit.renderResourcesAllocated;
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
#endif /* DEBUG */

	[_engine prepare];
	return YES;
}

#pragma mark - SFBAudioPlayerNodeDelegate

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingStarted:(id<SFBPCMDecoding>)decoder
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decodingStarted:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)])
		[_delegate audioPlayer:self decodingStarted:decoder];

	if(const auto flags = _flags.load(std::memory_order_acquire); (flags & eAudioPlayerFlagHavePendingDecoder) && !((flags & eAudioPlayerFlagEngineIsRunning) && audioPlayerNode->_impl->IsPlaying()) && audioPlayerNode->_impl->CurrentDecoder() == decoder) {
		_flags.fetch_or(eAudioPlayerFlagPendingDecoderBecameActive, std::memory_order_acq_rel);
		self.nowPlaying = decoder;
	}
	_flags.fetch_and(~eAudioPlayerFlagHavePendingDecoder, std::memory_order_acq_rel);
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingComplete:(id<SFBPCMDecoding>)decoder
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decodingComplete:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)])
		[_delegate audioPlayer:self decodingComplete:decoder];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:renderingWillStart:atHostTime:");
		return;
	}

	dispatch_after(hostTime, audioPlayerNode->_impl->mEventProcessingQueue, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingWillStart:atHostTime:", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "Rendering started notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(audioPlayerNode != playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:renderingWillStart:atHostTime:");
			return;
		}

		if(!(self->_flags.load(std::memory_order_acquire) & eAudioPlayerFlagPendingDecoderBecameActive))
			self.nowPlaying = decoder;
		self->_flags.fetch_and(~eAudioPlayerFlagPendingDecoderBecameActive, std::memory_order_acq_rel);

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[self->_delegate audioPlayer:self renderingStarted:decoder];
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[_delegate audioPlayer:self renderingWillStart:decoder atHostTime:hostTime];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingDecoder:(id<SFBPCMDecoding>)decoder willChangeToDecoder:(id<SFBPCMDecoding>)nextDecoder atHostTime:(uint64_t)hostTime
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:renderingDecoder:willChangeToDecoder:atHostTime:");
		return;
	}

	dispatch_after(hostTime, audioPlayerNode->_impl->mEventProcessingQueue, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingDecoder:willChangeToDecoder:atHostTime:", decoder);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(nextDecoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingDecoder:willChangeToDecoder:atHostTime:", nextDecoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "Rendering decoder changed notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(audioPlayerNode != playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:renderingDecoder:willChangeToDecoder:atHostTime:");
			return;
		}

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[self->_delegate audioPlayer:self renderingComplete:decoder];

		self.nowPlaying = nextDecoder;

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[self->_delegate audioPlayer:self renderingStarted:nextDecoder];
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[_delegate audioPlayer:self renderingWillComplete:decoder atHostTime:hostTime];

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[_delegate audioPlayer:self renderingWillStart:nextDecoder atHostTime:hostTime];
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillComplete:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:renderingWillComplete:atHostTime:");
		return;
	}

	dispatch_after(hostTime, audioPlayerNode->_impl->mEventProcessingQueue, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(_audioPlayerLog, "%{public}@ canceled after receiving -audioPlayerNode:renderingWillComplete:atHostTime:", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / audioPlayerNode.renderingFormat.sampleRate);
		if(delta > tolerance)
			os_log_debug(_audioPlayerLog, "Rendering complete notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(audioPlayerNode != playerNode) {
			os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance following -audioPlayerNode:renderingWillComplete:atHostTime:");
			return;
		}

		if([self->_delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[self->_delegate audioPlayer:self renderingComplete:decoder];

		if(self->_flags.load(std::memory_order_acquire) & eAudioPlayerFlagHavePendingDecoder)
			return;

		// Dequeue the next decoder
		if(id <SFBPCMDecoding> decoder = [self popDecoderFromInternalQueue]; decoder) {
			NSError *error = nil;
			if(![self configureForAndEnqueueDecoder:decoder forImmediatePlayback:NO error:&error]) {
				if(error && [self->_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
					[self->_delegate audioPlayer:self encounteredError:error];
			}
		}
		// End of audio
		else {
#if DEBUG
			os_log_debug(_audioPlayerLog, "End of audio reached");
#endif /* DEBUG */

			self.nowPlaying = nil;

			if([self->_delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)])
				[self->_delegate audioPlayerEndOfAudio:self];
			else
				[self stop];
		}
	});

	if([_delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[_delegate audioPlayer:self renderingWillComplete:decoder atHostTime:hostTime];
}


- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decoderCanceled:(id<SFBPCMDecoding>)decoder framesRendered:(AVAudioFramePosition)framesRendered
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	// It is not an error in this case if the player nodes don't match because when the
	// audio processing graph is reconfigured the existing player node may be replaced,
	// but any pending events will still be delivered before the instance is deallocated
#if false
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:decoderCanceled:framesRendered:");
		return;
	}
#endif /* false */

	objc_setAssociatedObject(decoder, &_decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

	if([_delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)])
		[_delegate audioPlayer:self decoderCanceled:decoder framesRendered:framesRendered];

	if(audioPlayerNode == playerNode) {
		_flags.fetch_and(~eAudioPlayerFlagPendingDecoderBecameActive, std::memory_order_acq_rel);
		if(const auto flags = _flags.load(std::memory_order_acquire); !(flags & eAudioPlayerFlagHavePendingDecoder) && !(flags & eAudioPlayerFlagEngineIsRunning))
			if(self.nowPlaying)
				self.nowPlaying = nil;
	}
}

- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode encounteredError:(NSError *)error
{
	SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)reinterpret_cast<void *>(_playerNodePtr.load(std::memory_order_acquire));
	if(audioPlayerNode != playerNode) {
		os_log_fault(_audioPlayerLog, "Unexpected SFBAudioPlayerNode instance in -audioPlayerNode:encounteredError:");
		return;
	}

	if([_delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
		[_delegate audioPlayer:self encounteredError:error];
}

@end
