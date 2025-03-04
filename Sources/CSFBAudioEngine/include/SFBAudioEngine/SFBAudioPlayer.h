//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#if !TARGET_OS_IPHONE
#import <CoreAudio/CoreAudio.h>
#import <AudioToolbox/AudioToolbox.h>
#endif /* !TARGET_OS_IPHONE */

#import <SFBAudioEngine/SFBAudioPlayerNode.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerDelegate;

/// A block accepting a single `AVAudioEngine` parameter
typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine) NS_SWIFT_NAME(AudioPlayer.AVAudioEngineClosure);

/// The possible playback states for `SFBAudioPlayer`
typedef NS_ENUM(NSUInteger, SFBAudioPlayerPlaybackState) {
	/// `SFBAudioPlayer.engineIsRunning` and `SFBAudioPlayer.playerNodeIsPlaying`
	SFBAudioPlayerPlaybackStatePlaying		= 0,
	/// `SFBAudioPlayer.engineIsRunning` and `!SFBAudioPlayer.playerNodeIsPlaying`
	SFBAudioPlayerPlaybackStatePaused		= 1,
	/// `!SFBAudioPlayer.engineIsRunning`
	SFBAudioPlayerPlaybackStateStopped		= 2,
} NS_SWIFT_NAME(AudioPlayer.PlaybackState);

/// An audio player wrapping an `AVAudioEngine` processing graph supplied by `SFBAudioPlayerNode`
///
/// `SFBAudioPlayer` supports gapless playback for audio with the same sample rate and number of channels.
/// For audio with different sample rates or channels, the audio processing graph is automatically reconfigured.
///
/// An `SFBAudioPlayer` may be in one of three playback states: playing, paused, or stopped. These states are
/// based on whether the underlying `AVAudioEngine` is running (`SFBAudioPlayer.engineIsRunning`)
/// and the `SFBAudioPlayerNode` is playing (`SFBAudioPlayer.playerNodeIsPlaying`).
///
/// `SFBAudioPlayer` supports delegate-based callbacks for the following events:
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
///  10. Decoder canceled
///  11. Processing graph format change with custom nodes present
///  12. `AVAudioEngineConfigurationChange` notification received
///  13. Asynchronous error encountered
///
/// The dispatch queue on which callbacks are performed is not specified.
NS_SWIFT_NAME(AudioPlayer) @interface SFBAudioPlayer : NSObject <SFBAudioPlayerNodeDelegate>

#pragma mark - Playlist Management

/// Cancels the current decoder, clears any queued decoders, creates and enqueues a decoder, and starts playback
/// - note: This is equivalent to `-enqueueURL:forImmediatePlayback:error:` with `YES` for `forImmediatePlayback` followed by `-playReturningError:`
/// - parameter url: The URL to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued and playback started successfully
- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
/// Cancels the current decoder, clears any queued decoders, enqueues a decoder, and starts playback
/// - note: This is equivalent to `-enqueueDecoder:forImmediatePlayback:error:` with `YES` for `forImmediatePlayback` followed by `-playReturningError:`
/// - parameter decoder: The decoder to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued and playback started successfully
- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

/// Creates and enqueues a decoder for subsequent playback
/// - note: This is equivalent to `-enqueueURL:forImmediatePlayback:error:` with `NO` for `forImmediatePlayback`
/// - parameter url: The URL to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Creates and enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any queued decoders
/// - note: This is equivalent to creating an `SFBAudioDecoder` object for `url` and passing that object to `-enqueueDecoder:forImmediatePlayback:error:`
/// - parameter url: The URL to enqueue
/// - parameter forImmediatePlayback: If `YES` the current decoder is canceled and any queued decoders are cleared before enqueuing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));
/// Enqueues a decoder for subsequent playback
/// - note: This is equivalent to `-enqueueDecoder:forImmediatePlayback:error:` with `NO` for `forImmediatePlayback`
/// - parameter decoder: The decoder to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Enqueues a decoder for subsequent playback, optionally canceling the current decoder and clearing any queued decoders
/// - note: If `forImmediatePlayback` is `YES`, the audio processing graph is reconfigured for `decoder.processingFormat` if necessary
/// - parameter decoder: The decoder to enqueue
/// - parameter forImmediatePlayback: If `YES` the current decoder is canceled and any queued decoders are cleared before enqueuing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder forImmediatePlayback:(BOOL)forImmediatePlayback error:(NSError **)error NS_SWIFT_NAME(enqueue(_:immediate:));

/// Returns `YES` if audio with `format` will be played gaplessly
- (BOOL)formatWillBeGaplessIfEnqueued:(AVAudioFormat *)format;

/// Clears the decoder queue
- (void)clearQueue;

/// Returns `YES` if the decoder queue is empty
@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

