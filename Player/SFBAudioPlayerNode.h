/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBPCMDecoding.h"

NS_ASSUME_NONNULL_BEGIN

// ========================================
// Playback position and time information
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

// ========================================
// Event types
typedef void (^SFBAudioDecoderEventBlock)(id <SFBPCMDecoding> decoder);
typedef void (^SFBAudioDecoderErrorBlock)(id <SFBPCMDecoding> decoder, NSError *error);
typedef void (^SFBAudioPlayerNodeErrorBlock)(NSError *error);

#pragma mark - SFBAudioPlayerNode

/// @brief An `AVAudioSourceNode` supporting gapless playback for PCM formats
///
/// `SFBAudioPlayerNode` decodes audio into a ring buffer and renders on demand.
/// `SFBAudioPlayerNode` supports seeking and playback control for all Decoder subclasses supported by the current Output.
///
/// Decoding occurs in a high priority (non-realtime) thread which reads audio via a `SFBPCMDecoder` instance and stores it in the ring buffer.
///
/// Rendering occurs in a realtime thread when the render block is called by the owning `AVAudioEngine`.
///
/// Since decoding and rendering are distinct operations performed in separate threads, a GCD timer on the background queue is
/// used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
/// rendering is complete, which cannot occur until after decoding is complete.
///
/// `SFBAudioPlayerNode` supports block-based callbacks for the following events:
///  1. Decoding started
///  2. Decoding finished
///  3. Rendering started
///  4. Rendering finished
///  6. Decoding errors
///  7. Other errors
///
/// All callbacks are performed on a dedicated notification queue.
NS_SWIFT_NAME(AudioPlayerNode ) @interface SFBAudioPlayerNode: AVAudioSourceNode

- (instancetype)initWithFormat:(AVAudioFormat *)format NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRenderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;
- (instancetype)initWithFormat:(AVAudioFormat *)format renderBlock:(AVAudioSourceNodeRenderBlock)block NS_UNAVAILABLE;

#pragma mark - Format Information

@property (nonatomic, readonly) AVAudioFormat * renderingFormat;
- (BOOL)supportsFormat:(AVAudioFormat *)format;

#pragma mark - Playlist Management

- (BOOL)playDecoder:(id <SFBPCMDecoding> )decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));
- (BOOL)enqueueDecoder:(id <SFBPCMDecoding> )decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

- (void)skipToNext;
- (void)clearQueue;

#pragma mark - Playback Control

- (void)play;
- (void)pause;
- (void)playPause;
- (void)stop;

#pragma mark - State

@property (nonatomic, readonly) BOOL isPlaying;
@property (nonatomic, nullable, readonly) NSURL *url; //!< Returns the url of the  rendering decoder's  input source  or \c nil if none
@property (nonatomic, nullable, readonly) id representedObject; //!< Returns the represented object of the rendering decoder or \c nil if none

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
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderErrorBlock decodingErrorNotificationHandler;
@property (nonatomic, nullable) SFBAudioPlayerNodeErrorBlock errorNotificationHandler;

@end

NS_ASSUME_NONNULL_END
