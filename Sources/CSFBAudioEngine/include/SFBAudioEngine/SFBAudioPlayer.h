//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

#if !TARGET_OS_IPHONE
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#endif /* !TARGET_OS_IPHONE */

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerDelegate;

/// A block accepting a single `AVAudioEngine` parameter
typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine) NS_SWIFT_NAME(AudioPlayer.AVAudioEngineClosure);

/// The possible playback states for `SFBAudioPlayer`
typedef NS_ENUM(NSUInteger, SFBAudioPlayerPlaybackState) {
    /// The `AVAudioEngine` is not running
    SFBAudioPlayerPlaybackStateStopped = 0,
    /// The `AVAudioEngine` is running and the player is not rendering audio
    SFBAudioPlayerPlaybackStatePaused = 1,
    /// The `AVAudioEngine` is running and the player is rendering audio
    SFBAudioPlayerPlaybackStatePlaying = 3,
} NS_SWIFT_NAME(AudioPlayer.PlaybackState);

/// An audio player using an `AVAudioEngine` processing graph for playback
///
/// `SFBAudioPlayer` supports gapless playback for audio with the same sample rate and number of channels.
/// For audio with different sample rates or channels, the audio processing graph is automatically reconfigured.
///
/// An `SFBAudioPlayer` may be in one of three playback states: playing, paused, or stopped.
///
/// `SFBAudioPlayer` supports delegate-based notifications for the following events:
///
///  1. Decoding started
///  2. Decoding complete
///  3. Rendering will start
///  4. Rendering started
///  5. Rendering will complete
///  6. Rendering complete
///  7. Now playing changed
///  8. Playback state changed
///  9. End of audio
///  10. Decoder canceled by user
///  11. Decoding aborted due to error
///  12. Asynchronous error encountered
///  13. Processing graph format change with custom nodes present
///  14. `AVAudioEngineConfigurationChange` notification received
///  15. `AVAudioSessionInterruption` notification received
///
/// The dispatch queue on which delegate messages are sent is not specified.
NS_SWIFT_NAME(AudioPlayer)
@interface SFBAudioPlayer : NSObject

// MARK: - Playlist Management

/// Cancels the current decoder, clears any queued decoders, creates and enqueues a decoder, and starts playback
/// - note: This is equivalent to ``-enqueueURL:forImmediatePlayback:error:`` with `YES` for `forImmediatePlayback`
/// followed by ``-playReturningError:``
/// - parameter url: The URL to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued and playback started successfully
- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
/// Cancels the current decoder, clears any queued decoders, enqueues a decoder, and starts playback
/// - note: This is equivalent to ``-enqueueDecoder:forImmediatePlayback:error:`` with `YES` for `forImmediatePlayback`
/// followed by ``-playReturningError:``
/// - parameter decoder: The decoder to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued and playback started successfully
- (BOOL)playDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

/// Creates and enqueues a decoder for subsequent playback
/// - note: This is equivalent to ``-enqueueURL:forImmediatePlayback:error:`` with `NO` for `forImmediatePlayback`
/// - parameter url: The URL to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Creates and enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any
/// queued decoders
/// - note: This is equivalent to creating an `SFBAudioDecoder` object for `url` and passing that object to
/// ``-enqueueDecoder:forImmediatePlayback:error:``
/// - parameter url: The URL to enqueue
/// - parameter forImmediatePlayback: If `YES` the current decoder is canceled and any queued decoders are cleared
/// before enqueuing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url
        forImmediatePlayback:(BOOL)forImmediatePlayback
                       error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));
/// Enqueues a decoder for subsequent playback
/// - note: This is equivalent to ``-enqueueDecoder:forImmediatePlayback:error:`` with `NO` for `forImmediatePlayback`
/// - parameter decoder: The decoder to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any queued
/// decoders
/// - note: If `forImmediatePlayback` is `YES`, the audio processing graph is reconfigured for
/// `decoder.processingFormat` if necessary
/// - parameter decoder: The decoder to enqueue
/// - parameter forImmediatePlayback: If `YES` the current decoder is canceled and any queued decoders are cleared
/// before enqueuing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id<SFBPCMDecoding>)decoder
        forImmediatePlayback:(BOOL)forImmediatePlayback
                       error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));

/// Returns `YES` if audio with `format` will be played gaplessly
- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format;

/// Clears the decoder queue
- (void)clearQueue;

/// `YES` if the decoder queue is empty
@property(nonatomic, readonly) BOOL queueIsEmpty;

// MARK: - Playback Control

