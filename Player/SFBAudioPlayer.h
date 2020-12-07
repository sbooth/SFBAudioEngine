/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <SFBAudioEngine/SFBAudioPlayerNode.h>
#if TARGET_OS_OSX
#import <SFBAudioEngine/SFBAudioOutputDevice.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerDelegate;

/// Playback position information for \c SFBAudioPlayer
typedef SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerPlaybackPosition /*NS_SWIFT_UNAVAILABLE("Use AudioPlayer.PlaybackPosition instead")*/;
/// Playback time information for \c SFBAudioPlayer
typedef SFBAudioPlayerNodePlaybackTime SFBAudioPlayerPlaybackTime /*NS_SWIFT_UNAVAILABLE("Use AudioPlayer.PlaybackTime instead")*/;

/// A block accepting a single \c AVAudioEngine parameter
typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine) NS_SWIFT_NAME(AudioPlayer.AVAudioEngineClosure);

/// The possible playback states for \c SFBAudioPlayer
typedef NS_ENUM(NSUInteger, SFBAudioPlayerPlaybackState) {
	/// \c SFBAudioPlayer.engineIsRunning and \c SFBAudioPlayer.playerNodeIsPlaying
	SFBAudioPlayerPlaybackStatePlaying		= 0,
	/// \c SFBAudioPlayer.engineIsRunning and \c !SFBAudioPlayer.playerNodeIsPlaying
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
///
/// \c SFBAudioPlayer supports delegate-based callbacks for the following events:
///
///  1. Decoding started
///  2. Decoding complete
///  3. Decoding canceled
///  4. Rendering will start
///  5. Rendering started
///  6. Rendering complete
///  7. Now playing changed
///  8. Playback state changed
///  9. AVAudioEngineConfigurationChange notification received
///  10. End of audio
///  11. Asynchronous error encountered
///
/// The dispatch queue on which callbacks are performed is not specified.
NS_SWIFT_NAME(AudioPlayer) @interface SFBAudioPlayer : NSObject <SFBAudioPlayerNodeDelegate>

#pragma mark - Playlist Management

/// Cancels the current decoder, clears any queued decoders, creates and enqueues a decoder, and starts playback
/// @note This is equivalent to \c -enqueueURL:forImmediatePlayback:error: with \c YES for \c forImmediatePlayback followed by \c -playReturningError:
/// @param url The URL to play
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued and playback started successfully
- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
/// Cancels the current decoder, clears any queued decoders, enqueues a decoder, and starts playback
/// @note This is equivalent to \c -enqueueDecoder:forImmediatePlayback:error: with \c YES for \c forImmediatePlayback followed by \c -playReturningError:
/// @param decoder The decoder to play
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued and playback started successfully
- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

/// Creates and enqueues a decoder for subsequent playback
/// @note This is equivalent to \c -enqueueURL:forImmediatePlayback:error: with \c NO for \c forImmediatePlayback
/// @param url The URL to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Creates and enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any queued decoders
/// @note This is equivalent to creating an \c SFBAudioDecoder object for \c url and passing that object to \c -enqueueDecoder:forImmediatePlayback:error:
/// @param url The URL to enqueue
/// @param forImmediatePlayback If \c YES the current decoder is canceled and any queued decoders are cleared before enqueuing
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));
/// Enqueues a decoder for subsequent playback
/// @note This is equivalent to \c -enqueueDecoder:forImmediatePlayback:error: with \c NO for \c forImmediatePlayback
/// @param decoder The decoder to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any queued decoders
/// @note If \c forImmediatePlayback is \c YES, the audio processing graph is reconfigured for \c decoder.processingFormat if necessary
/// @param decoder The decoder to enqueue
/// @param forImmediatePlayback If \c YES the current decoder is canceled and any queued decoders are cleared before enqueuing
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));

/// Returns \c YES if audio with \c format will be played gaplessly
- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format;

/// Empties the decoder queue
- (void)clearQueue;

/// Returns \c YES if the decoder queue is empty
@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

/// Starts the underlying \c AVAudioEngine and plays the \c SFBAudioPlayerNode
/// @note If the current \c playbackState is \c SFBAudioPlayerPlaybackStatePlaying this method has no effect
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the underlying \c AVAudioEngine was successfully started
- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
/// Pauses the \c SFBAudioPlayerNode
/// @note If the current \c playbackState is not \c SFBAudioPlayerPlaybackStatePlaying this method has no effect
- (void)pause;
/// Plays the \c SFBAudioPlayerNode
/// @note If the current \c playbackState is not \c SFBAudioPlayerPlaybackStatePaused this method has no effect
- (void)resume;
/// Stops both the underlying \c AVAudioEngine and \c SFBAudioPlayerNode
/// @note This method cancels the current decoder and clears any queued decoders
/// @note If the current \c playbackState is \c SFBAudioPlayerPlaybackStateStopped this method has no effect
- (void)stop;
/// Toggles the player between playing and paused states, starting playback if stopped
///
/// If the current \c playbackState is \c SFBAudioPlayerPlaybackStateStopped this method sends \c -playReturningError:
/// If the current \c playbackState is \c SFBAudioPlayerPlaybackStatePlaying this method sends \c -pause
/// If the current \c playbackState is \c SFBAudioPlayerPlaybackStatePaused this method sends \c -resume
- (BOOL)togglePlayPauseReturningError:(NSError **)error NS_SWIFT_NAME(togglePlayPause());

