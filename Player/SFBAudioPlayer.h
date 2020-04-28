/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBAudioPlayerNode.h"
#import "SFBAudioOutputDevice.h"

NS_ASSUME_NONNULL_BEGIN

typedef SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerPlaybackPosition;
typedef SFBAudioPlayerNodePlaybackTime SFBAudioPlayerPlaybackTime;

// Event types
typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine);

/// The possible playback states for \c SFBAudioPlayer
typedef NS_ENUM(NSUInteger, SFBAudioPlayerPlaybackState) {
	SFBAudioPlayerPlaybackStatePlaying		= 0,	///<  \c SFBAudioPlayer.engineIsRunning  and \c SFBAudioPlayer.playerNodeIsPlaying
	SFBAudioPlayerPlaybackStatePaused		= 1,	///<  \c SFBAudioPlayer.engineIsRunning  and \c !SFBAudioPlayer.playerNodeIsPlaying
	SFBAudioPlayerPlaybackStateStopped		= 2		///<  \c !SFBAudioPlayer.engineIsRunning
};

/// @brief An audio player wrapping an \c AVAudioEngine processing graph supplied by \c SFBAudioPlayerNode
///
/// \c SFBAudioPlayer supports gapless playback for audio with the same sample rate and number of channels.
/// For audio with different sample rates or channels, the audio processing graph is automatically reconfigured.
///
/// An \c SFBAudioPlayer may be in one of three playback states: playing, paused, or stopped. These states are
/// based on whether the underlying \c AVAudioEngine is running (\c SFBAudioPlayer.engineIsRunning)
/// and the \c SFBAudioPlayerNode is playing (\c SFBAudioPlayer.playerNodeIsPlaying).
NS_SWIFT_NAME(AudioPlayer) @interface SFBAudioPlayer : NSObject

#pragma mark - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format;

- (void)skipToNext;
- (void)clearQueue;

@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
- (void)pause;
- (void)stop;
- (BOOL)playPauseReturningError:(NSError **)error NS_SWIFT_NAME(playPause());

#pragma mark - Player State

@property (nonatomic, readonly) BOOL engineIsRunning; ///< Returns \c YES if the \c AVAudioEngine is running
@property (nonatomic, readonly) BOOL playerNodeIsPlaying; ///< Returns \c YES if the \c SFBAudioPlayerNode is playing

@property (nonatomic, readonly) SFBAudioPlayerPlaybackState playbackState; ///< Returns the current playback state
@property (nonatomic, readonly) BOOL isPlaying; ///< Returns \c YES if \c engineIsRunning and \c playerNodeIsPlaying
@property (nonatomic, readonly) BOOL isPaused; ///< Returns \c YES if \c engineIsRunning and \c !playerNodeIsPlaying
@property (nonatomic, readonly) BOOL isStopped; ///< Returns \c NO if \c engineIsRunning

@property (nonatomic, nullable, readonly) NSURL *url; ///< Returns the url of the  rendering decoder's  input source  or \c nil if none
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> decoder; ///< Returns the  rendering decoder  or \c nil if none. @warning Do not change any properties of the returned object

#pragma mark - Playback Properties

@property (nonatomic, readonly) AVAudioFramePosition framePosition;
@property (nonatomic, readonly) AVAudioFramePosition frameLength;
@property (nonatomic, readonly) SFBAudioPlayerPlaybackPosition playbackPosition NS_SWIFT_NAME(position);

@property (nonatomic, readonly) NSTimeInterval currentTime;
@property (nonatomic, readonly) NSTimeInterval totalTime;
@property (nonatomic, readonly) SFBAudioPlayerPlaybackTime playbackTime NS_SWIFT_NAME(time);

- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerPlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

#pragma mark - Seeking

- (BOOL)seekForward;
- (BOOL)seekBackward;

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(forward:));
- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(backward:));

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds NS_SWIFT_NAME(seek(time:));
- (BOOL)seekToPosition:(float)position NS_SWIFT_NAME(seek(position:));
- (BOOL)seekToFrame:(AVAudioFramePosition)frame NS_SWIFT_NAME(seek(frame:));

@property (nonatomic, readonly) BOOL supportsSeeking;

#pragma mark - Volume Control

/// Returns \c kHALOutputParam_Volume on channel \c 0 for \c AVAudioEngine.outputNode.audioUnit or \c -1 on error
@property (nonatomic, readonly) float volume;
- (BOOL)setVolume:(float)volume error:(NSError **)error;

/// Returns \c kHALOutputParam_Volume on \c channel for \c AVAudioEngine.outputNode.audioUnit or \c -1 on error
- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error;

#pragma mark - Player Event Callbacks

@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingCanceledNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingFinishedNotificationHandler;

#pragma mark - Output Device

@property (nonatomic, nonnull, readonly) SFBAudioOutputDevice *outputDevice;
- (BOOL)setOutputDevice:(SFBAudioOutputDevice *)outputDevice error:(NSError **)error;

#pragma mark - AVAudioEngine Access

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block;

@end

NS_ASSUME_NONNULL_END