/// Starts the `AVAudioEngine` and begins rendering audio
/// - note: If the current playback state is `SFBAudioPlayerPlaybackStatePlaying` this method has no effect
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the `AVAudioEngine` is running and the player is rendering audio
- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
/// Pauses audio rendering
/// - note: If the current playback state is not `SFBAudioPlayerPlaybackStatePlaying` this method has no effect
/// - returns: `YES` if the `AVAudioEngine` is running and the player is not rendering audio
- (BOOL)pause;
/// Resumes audio rendering
/// - note: If the current playback state is not `SFBAudioPlayerPlaybackStatePaused` this method has no effect
/// - returns: `YES` if the `AVAudioEngine` is running and the player is rendering audio
- (BOOL)resume;
/// Stops the `AVAudioEngine`
/// - note: This method cancels the current decoder and clears any queued decoders
/// - note: If the current playback state is `SFBAudioPlayerPlaybackStateStopped` this method has no effect
- (void)stop;
/// Toggles the player between playing and paused states, starting playback if stopped
///
/// If the current playback state is `SFBAudioPlayerPlaybackStateStopped` this method sends ``-playReturningError:``
/// If the current playback state is `SFBAudioPlayerPlaybackStatePlaying` this method sends ``-pause``
/// If the current playback state is `SFBAudioPlayerPlaybackStatePaused` this method sends ``-resume``
- (BOOL)togglePlayPauseReturningError:(NSError **)error NS_SWIFT_NAME(togglePlayPause());

/// Cancels the current decoder, clears any queued decoders, and resets the `AVAudioEngine`
- (void)reset;

// MARK: - Player State

/// `YES` if the `AVAudioEngine` is running
@property(nonatomic, readonly) BOOL engineIsRunning;

/// The current playback state
@property(nonatomic, readonly) SFBAudioPlayerPlaybackState playbackState;
/// `YES` if the `AVAudioEngine` is running and the player is rendering audio
@property(nonatomic, readonly) BOOL isPlaying;
/// `YES` if the `AVAudioEngine` is running and the player is not rendering audio
@property(nonatomic, readonly) BOOL isPaused;
/// `YES` if the `AVAudioEngine` is not running
@property(nonatomic, readonly) BOOL isStopped;

/// `YES` if a decoder is available to supply audio for the next render cycle
@property(nonatomic, readonly) BOOL isReady;
/// The decoder supplying the earliest audio frame for the next render cycle or `nil` if none
/// - warning: Do not change any properties of the returned object
@property(nonatomic, nullable, readonly) id<SFBPCMDecoding> currentDecoder;
/// The decoder approximating what a user would expect to see as the "now playing" item
/// - warning: Do not change any properties of the returned object
@property(nonatomic, nullable, readonly) id<SFBPCMDecoding> nowPlaying;

// MARK: - Playback Properties

/// The frame position in the current decoder or `SFBUnknownFramePosition` if the current decoder is `nil`
@property(nonatomic, readonly) AVAudioFramePosition framePosition NS_REFINED_FOR_SWIFT;
/// The frame length of the current decoder or `SFBUnknownFrameLength` if the current decoder is `nil`
@property(nonatomic, readonly) AVAudioFramePosition frameLength NS_REFINED_FOR_SWIFT;
/// The playback position in the current decoder or `SFBInvalidPlaybackPosition` if the current decoder is `nil`
@property(nonatomic, readonly) SFBPlaybackPosition playbackPosition;

/// The current time in the current decoder or `SFBUnknownTime` if the current decoder is `nil`
@property(nonatomic, readonly) NSTimeInterval currentTime NS_REFINED_FOR_SWIFT;
/// The total time of the current decoder or `SFBUnknownTime` if the current decoder is `nil`
@property(nonatomic, readonly) NSTimeInterval totalTime NS_REFINED_FOR_SWIFT;
/// The playback time in the current decoder or `SFBInvalidPlaybackTime` if the current decoder is `nil`
@property(nonatomic, readonly) SFBPlaybackTime playbackTime;

/// Retrieves the playback position and time
/// - parameter playbackPosition: An optional pointer to an `SFBPlaybackPosition` struct to receive playback position
/// information
/// - parameter playbackTime: An optional pointer to an `SFBPlaybackTime` struct to receive playback time information
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)getPlaybackPosition:(nullable SFBPlaybackPosition *)playbackPosition
                    andTime:(nullable SFBPlaybackTime *)playbackTime;

// MARK: - Seeking

