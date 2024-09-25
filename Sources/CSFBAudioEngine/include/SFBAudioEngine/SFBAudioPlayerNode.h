//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerNodeDelegate;

#pragma mark - Playback position and time information

/// Playback position information for `SFBAudioPlayerNode`
struct SFBAudioPlayerNodePlaybackPosition {
	/// The current frame position or `SFBUnknownFramePosition` if unknown
	AVAudioFramePosition framePosition;
	/// The total number of frames or `SFBUnknownFrameLength` if unknown
	AVAudioFramePosition frameLength;
} NS_REFINED_FOR_SWIFT;
typedef struct SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerNodePlaybackPosition;

/// Playback time information for `SFBAudioPlayerNode`
struct SFBAudioPlayerNodePlaybackTime {
	/// The current time or `SFBUnknownTime` if unknown
	NSTimeInterval currentTime;
	/// The total time or `SFBUnknownTime` if unknown
	NSTimeInterval totalTime;
} NS_REFINED_FOR_SWIFT;
typedef struct SFBAudioPlayerNodePlaybackTime SFBAudioPlayerNodePlaybackTime;

#pragma mark - SFBAudioPlayerNode

/// An `AVAudioSourceNode` supporting gapless playback for PCM formats
///
/// The output format of `SFBAudioPlayerNode` is specified at object initialization and cannot be changed. The output
/// format must be the standard format (deinterleaved native-endian 32-bit floating point PCM) at any sample rate with
/// any number of channels.
///
/// `SFBAudioPlayerNode` is supplied by objects implementing `SFBPCMDecoding` (decoders) and supports audio at the
/// same sample rate and with the same number of channels as the output format. `SFBAudioPlayerNode` supports seeking
/// when supported by the decoder.
///
/// `SFBAudioPlayerNode` maintains a current decoder and a queue of pending decoders. The current decoder is the
/// decoder that will supply the earliest audio frame in the next render cycle when playing. Pending decoders are
/// automatically dequeued and become current when the final frame of the current decoder is pushed in the render block.
///
/// `SFBAudioPlayerNode` decodes audio in a high-priority non-realtime thread into a ring buffer and renders on
/// demand. Rendering occurs in a realtime thread when the render block is called; the render block always supplies
/// audio. When playback is paused or insufficient audio is available the render block outputs silence.
///
/// `SFBAudioPlayerNode` supports delegate-based callbacks for the following events:
///
///  1. Decoding started
///  2. Decoding complete
///  3. Decoding canceled
///  4. Rendering will start
///  5. Rendering will complete
///  6. Audio will end
///  7. Asynchronous error encountered
///
/// All callbacks are performed on a dedicated notification queue.
NS_SWIFT_NAME(AudioPlayerNode) @interface SFBAudioPlayerNode : AVAudioSourceNode

/// Returns an initialized `SFBAudioPlayerNode` object for stereo audio at 44,100 Hz
- (instancetype)init;
/// Returns an initialized `SFBAudioPlayerNode` object for audio with a specified number of channels and sample rate
/// - parameter sampleRate: The sample rate supplied by the render block
/// - parameter channels: The number of channels supplied by the render block
/// - returns: An initialized `SFBAudioPlayerNode` object or `nil` if memory or resource allocation failed
- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels;
/// Returns an initialized `SFBAudioPlayerNode` object
/// - important: `format` must be standard
/// - parameter format: The format supplied by the render block
/// - returns: An initialized `SFBAudioPlayerNode` object or `nil` if memory or resource allocation failed
- (instancetype)initWithFormat:(AVAudioFormat *)format;
/// Returns an initialized `SFBAudioPlayerNode` object
/// - important: `format` must be standard
/// - parameter format: The format supplied by the render block
/// - parameter ringBufferSize: The desired minimum ring buffer size, in frames.
/// - returns: An initialized `SFBAudioPlayerNode` object or `nil` if memory or resource allocation failed
- (instancetype)initWithFormat:(AVAudioFormat *)format ringBufferSize:(uint32_t)ringBufferSize NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRenderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;
- (instancetype)initWithFormat:(AVAudioFormat *)format renderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;

#pragma mark - Format Information

/// Returns the format supplied by this object's render block
@property (nonatomic, readonly) AVAudioFormat * renderingFormat;
/// Returns `YES` if audio with `format` can be played
/// - parameter format: A format to test for support
/// - returns: `YES` if `format` has the same number of channels and sample rate as the rendering format
- (BOOL)supportsFormat:(AVAudioFormat *)format;

#pragma mark - Queue Management

/// Cancels the current decoder, clears any queued decoders, and creates and enqueues a decoder for subsequent playback
/// - note: This is equivalent to `-reset` followed by `-enqueueURL:error:`
/// - parameter url: The URL to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(resetAndEnqueue(_:));
/// Cancels the current decoder, clears any queued decoders, and enqueues a decoder for subsequent playback
/// - note: This is equivalent to `-reset` followed by `-enqueueDecoder:error:`
/// - parameter decoder: The decoder to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)resetAndEnqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(resetAndEnqueue(_:));

