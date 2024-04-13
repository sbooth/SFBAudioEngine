//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioPlayerNodeDelegate;

#pragma mark - Playback position and time information

/// Playback position information for \c SFBAudioPlayerNode
struct SFBAudioPlayerNodePlaybackPosition {
	/// The current frame position or \c SFBUnknownFramePosition if unknown
	AVAudioFramePosition framePosition;
	/// The total number of frames or \c SFBUnknownFrameLength if unknown
	AVAudioFramePosition frameLength;
} /*NS_SWIFT_UNAVAILABLE("Use AudioPlayerNode.PlaybackPosition instead")*/;
typedef struct SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerNodePlaybackPosition;

/// Playback time information for \c SFBAudioPlayerNode
struct SFBAudioPlayerNodePlaybackTime {
	/// The current time or \c SFBUnknownTime if unknown
	NSTimeInterval currentTime;
	/// The total time or \c SFBUnknownTime if unknown
	NSTimeInterval totalTime;
} /*NS_SWIFT_UNAVAILABLE("Use AudioPlayerNode.PlaybackTime instead")*/;
typedef struct SFBAudioPlayerNodePlaybackTime SFBAudioPlayerNodePlaybackTime;

#pragma mark - SFBAudioPlayerNode

/// An \c AVAudioSourceNode supporting gapless playback for PCM formats
///
/// The output format of \c SFBAudioPlayerNode is specified at object initialization and cannot be changed. The output
/// format must be the standard format (deinterleaved native-endian 32-bit floating point PCM) at any sample rate with
/// any number of channels.
///
/// \c SFBAudioPlayerNode is supplied by objects implementing \c SFBPCMDecoding (decoders) and supports audio at the
/// same sample rate and with the same number of channels as the output format. \c SFBAudioPlayerNode supports seeking
/// when supported by the decoder.
///
/// \c SFBAudioPlayerNode maintains a current decoder and a queue of pending decoders. The current decoder is the
/// decoder that will supply the earliest audio frame in the next render cycle when playing. Pending decoders are
/// automatically dequeued and become current when the final frame of the current decoder is pushed in the render block.
///
/// \c SFBAudioPlayerNode decodes audio in a high-priority non-realtime thread into a ring buffer and renders on
/// demand. Rendering occurs in a realtime thread when the render block is called; the render block always supplies
/// audio. When playback is paused or insufficient audio is available the render block outputs silence.
///
/// Since decoding and rendering are distinct operations performed in separate threads, a GCD source on a background
/// queue is used for garbage collection. This is necessary because state data created in the decoding thread needs to
/// live until rendering is complete, which cannot occur until after decoding is complete.
///
/// \c SFBAudioPlayerNode supports delegate-based callbacks for the following events:
///
///  1. Decoding started
///  2. Decoding complete
///  3. Decoding canceled
///  4. Rendering will start
///  5. Rendering started
///  6. Rendering complete
///  7. End of audio
///
/// All callbacks are performed on a dedicated notification queue.
NS_SWIFT_NAME(AudioPlayerNode) @interface SFBAudioPlayerNode : AVAudioSourceNode

/// Returns an initialized \c SFBAudioPlayerNode object for stereo audio at 44,100 Hz
- (instancetype)init;
/// Returns an initialized \c SFBAudioPlayerNode object for audio with a specified number of channels and sample rate
/// @param sampleRate The sample rate supplied by the render block
/// @param channels The number of channels supplied by the render block
/// @return An initialized \c SFBAudioPlayerNode object or \c nil if memory or resource allocation failed
- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels;
/// Returns an initialized \c SFBAudioPlayerNode object
/// @note \c format must be standard
/// @param format The format supplied by the render block
/// @return An initialized \c SFBAudioPlayerNode object or \c nil if memory or resource allocation failed
- (instancetype)initWithFormat:(AVAudioFormat *)format;
/// Returns an initialized \c SFBAudioPlayerNode object
/// @note \c format must be standard
/// @param format The format supplied by the render block
/// @param ringBufferSize The desired minimum ring buffer size, in frames.
/// @return An initialized \c SFBAudioPlayerNode object or \c nil if memory or resource allocation failed
- (instancetype)initWithFormat:(AVAudioFormat *)format ringBufferSize:(uint32_t)ringBufferSize NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRenderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;
- (instancetype)initWithFormat:(AVAudioFormat *)format renderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;

#pragma mark - Format Information

/// Returns the format supplied by this object's render block
@property (nonatomic, readonly) AVAudioFormat * renderingFormat;
/// Returns \c YES if audio with \c format can be played
/// @param format A format to test for support
/// @return \c YES if \c format has the same number of channels and sample rate as the rendering format
- (BOOL)supportsFormat:(AVAudioFormat *)format;

#pragma mark - Queue Management