/// Seeks forward in the current decoder by 3 seconds
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekForward;
/// Seeks backward in the current decoder by 3 seconds
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekBackward;

/// Seeks forward in the current decoder by the specified number of seconds
/// - parameter secondsToSkip: The number of seconds to skip forward
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekForward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(forward:));
/// Seeks backward in the current decoder by the specified number of seconds
/// - parameter secondsToSkip: The number of seconds to skip backward
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(backward:));

/// Seeks to the specified time in the current decoder
/// - parameter timeInSeconds: The desired time in seconds
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds NS_SWIFT_NAME(seek(time:));
/// Seeks to the specified positioni n the current decoder
/// - parameter position: The desired position in the interval `[0, 1)`
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekToPosition:(double)position NS_SWIFT_NAME(seek(position:));
/// Seeks to the specified audio frame in the current decoder
/// - parameter frame: The desired audio frame
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)seekToFrame:(AVAudioFramePosition)frame NS_SWIFT_NAME(seek(frame:));

/// Returns `YES` if the current decoder supports seeking
@property(nonatomic, readonly) BOOL supportsSeeking;

#if !TARGET_OS_IPHONE
// MARK: - Volume Control

/// Returns `kHALOutputParam_Volume` on channel `0` for `AVAudioEngine.outputNode.audioUnit` or `NaN` on error
@property(nonatomic, readonly) float volume;
/// Sets `kHALOutputParam_Volume` on channel `0` for `AVAudioEngine.outputNode.audioUnit`
/// - parameter volume: The desired volume
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the volume was successfully set
- (BOOL)setVolume:(float)volume error:(NSError **)error;

/// Returns `kHALOutputParam_Volume` on `channel` for `AVAudioEngine.outputNode.audioUnit` or `NaN` on error
- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
/// Sets `kHALOutputParam_Volume` on `channel` for `AVAudioEngine.outputNode.audioUnit`
/// - parameter volume: The desired volume
/// - parameter channel: The channel to adjust
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the volume was successfully set
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error;

// MARK: - Output Device

/// The output device object ID for `AVAudioEngine.outputNode`
@property(nonatomic, readonly) AUAudioObjectID outputDeviceID;
/// Sets the output device for `AVAudioEngine.outputNode`
/// - parameter outputDeviceID: The audio object ID of the desired output device
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the output device was successfully set
- (BOOL)setOutputDeviceID:(AUAudioObjectID)outputDeviceID error:(NSError **)error;
#endif

// MARK: - Delegate

/// An optional delegate
@property(nonatomic, nullable, weak) id<SFBAudioPlayerDelegate> delegate;

// MARK: - AVAudioEngine

/// Calls `block` from a context safe to perform operations on the `AVAudioEngine` processing graph
/// - important: Graph modifications may only be made between `sourceNode` and `engine.mainMixerNode`
/// - attention: The audio engine must not be started or stopped directly; use the player's playback control methods
/// instead. Directly starting or stopping the engine may cause internal state inconsistencies.
/// - parameter block: A block performing operations on the `AVAudioEngine`
- (void)modifyProcessingGraph:(SFBAudioPlayerAVAudioEngineBlock)block;
/// The audio processing graph's source node
/// - attention: Do not make any modifications to the node's connections.
@property(nonatomic, nonnull, readonly) AVAudioSourceNode *sourceNode;
/// The audio processing graph's main mixer node
/// - attention: Do not make any modifications to the node's connections.
@property(nonatomic, nonnull, readonly) AVAudioMixerNode *mainMixerNode;
/// The audio processing graph's output node
/// - attention: Do not make any modifications to the node's connections.
@property(nonatomic, nonnull, readonly) AVAudioOutputNode *outputNode;

// MARK: - Debugging