/// Resets both the underlying \c AVAudioEngine and \c SFBAudioPlayerNode
/// @note This method cancels the current decoder and clears any queued decoders
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
/// Returns the decoder approximating what a user would expect to see as the "now playing" item- the decoder that is
/// currently rendering audio.
/// @warning Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> nowPlaying;

#pragma mark - Playback Properties

/// Returns the frame position in the current decoder or \c SFBUnknownFramePosition if the current decoder is \c nil
@property (nonatomic, readonly) AVAudioFramePosition framePosition;
/// Returns the frame length of the current decoder or \c SFBUnknownFrameLength if the current decoder is \c nil
@property (nonatomic, readonly) AVAudioFramePosition frameLength;
/// Returns the playback position in the current decoder or \c {SFBUnknownFramePosition, \c SFBUnknownFrameLength} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerPlaybackPosition playbackPosition NS_REFINED_FOR_SWIFT;

/// Returns the current time in the current decoder or \c SFBUnknownTime if the current decoder is \c nil
@property (nonatomic, readonly) NSTimeInterval currentTime;
/// Returns the total time of the current decoder or \c SFBUnknownTime if the current decoder is \c nil
@property (nonatomic, readonly) NSTimeInterval totalTime;
/// Returns the playback time in the current decoder or \c {SFBUnknownTime, \c SFBUnknownTime} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerPlaybackTime playbackTime NS_REFINED_FOR_SWIFT;

/// Retrieves the playback position and time
/// @param playbackPosition An optional pointer to an \c SFBAudioPlayerPlaybackPosition struct to receive playback position information
/// @param playbackTime An optional pointer to an \c SFBAudioPlayerPlaybackTime struct to receive playback time information
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
- (BOOL)seekToPosition:(double)position NS_SWIFT_NAME(seek(position:));
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
/// Returns the \c SFBAudioPlayerNode that is the source of the audio processing graph
@property (nonatomic, nonnull, readonly) SFBAudioPlayerNode *playerNode;

@end

#pragma mark - SFBAudioPlayerDelegate

/// Delegate methods supported by \c SFBAudioPlayer
NS_SWIFT_NAME(AudioPlayer.Delegate) @protocol SFBAudioPlayerDelegate <NSObject>
@optional
/// Called to notify the delegate before decoding the first frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing \c decoder
/// @param decoder The decoder for which decoding started
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate after decoding the final frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing \c decoder
/// @param decoder The decoder for which decoding is complete
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that decoding has been canceled
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing \c decoder
/// @param decoder The decoder for which decoding is canceled
/// @param partiallyRendered \c YES if any audio frames from \c decoder were rendered
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingCanceled:(id<SFBPCMDecoding>)decoder partiallyRendered:(BOOL)partiallyRendered;
/// Called to notify the delegate that audio will soon begin rendering
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing \c decoder
/// @param decoder The decoder for which rendering is about to start
/// @param hostTime The host time at which the first audio frame from \c decoder will reach the device
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillStart:at:));
/// Called to notify the delegate when rendering the first frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing decoder
/// @param decoder The decoder for which rendering started
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when rendering the final frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayer The \c SFBAudioPlayer object processing \c decoder
/// @param decoder The decoder for which rendering is complete
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when the now playing item changes
/// @param audioPlayer The \c SFBAudioPlayer object
- (void)audioPlayerNowPlayingChanged:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerNowPlayingChanged(_:));
/// Called to notify the delegate when the playback state changes
/// @param audioPlayer The \c SFBAudioPlayer object
- (void)audioPlayerPlaybackStateChanged:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerPlaybackStateChanged(_:));
/// Called to notify the delegate when the configuration of the underlying \c AVAudioEngine changes
/// @note Use this instead of listening for \c AVAudioEngineConfigurationChangeNotification
/// @param audioPlayer The \c SFBAudioPlayer object
- (void)audioPlayerAVAudioEngineConfigurationChange:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerAVAudioEngineConfigurationChange(_:));
/// Called to notify the delegate when rendering is complete for all available decoders
/// @param audioPlayer The \c SFBAudioPlayer object
- (void)audioPlayerEndOfAudio:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerEndOfAudio(_:));
/// Called to notify the delegate when an asynchronous error occurs
/// @param audioPlayer The \c SFBAudioPlayer object
/// @param error The error
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer encounteredError:(NSError *)error;
@end

NS_ASSUME_NONNULL_END