/// Starts the underlying `AVAudioEngine` and plays the `SFBAudioPlayerNode`
/// - note: If the current `playbackState` is `SFBAudioPlayerPlaybackStatePlaying` this method has no effect
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the underlying `AVAudioEngine` was successfully started
- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
/// Pauses the `SFBAudioPlayerNode`
/// - note: If the current `playbackState` is not `SFBAudioPlayerPlaybackStatePlaying` this method has no effect
- (void)pause;
/// Plays the `SFBAudioPlayerNode`
/// - note: If the current `playbackState` is not `SFBAudioPlayerPlaybackStatePaused` this method has no effect
- (void)resume;
/// Stops both the underlying `AVAudioEngine` and `SFBAudioPlayerNode`
/// - note: This method cancels the current decoder and clears any queued decoders
/// - note: If the current `playbackState` is `SFBAudioPlayerPlaybackStateStopped` this method has no effect
- (void)stop;
/// Toggles the player between playing and paused states, starting playback if stopped
///
/// If the current `playbackState` is `SFBAudioPlayerPlaybackStateStopped` this method sends `-playReturningError:`
/// If the current `playbackState` is `SFBAudioPlayerPlaybackStatePlaying` this method sends `-pause`
/// If the current `playbackState` is `SFBAudioPlayerPlaybackStatePaused` this method sends `-resume`
- (BOOL)togglePlayPauseReturningError:(NSError **)error NS_SWIFT_NAME(togglePlayPause());

/// Resets both the underlying `AVAudioEngine` and `SFBAudioPlayerNode`
/// - note: This method cancels the current decoder and clears any queued decoders
- (void)reset;

#pragma mark - Player State

 /// Returns `YES` if the `AVAudioEngine` is running
@property (nonatomic, readonly) BOOL engineIsRunning;
/// Returns `YES` if the `SFBAudioPlayerNode` is playing
@property (nonatomic, readonly) BOOL playerNodeIsPlaying;

/// Returns the current playback state
@property (nonatomic, readonly) SFBAudioPlayerPlaybackState playbackState;
/// Returns `YES` if `engineIsRunning` and `playerNodeIsPlaying`
@property (nonatomic, readonly) BOOL isPlaying;
/// Returns `YES` if `engineIsRunning` and `!playerNodeIsPlaying`
@property (nonatomic, readonly) BOOL isPaused;
/// Returns `NO` if `engineIsRunning`
@property (nonatomic, readonly) BOOL isStopped;

/// Returns `YES` if a decoder is available to supply audio for the next render cycle
@property (nonatomic, readonly) BOOL isReady;
/// Returns the decoder supplying the earliest audio frame for the next render cycle or `nil` if none
/// - warning: Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> currentDecoder;
/// Returns the decoder approximating what a user would expect to see as the "now playing" item- the decoder that is
/// currently rendering audio.
/// - warning: Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> nowPlaying;

#pragma mark - Playback Properties

/// Returns the frame position in the current decoder or `SFBUnknownFramePosition` if the current decoder is `nil`
@property (nonatomic, readonly) AVAudioFramePosition framePosition NS_REFINED_FOR_SWIFT;
/// Returns the frame length of the current decoder or `SFBUnknownFrameLength` if the current decoder is `nil`
@property (nonatomic, readonly) AVAudioFramePosition frameLength NS_REFINED_FOR_SWIFT;
/// Returns the playback position in the current decoder or `SFBInvalidPlaybackPosition` if the current decoder is `nil`
@property (nonatomic, readonly) SFBPlaybackPosition playbackPosition;

/// Returns the current time in the current decoder or `SFBUnknownTime` if the current decoder is `nil`
@property (nonatomic, readonly) NSTimeInterval currentTime NS_REFINED_FOR_SWIFT;
/// Returns the total time of the current decoder or `SFBUnknownTime` if the current decoder is `nil`
@property (nonatomic, readonly) NSTimeInterval totalTime NS_REFINED_FOR_SWIFT;
/// Returns the playback time in the current decoder or `SFBInvalidPlaybackTime` if the current decoder is `nil`
@property (nonatomic, readonly) SFBPlaybackTime playbackTime;

/// Retrieves the playback position and time
/// - parameter playbackPosition: An optional pointer to an `SFBPlaybackPosition` struct to receive playback position information
/// - parameter playbackTime: An optional pointer to an `SFBPlaybackTime` struct to receive playback time information
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)getPlaybackPosition:(nullable SFBPlaybackPosition *)playbackPosition andTime:(nullable SFBPlaybackTime *)playbackTime;

#pragma mark - Seeking

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
@property (nonatomic, readonly) BOOL supportsSeeking;

#if !TARGET_OS_IPHONE
#pragma mark - Volume Control

/// Returns `kHALOutputParam_Volume` on channel `0` for `AVAudioEngine.outputNode.audioUnit` or `NaN` on error
@property (nonatomic, readonly) float volume;
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

#pragma mark - Output Device