/// Logs a description of the player's audio processing graph
/// - parameter log: An `os_log_t` object to receive the message
/// - parameter type: The type of log message
- (void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type;

@end

// MARK: - Error Information

/// The `NSErrorDomain` used by `SFBAudioPlayer`
extern NSErrorDomain const SFBAudioPlayerErrorDomain NS_SWIFT_NAME(AudioPlayer.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioPlayer`
typedef NS_ERROR_ENUM(SFBAudioPlayerErrorDomain, SFBAudioPlayerErrorCode){
    /// Internal or unspecified error
    SFBAudioPlayerErrorCodeInternalError = 0,
    /// Format not supported
    SFBAudioPlayerErrorCodeFormatNotSupported = 1,
} NS_SWIFT_NAME(AudioPlayer.Error);

// MARK: - SFBAudioPlayerDelegate

/// Delegate methods supported by `SFBAudioPlayer`
NS_SWIFT_NAME(AudioPlayer.Delegate)
@protocol SFBAudioPlayerDelegate <NSObject>
@optional
/// Called to notify the delegate before decoding the first frame of audio from a decoder
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which decoding started
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate after decoding the final frame of audio from a decoder
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which decoding is complete
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decodingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that the first audio frame from a decoder will render
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which rendering will start
/// - parameter hostTime: The host time at which the first audio frame from `decoder` will reach the device
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer
        renderingWillStart:(id<SFBPCMDecoding>)decoder
                atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillStart:at:));
/// Called to notify the delegate when rendering the first frame of audio from a decoder
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing decoder
/// - parameter decoder: The decoder for which rendering started
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that the final audio frame from a decoder will render
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which rendering will complete
/// - parameter hostTime: The host time at which the final audio frame from `decoder` will finish playing on the device
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer
        renderingWillComplete:(id<SFBPCMDecoding>)decoder
                   atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillComplete:at:));
/// Called to notify the delegate when rendering the final frame of audio from a decoder
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which rendering is complete
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when the now playing item changes
/// - warning: Do not change any properties of `nowPlaying`
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter nowPlaying: The decoder that is now playing
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer nowPlayingChanged:(nullable id<SFBPCMDecoding>)nowPlaying;
/// Called to notify the delegate when the playback state changes
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter playbackState: The current playback state
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer playbackStateChanged:(SFBAudioPlayerPlaybackState)playbackState;
/// Called to notify the delegate when rendering is complete for all available decoders
/// - parameter audioPlayer: The `SFBAudioPlayer` object
- (void)audioPlayerEndOfAudio:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerEndOfAudio(_:));
/// Called to notify the delegate that the decoding and rendering processes for a decoder have been canceled by a
/// user-initiated request
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which decoding and rendering are canceled
/// - parameter framesRendered: The number of audio frames from `decoder` that were rendered
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer
        decoderCanceled:(id<SFBPCMDecoding>)decoder
         framesRendered:(AVAudioFramePosition)framesRendered;
/// Called to notify the delegate that the decoding process for a decoder has been aborted because of an error
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which decoding is aborted
/// - parameter error: The error causing `decoder` to abort
/// - parameter framesRendered: The number of audio frames from `decoder` that were rendered
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer
        decodingAborted:(id<SFBPCMDecoding>)decoder
                  error:(NSError *)error
         framesRendered:(AVAudioFramePosition)framesRendered;
/// Called to notify the delegate when an asynchronous error occurs
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter error: The error
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer encounteredError:(NSError *)error;
/// Called to notify the delegate when additional changes to the `AVAudioEngine` processing graph may need to be made in
/// response to a format change
///
/// Before this method is called the main mixer node will be connected to the output node, and the source node will be
/// attached to the processing graph with no connections.
///
/// The delegate should establish or update any connections in the processing graph segment between the node to be
/// returned and the main mixer node.
///
/// After this method returns the source node will be connected to the returned node using the specified format.
/// - important: This method is called from a context where it is safe to modify `engine`
/// - note: This method is only called when one or more nodes have been inserted between the source node and main mixer
/// node.
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter engine: The `AVAudioEngine` object
/// - parameter format: The rendering format of the source node
/// - returns: The `AVAudioNode` to which the source node should be connected
- (AVAudioNode *)audioPlayer:(SFBAudioPlayer *)audioPlayer
        reconfigureProcessingGraph:(AVAudioEngine *)engine
                        withFormat:(AVAudioFormat *)format
        NS_SWIFT_NAME(audioPlayer(_:reconfigureProcessingGraph:with:));
/// Called to notify the delegate when the hardware channel count or sample rate of the `AVAudioEngine` output unit
/// changes
///
/// This method is called after the processing graph is updated for the new hardware channel count or sample rate
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter userInfo: The `userInfo` object from the notification
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer audioEngineConfigurationChange:(nullable NSDictionary *)userInfo;
#if TARGET_OS_IPHONE
/// Called to notify the delegate of an `AVAudioSession` interruption begin or end
///
/// If the interruption began, this method is called after the playback state is saved and the player is stopped.
/// If the interruption ended, this method is called before optionally attempting to activate the audio session and
/// resume playback.
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter userInfo: The `userInfo` object from the notification
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer audioSessionInterruption:(nullable NSDictionary *)userInfo;
#endif
@end

NS_ASSUME_NONNULL_END