/// Cancels the current decoder, clears any queued decoders, and creates and enqueues a decoder for subsequent playback
/// @note This is equivalent to \c -reset followed by \c -enqueueURL:error:
/// @param url The URL to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if a decoder was created and enqueued successfully
- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(resetAndEnqueue(_:));
/// Cancels the current decoder, clears any queued decoders, and enqueues a decoder for subsequent playback
/// @note This is equivalent to \c -reset followed by \c -enqueueDecoder:error:
/// @param decoder The decoder to enqueue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the decoder was enqueued successfully
- (BOOL)resetAndEnqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(resetAndEnqueue(_:));

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

/// Removes and returns the next decoder from the decoder queue
/// @return The next decoder from the decoder queue or \c nil if none
- (nullable id <SFBPCMDecoding>)dequeueDecoder;

/// Returns the decoder supplying the earliest audio frame for the next render cycle or \c nil if none
/// @warning Do not change any properties of the returned object
@property (nonatomic, nullable, readonly) id <SFBPCMDecoding> currentDecoder;
/// Cancels the current decoder
- (void)cancelCurrentDecoder;

/// Empties the decoder queue
- (void)clearQueue;

/// Returns \c YES if the decoder queue is empty
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

 /// Returns \c YES if the \c SFBAudioPlayerNode is playing
@property (nonatomic, readonly) BOOL isPlaying;

 /// Returns \c YES if a decoder is available to supply audio for the next render cycle
@property (nonatomic, readonly) BOOL isReady;

#pragma mark - Playback Properties

/// Returns the playback position in the current decoder or \c {SFBUnknownFramePosition, \c SFBUnknownFrameLength} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackPosition playbackPosition NS_REFINED_FOR_SWIFT;
/// Returns the playback time in the current decoder or \c {SFBUnknownTime, \c SFBUnknownTime} if the current decoder is \c nil
@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackTime playbackTime NS_REFINED_FOR_SWIFT;

/// Retrieves the playback position and time
/// @param playbackPosition An optional pointer to an \c SFBAudioPlayerNodePlaybackPosition struct to receive playback position information
/// @param playbackTime An optional pointer to an \c SFBAudioPlayerNodePlaybackTime struct to receive playback time information
/// @return \c NO if the current decoder is \c nil
- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerNodePlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerNodePlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

#pragma mark - Seeking

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

#pragma mark - Delegate

/// An optional delegate
@property (nonatomic, nullable, weak) id<SFBAudioPlayerNodeDelegate> delegate;

@end

#pragma mark - SFBAudioPlayerNodeDelegate

/// Delegate methods supported by \c SFBAudioPlayerNode
NS_SWIFT_NAME(AudioPlayerNode.Delegate) @protocol SFBAudioPlayerNodeDelegate <NSObject>
@optional
/// Called to notify the delegate before decoding the first frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing \c decoder
/// @param decoder The decoder for which decoding started
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate after decoding the final frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing \c decoder
/// @param decoder The decoder for which decoding is complete
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate that decoding has been canceled
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing \c decoder
/// @param decoder The decoder for which decoding is canceled
/// @param partiallyRendered \c YES if any audio frames from \c decoder were rendered
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode decodingCanceled:(id<SFBPCMDecoding>)decoder partiallyRendered:(BOOL)partiallyRendered;
/// Called to notify the delegate that audio will soon begin rendering
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing \c decoder
/// @param decoder The decoder for which rendering is about to start
/// @param hostTime The host time at which the first audio frame from \c decoder will reach the device
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingWillStart:(id<SFBPCMDecoding>)decoder atHostTime:(uint64_t)hostTime NS_SWIFT_NAME(audioPlayerNode(_:renderingWillStart:at:));
/// Called to notify the delegate when rendering the first frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing decoder
/// @param decoder The decoder for which rendering started
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingStarted:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when rendering the final frame of audio
/// @warning Do not change any properties of \c decoder
/// @param audioPlayerNode The \c SFBAudioPlayerNode object processing \c decoder
/// @param decoder The decoder for which rendering is complete
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode renderingComplete:(id<SFBPCMDecoding>)decoder;
/// Called to notify the delegate when rendering is complete for all available decoders
/// @param audioPlayerNode The \c SFBAudioPlayerNode object
- (void)audioPlayerNodeEndOfAudio:(SFBAudioPlayerNode *)audioPlayerNode NS_SWIFT_NAME(audioPlayerNodeEndOfAudio(_:));
/// Called to notify the delegate when an asynchronous error occurs
/// @param audioPlayerNode The \c SFBAudioPlayerNode object
/// @param error The error
- (void)audioPlayerNode:(SFBAudioPlayerNode *)audioPlayerNode encounteredError:(NSError *)error;
@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioPlayerNode
extern NSErrorDomain const SFBAudioPlayerNodeErrorDomain NS_SWIFT_NAME(AudioPlayerNode.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioPlayerNode
typedef NS_ERROR_ENUM(SFBAudioPlayerNodeErrorDomain, SFBAudioPlayerNodeErrorCode) {
	/// Format not supported
	SFBAudioPlayerNodeErrorFormatNotSupported	= 0
} NS_SWIFT_NAME(AudioPlayerNode.ErrorCode);

NS_ASSUME_NONNULL_END
