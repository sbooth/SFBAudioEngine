/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBAudioPlayerNode.h"
#if TARGET_OS_OSX
#import "SFBAudioOutputDevice.h"
#endif

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerDelegate;

/// Posted when the configuration of the underlying \c AVAudioEngine changes
/// @note Use this instead of \c AVAudioEngineConfigurationChangeNotification
extern const NSNotificationName SFBAudioPlayerAVAudioEngineConfigurationChangeNotification;

/// Playback position information for \c SFBAudioPlayerNode
typedef SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerPlaybackPosition NS_SWIFT_NAME(AudioPlayer.PlaybackPosition);
/// Playback time information for \c SFBAudioPlayerNode
typedef SFBAudioPlayerNodePlaybackTime SFBAudioPlayerPlaybackTime NS_SWIFT_NAME(AudioPlayer.PlaybackTime);

/// A block accepting a single \c AVAudioEngine parameter
typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine) NS_SWIFT_NAME(AudioPlayer.AVAudioEngineClosure);

/// The possible playback states for \c SFBAudioPlayer
typedef NS_ENUM(NSUInteger, SFBAudioPlayerPlaybackState) {
	/// \c SFBAudioPlayer.engineIsRunning  and \c SFBAudioPlayer.playerNodeIsPlaying
	SFBAudioPlayerPlaybackStatePlaying		= 0,
	/// \c SFBAudioPlayer.engineIsRunning  and \c !SFBAudioPlayer.playerNodeIsPlaying
	SFBAudioPlayerPlaybackStatePaused		= 1,
	/// \c !SFBAudioPlayer.engineIsRunning
	SFBAudioPlayerPlaybackStateStopped		= 2
} NS_SWIFT_NAME(AudioPlayer.PlaybackState);

/// An audio player wrapping an \c AVAudioEngine processing graph supplied by \c SFBAudioPlayerNode
///
/// \c SFBAudioPlayer supports gapless playback for audio with the same sample rate and number of channels.
/// For audio with different sample rates or channels, the audio processing graph is automatically reconfigured.
///
/// An \c SFBAudioPlayer may be in one of three playback states: playing, paused, or stopped. These states are
/// based on whether the underlying \c AVAudioEngine is running (\c SFBAudioPlayer.engineIsRunning)
/// and the \c SFBAudioPlayerNode is playing (\c SFBAudioPlayer.playerNodeIsPlaying).
NS_SWIFT_NAME(AudioPlayer) @interface SFBAudioPlayer : NSObject <SFBAudioPlayerNodeDelegate>

#pragma mark - Playlist Management

/// Cancels the current decoder, clears any queued decoders, creates and enqueues a decoder, and starts playback
/// @note This is equivalent to creating an \c SFBAudioDecoder object for \c url and passing that object to \c -playDecoder:error:
/// @param url The URL to play
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued and playback started successfully
- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
/// Cancels the current decoder, clears any queued decoders, enqueues a decoder, and starts playback
/// @note This is equivalent to \c -reset followed by \c -enqueueDecoder:error: and \c -playReturningError:
/// @param decoder The decoder to play
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued and playback started successfully
- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

/// Creates and enqueues a decoder for subsequent playback
/// @note This is equivalent to creating an \c SFBAudioDecoder object for \c url and passing that object to \c -enqueueDecoder:error:
/// @param url The URL to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Enqueues a decoder for subsequent playback
/// @param decoder The decoder to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

/// Returns \c YES if audio with \c format will be played gaplessly
- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format;

/// Cancels the current decoder and dequeues the next decoder for playback
- (void)skipToNext;
/// Empties the decoder queue
- (void)clearQueue;

/// Returns \c YES if the decoder queue is empty
@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

/// Starts the underlying \c AVAudioEngine and plays the \c SFBAudioPlayerNode
- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
/// Pauses the \c SFBAudioPlayerNode
- (void)pause;
/// Stops both the underlying \c AVAudioEngine and \c SFBAudioPlayerNode
- (void)stop;
/// Toggles the player between playing and paused
- (BOOL)togglePlayPauseReturningError:(NSError **)error NS_SWIFT_NAME(togglePlayPause());

/// Resets both the underlying \c AVAudioEngine and \c SFBAudioPlayerNode
- (void)reset;

#pragma mark - Player State

 /// Returns \c YES if the \c AVAudioEngine is running
@property (nonatomic, readonly) BOOL engineIsRunning;
/// Returns \c YES if the \c SFBAudioPlayerNode is playing
@property (nonatomic, readonly) BOOL playerNodeIsPlaying;

/// Returns the current playback state
@property (nonatomic, readonly) SFBAudioPlayerPlaybackState playbackState;
/// Returns \c YES if \c engineIsRunning and \c playerNodeIsPlaying
@property (nonatomic, readonly) BOOL isPlaying;
/// Returns \c YES if \c engineIsRunning and \c !playerNodeIsPlaying
@property (nonatomic, readonly) BOOL isPaused;
/// Returns \c NO if \c engineIsRunning
@property (nonatomic, readonly) BOOL isStopped;

/// Returns \c YES if a decoder is available to supply audio for the next render cycle
@property (nonatomic, readonly) BOOL isReady;
/// Returns the decoder supplying the earliest audio frame for the next render cycle or \c nil if none
/// @warning Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> currentDecoder;