/// Creates and enqueues a decoder for subsequent playback
/// - note: This is equivalent to creating an `SFBAudioDecoder` object for `url` and passing that object to `-enqueueDecoder:error:`
/// - parameter url: The URL to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if a decoder was created and enqueued successfully
- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
/// Enqueues a decoder for subsequent playback
/// - parameter decoder: The decoder to enqueue
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the decoder was enqueued successfully
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

/// Removes and returns the next decoder from the decoder queue
/// - returns: The next decoder from the decoder queue or `nil` if none
- (nullable id <SFBPCMDecoding>)dequeueDecoder;

/// Returns the decoder supplying the earliest audio frame for the next render cycle or `nil` if none
/// - warning: Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> currentDecoder;
/// Cancels the current decoder
- (void)cancelCurrentDecoder;

/// Empties the decoder queue
- (void)clearQueue;

/// Returns `YES` if the decoder queue is empty
@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

/// Begins pushing audio from the current decoder
- (void)play;
/// Pauses audio from the current decoder and pushes silence
- (void)pause;
/// Cancels the current decoder, clears any queued decoders, and pushes silence
- (void)stop;
/// Toggles the playback state
- (void)togglePlayPause;

#pragma mark - State

 /// Returns `YES` if the `SFBAudioPlayerNode` is playing
@property (nonatomic, readonly) BOOL isPlaying;

 /// Returns `YES` if a decoder is available to supply audio for the next render cycle
@property (nonatomic, readonly) BOOL isReady;

#pragma mark - Playback Properties

/// Returns the playback position in the current decoder or `{SFBUnknownFramePosition, SFBUnknownFrameLength}` if the current decoder is `nil`
@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackPosition playbackPosition NS_REFINED_FOR_SWIFT;
/// Returns the playback time in the current decoder or `{SFBUnknownTime, SFBUnknownTime}` if the current decoder is `nil`
@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackTime playbackTime NS_REFINED_FOR_SWIFT;

/// Retrieves the playback position and time
/// - parameter playbackPosition: An optional pointer to an `SFBAudioPlayerNodePlaybackPosition` struct to receive playback position information
/// - parameter playbackTime: An optional pointer to an `SFBAudioPlayerNodePlaybackTime` struct to receive playback time information
/// - returns: `NO` if the current decoder is `nil`
- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerNodePlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerNodePlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

#pragma mark - Seeking

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

#pragma mark - Delegate

/// An optional delegate
@property (nonatomic, nullable, weak) id<SFBAudioPlayerNodeDelegate> delegate;
/// The dispatch queue on which the `SFBAudioPlayerNode` messages the delegate
@property (nonatomic, readonly) dispatch_queue_t delegateQueue;

@end

#pragma mark - SFBAudioPlayerNodeDelegate

/// Delegate methods supported by `SFBAudioPlayerNode`
NS_SWIFT_NAME(AudioPlayerNode.Delegate) @protocol SFBAudioPlayerNodeDelegate <NSObject>
@optional
/// Called to notify the delegate before decoding the first frame of audio
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object processing `decoder`
/// - parameter decoder: The decoder for which decoding started
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate after decoding the final frame of audio
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object processing `decoder`
/// - parameter decoder: The decoder for which decoding is complete
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that decoding has been canceled
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object processing `decoder`
/// - parameter decoder: The decoder for which decoding is canceled
/// - parameter partiallyRendered: `YES` if any audio frames from `decoder` were rendered
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingCanceled:(id<SFBPCMDecoding>)decoder partiallyRendered:(BOOL)partiallyRendered;
/// Called to notify the delegate that the first audio frame from `decoder` will render at `hostTime`
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object processing `decoder`
/// - parameter decoder: The decoder for which rendering will start
/// - parameter hostTime: The host time at which the first audio frame from `decoder` will reach the device
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayerNode(_:renderingWillStart:at:));
/// Called to notify the delegate that the final audio frame from `decoder` will render at `hostTime`
/// - warning: Do not change any properties of `decoder`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object processing `decoder`
/// - parameter decoder: The decoder for which rendering will complete
/// - parameter hostTime: The host time at which the final audio frame from `decoder` will reach the device
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillComplete:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayerNode(_:renderingWillComplete:at:));
/// Called to notify the delegate that rendering will complete for all available decoders at `hostTime`
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object
/// - parameter hostTime: The host time at which the final audio frame will reach the device
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode audioWillEndAtHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayerNode(_:audioWillEndAt:));
/// Called to notify the delegate when an asynchronous error occurs
/// - parameter audioPlayerNode: The `SFBAudioPlayerNode` object
/// - parameter error: The error
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode encounteredError:(NSError *)error;
@end

#pragma mark - Error Information

/// The `NSErrorDomain` used by `SFBAudioPlayerNode`
extern NSErrorDomain const SFBAudioPlayerNodeErrorDomain NS_SWIFT_NAME(AudioPlayerNode.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioPlayerNode`
typedef NS_ERROR_ENUM(SFBAudioPlayerNodeErrorDomain, SFBAudioPlayerNodeErrorCode) {
	/// Internal or unspecified error
	SFBAudioPlayerNodeErrorCodeInternalError 		= 0,
	/// Format not supported
	SFBAudioPlayerNodeErrorCodeFormatNotSupported 	= 1,
} NS_SWIFT_NAME(AudioPlayerNode.ErrorCode);

NS_ASSUME_NONNULL_END