/// Returns the output device object ID for `AVAudioEngine.outputNode`
@property (nonatomic, readonly) AUAudioObjectID outputDeviceID;
/// Sets the output device for `AVAudioEngine.outputNode`
/// - parameter outputDeviceID: The audio object ID of the desired output device
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the output device was successfully set
- (BOOL)setOutputDeviceID:(AUAudioObjectID)outputDeviceID error:(NSError **)error;
#endif /* !TARGET_OS_IPHONE */

#pragma mark - Delegate

/// An optional delegate
@property (nonatomic, nullable, weak) id<SFBAudioPlayerDelegate> delegate;

#pragma mark - AVAudioEngine Access

/// Peforms an operation on the underlying `AVAudioEngine`
/// - important: Graph modifications may only be made between `playerNode` and `engine.mainMixerNode`
/// - parameter block: A block performing operations on the underlying `AVAudioEngine`
- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block;
/// Returns the `SFBAudioPlayerNode` that is the source of the audio processing graph
@property (nonatomic, nonnull, readonly) SFBAudioPlayerNode *playerNode;

#pragma mark - Debugging

/// Asynchronously logs a description of the player's audio processing graph
/// - parameter log: An `os_log_t` object to receive the message
/// - parameter type: The type of log message
-(void)logProcessingGraphDescription:(os_log_t)log type:(os_log_type_t)type;

@end

#pragma mark - SFBAudioPlayerDelegate

/// Delegate methods supported by `SFBAudioPlayer`
NS_SWIFT_NAME(AudioPlayer.Delegate) @protocol SFBAudioPlayerDelegate <NSObject>
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
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillStart:at:));
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
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingWillComplete:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayer(_:renderingWillComplete:at:));
/// Called to notify the delegate when rendering the final frame of audio from a decoder
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which rendering is complete
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer renderingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when the now playing item changes
/// - warning: Do not change any properties of `nowPlaying` or `previouslyPlaying`
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter nowPlaying: The decoder that is now playing
/// - parameter previouslyPlaying: The decoder that was playing previously
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer nowPlayingChanged:(nullable id <SFBPCMDecoding>)nowPlaying previouslyPlaying:(nullable id <SFBPCMDecoding>)previouslyPlaying;
/// Called to notify the delegate when the playback state changes
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter playbackState: The current playback state
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer playbackStateChanged:(SFBAudioPlayerPlaybackState)playbackState;
/// Called to notify the delegate when rendering is complete for all available decoders
/// - parameter audioPlayer: The `SFBAudioPlayer` object
- (void)audioPlayerEndOfAudio:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerEndOfAudio(_:));
/// Called to notify the delegate that the decoding and rendering process for a decoder has been canceled
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayer: The `SFBAudioPlayer` object processing `decoder`
/// - parameter decoder: The decoder for which decoding and rendering is canceled
/// - parameter framesRendered: The number of audio frames from `decoder` that were rendered
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer decoderCanceled:(id<SFBPCMDecoding>)decoder framesRendered:(AVAudioFramePosition)framesRendered;
/// Called to notify the delegate when additional changes to the `AVAudioEngine` processing graph may need to be made in response to a format change
///
/// Before this method is called the main mixer node will be connected to the output node, and the player node will be attached
/// to the processing graph with no connections.
///
/// The delegate should establish or update any connections in the processing graph segment between the node to be returned and the main mixer node.
///
/// After this method returns the player node will be connected to the returned node using the specified format.
/// - important: This method is called from a context where it is safe to modify `engine`
/// - note: This method is only called when one or more nodes have been inserted between the player node and main mixer node.
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter engine: The `AVAudioEngine` object
/// - parameter format: The rendering format of the player node
/// - returns: The `AVAudioNode` to which the player node should be connected
- (AVAudioNode *)audioPlayer:(SFBAudioPlayer *)audioPlayer reconfigureProcessingGraph:(AVAudioEngine *)engine withFormat:(AVAudioFormat *)format NS_SWIFT_NAME(audioPlayer(_:reconfigureProcessingGraph:with:));
/// Called to notify the delegate when the configuration of the underlying `AVAudioEngine` changes
/// - note: Use this instead of listening for `AVAudioEngineConfigurationChangeNotification`
/// - parameter audioPlayer: The `SFBAudioPlayer` object
- (void)audioPlayerAVAudioEngineConfigurationChange:(SFBAudioPlayer *)audioPlayer NS_SWIFT_NAME(audioPlayerAVAudioEngineConfigurationChange(_:));
/// Called to notify the delegate when an asynchronous error occurs
/// - parameter audioPlayer: The `SFBAudioPlayer` object
/// - parameter error: The error
- (void)audioPlayer:(SFBAudioPlayer *)audioPlayer encounteredError:(NSError *)error;
@end

NS_ASSUME_NONNULL_END