#pragma mark - Playback Properties

/// Returns the frame position in the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) AVAudioFramePosition framePosition;
/// Returns the frame length of the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) AVAudioFramePosition frameLength;
/// Returns the playback position in the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerPlaybackPosition playbackPosition NS_SWIFT_NAME(position);

/// Returns the current time in the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) NSTimeInterval currentTime;
/// Returns the total time of the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) NSTimeInterval totalTime;
/// Returns the playback time in the current decoder or \c {-1, \c -1} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerPlaybackTime playbackTime NS_SWIFT_NAME(time);

/// Retrieves the playback position and time
/// @param playbackPosition An optional pointer to an \c SFBAudioPlayerNodePlaybackPosition struct to receive playback position information
/// @param playbackTime An optional pointer to an \c SFBAudioPlayerNodePlaybackTime struct to receive playback time information
/// @return \c NO if the current decoder is \c nil
- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerPlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

#pragma mark - Seeking

/// Seeks forward in the current decoder by \c 3 seconds
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekForward;
/// Seeks backward in the current decoder by \c 3 seconds
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekBackward;

/// Seeks forward in the current decoder by the specified number of seconds
/// @param secondsToSkip The number of seconds to skip forward
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekForward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(forward:));
/// Seeks backward in the current decoder by the specified number of seconds
/// @param secondsToSkip The number of seconds to skip backward
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(backward:));

/// Seeks to the specified time in the current decoder
/// @param timeInSeconds The desired time in seconds
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds NS_SWIFT_NAME(seek(time:));
/// Seeks to the specified positioni n the current decoder
/// @param position The desired position in the interval \c [0, 1)
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekToPosition:(float)position NS_SWIFT_NAME(seek(position:));
/// Seeks to the specified audio frame in the current decoder
/// @param frame The desired audio frame
/// @return \c NO if the current decoder is \c nil
- (BOOL)seekToFrame:(AVAudioFramePosition)frame NS_SWIFT_NAME(seek(frame:));

/// Returns \c YES if the current decoder supports seeking
@property (nonatomic, readonly) BOOL supportsSeeking;

#if TARGET_OS_OSX

#pragma mark - Volume Control

/// Returns \c kHALOutputParam_Volume on channel \c 0 for \c AVAudioEngine.outputNode.audioUnit or \c NaN on error
@property (nonatomic, readonly) float volume;
/// Sets \c kHALOutputParam_Volume on channel \c 0 for \c AVAudioEngine.outputNode.audioUnit
/// @param volume The desired volume
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was successfully set
- (BOOL)setVolume:(float)volume error:(NSError **)error;

/// Returns \c kHALOutputParam_Volume on \c channel for \c AVAudioEngine.outputNode.audioUnit or \c NaN on error
- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
/// Sets \c kHALOutputParam_Volume on channel \c 0 for \c AVAudioEngine.outputNode.audioUnit
/// @param volume The desired volume
/// @param channel The channel to adjust
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was successfully set
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error;

#pragma mark - Output Device

/// Returns the output device for \c AVAudioEngine.outputNode
@property (nonatomic, nonnull, readonly) SFBAudioOutputDevice *outputDevice;
/// Sets the output device for \c AVAudioEngine.outputNode
/// @param outputDevice The desired output device
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the output device was successfully set
- (BOOL)setOutputDevice:(SFBAudioOutputDevice *)outputDevice error:(NSError **)error;

#endif

#pragma mark - Delegate

/// An optional delegate
@property (nonatomic, nullable, weak) id<SFBAudioPlayerDelegate> delegate;

#pragma mark - AVAudioEngine Access

/// Peforms an operation on the underlying \c AVAudioEngine
/// @note Graph modifications may only be made between \c playerNode and \c engine.mainMixerNode
/// @param block A block performing operations on the underlying \c AVAudioEngine
- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block;
/// Returns the \c SFBAudioPlayerNode that is the source audio processing graph
@property (nonatomic, nonnull, readonly) SFBAudioPlayerNode *playerNode;

@end

#pragma mark - SFBAudioPlayerDelegate

NS_SWIFT_NAME(AudioPlayer.Delegate) @protocol SFBAudioPlayerDelegate <NSObject>
@optional
/// Called before \c audioPlayer decodes the first frame of audio from \c decoder
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingStarted:(id<SFBPCMDecoding>)decoder;
/// Called after \c audioPlayer decodes the final frame of audio from \c decoder
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingComplete:(id<SFBPCMDecoding>)decoder;
/// Called when \c audioPlayer cancels decoding for \c decoder
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingCanceled:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that \c audioPlayer will begin rendering audio from \c decoder at \c hostTime
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillStart:at:));
/// Called when \c audioPlayer renders the first frame of audio from \c decoder
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingStarted:(id<SFBPCMDecoding>)decoder;
/// Called when \c audioPlayer renders the final frame of audio from \c decoder
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingComplete:(id<SFBPCMDecoding>)decoder;
/// Called when \c audioPlayer encounters an asynchronous error
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer encounteredError:(NSError *)error;
/// Called when \c audioPlayer has completed rendered for all available decoders
- (void)audioPlayerEndOfAudio:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerEndOfAudio(_:));
@end

NS_ASSUME_NONNULL_END
