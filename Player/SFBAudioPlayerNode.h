/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBPCMDecoding.h"

NS_ASSUME_NONNULL_BEGIN

#pragma mark - Playback position and time information

struct NS_SWIFT_NAME(PlaybackPosition) SFBAudioPlayerNodePlaybackPosition {
	AVAudioFramePosition framePosition NS_SWIFT_NAME(current);
	AVAudioFramePosition frameLength NS_SWIFT_NAME(total);
};
typedef struct SFBAudioPlayerNodePlaybackPosition SFBAudioPlayerNodePlaybackPosition;

struct NS_SWIFT_NAME(PlaybackTime) SFBAudioPlayerNodePlaybackTime {
	NSTimeInterval currentTime NS_SWIFT_NAME(current);
	NSTimeInterval totalTime NS_SWIFT_NAME(total);
};
typedef struct SFBAudioPlayerNodePlaybackTime SFBAudioPlayerNodePlaybackTime;

#pragma mark - Event types

typedef void (^SFBAudioDecoderEventBlock)(id <SFBPCMDecoding> decoder);

#pragma mark - SFBAudioPlayerNode

/// @brief An \c AVAudioSourceNode supporting gapless playback for PCM formats
///
/// The output format of \c SFBAudioPlayerNode is specified at object creation and cannot be changed. The output format must be
/// a flavor of non-interleaved PCM audio.
///
/// \c SFBAudioPlayerNode is supplied by objects implementing \c SFBPCMDecoding  (decoders) and supports audio at the same sample rate
/// and with the same number of channels as the output format.
/// \c SFBAudioPlayerNode supports seeking when supported by the decoder's input source.
///
/// \c SFBAudioPlayerNode decodes audio in a high priority (non-realtime) thread into a ring buffer and renders on demand.
/// Rendering occurs in a realtime thread when the render block is called.
///
/// Since decoding and rendering are distinct operations performed in separate threads, a GCD timer on the background queue is
/// used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
/// rendering is complete, which cannot occur until after decoding is complete.
///
/// \c SFBAudioPlayerNode supports block-based callbacks for the following events:
///  1. Decoding started
///  2. Decoding finished
///  3. Decoding canceled
///  4. Rendering started
///  5. Rendering finished
///
/// All callbacks are performed on a dedicated notification queue.
NS_SWIFT_NAME(AudioPlayerNode ) @interface SFBAudioPlayerNode : AVAudioSourceNode

- (instancetype)initWithFormat:(AVAudioFormat *)format NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRenderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;
- (instancetype)initWithFormat:(AVAudioFormat *)format renderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;

#pragma mark - Format Information

@property (nonatomic, readonly) AVAudioFormat * renderingFormat;
- (BOOL)supportsFormat:(AVAudioFormat *)format;

#pragma mark - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
- (BOOL)playDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

- (void)skipToNext;
- (void)clearQueue;

@property (nonatomic, readonly) BOOL queueIsEmpty;

#pragma mark - Playback Control

- (void)play;
- (void)pause;
- (void)playPause;
- (void)stop;

#pragma mark - State

@property (nonatomic, readonly) BOOL isPlaying; ///< Returns \c YES if the SFBAudioPlayerNode is playing
@property (nonatomic, nullable, readonly) NSURL *url; ///< Returns the url of the  rendering decoder's  input source  or \c nil if none
@property (nonatomic, readonly, nullable) id <SFBPCMDecoding> decoder; ///< Returns the  rendering decoder  or \c nil if none. @warning Do not change any properties of the returned object

#pragma mark - Playback Properties

@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackPosition playbackPosition NS_SWIFT_NAME(position);
@property (nonatomic, readonly) SFBAudioPlayerNodePlaybackTime playbackTime NS_SWIFT_NAME(time);

- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerNodePlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerNodePlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

#pragma mark - Seeking

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(forward:));
- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(backward:));

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds NS_SWIFT_NAME(seek(time:));
- (BOOL)seekToPosition:(float)position NS_SWIFT_NAME(seek(position:));
- (BOOL)seekToFrame:(AVAudioFramePosition)frame NS_SWIFT_NAME(seek(frame:));

@property (nonatomic, readonly) BOOL supportsSeeking;

#pragma mark - Event Callbacks

@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingCanceledNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingFinishedNotificationHandler;

@end

NS_ASSUME_NONNULL_END
