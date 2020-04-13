/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

// Playback position and time information
struct NS_SWIFT_NAME(PlaybackPosition) SFBAudioPlayerPlaybackPosition {
	AVAudioFramePosition framePosition NS_SWIFT_NAME(current);
	AVAudioFramePosition frameLength NS_SWIFT_NAME(total);
};
typedef struct SFBAudioPlayerPlaybackPosition SFBAudioPlayerPlaybackPosition;

struct NS_SWIFT_NAME(PlaybackTime) SFBAudioPlayerPlaybackTime {
	NSTimeInterval currentTime NS_SWIFT_NAME(current);
	NSTimeInterval totalTime NS_SWIFT_NAME(total);
};
typedef struct SFBAudioPlayerPlaybackTime SFBAudioPlayerPlaybackTime;

// Audio player event types
typedef void (^SFBAudioDecoderEventBlock)(SFBAudioDecoder *decoder);
typedef void (^SFBAudioDecoderErrorBlock)(SFBAudioDecoder *decoder, NSError *error);
typedef void (^SFBAudioPlayerErrorBlock)(NSError *error);

typedef void (^SFBAudioPlayerAVAudioEngineBlock)(AVAudioEngine *engine);

//! An audio player
NS_SWIFT_NAME(AudioPlayer) @interface SFBAudioPlayer: NSObject

- (nullable instancetype)init NS_DESIGNATED_INITIALIZER;

// ========================================
// Playlist Management
- (BOOL)playURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(play(_:));
- (BOOL)playDecoder:(SFBAudioDecoder *)decoder error:(NSError **)error NS_SWIFT_NAME(play(_:));

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));
- (BOOL)enqueueDecoder:(SFBAudioDecoder *)decoder error:(NSError **)error NS_SWIFT_NAME(enqueue(_:));

- (BOOL)skipToNext;
- (void)clearQueue;

// ========================================
// Playback Control
- (BOOL)playReturningError:(NSError **)error NS_SWIFT_NAME(play());
- (BOOL)pauseReturningError:(NSError **)error NS_SWIFT_NAME(pause());
- (BOOL)stopReturningError:(NSError **)error NS_SWIFT_NAME(stop());
- (BOOL)playPauseReturningError:(NSError **)error NS_SWIFT_NAME(playPause());

// ========================================
// Player State
@property (nonatomic, readonly) BOOL isRunning; //!< Returns \c YES if the AVAudioEngine is running
@property (nonatomic, readonly) BOOL isPlaying; //!< Returns \c YES if the  AVAudioPlayerNode is playing
@property (nonatomic, nullable, readonly) NSURL *url; //!< Returns the url of the  rendering decoder's  input source  or \c nil if none
@property (nonatomic, nullable, readonly) id representedObject; //!< Returns the represented object of the rendering decoder or \c nil if none

// ========================================
// Playback Properties
@property (nonatomic, readonly) AVAudioFramePosition framePosition;
@property (nonatomic, readonly) AVAudioFramePosition frameLength;
@property (nonatomic, readonly) SFBAudioPlayerPlaybackPosition playbackPosition NS_SWIFT_NAME(position);

@property (nonatomic, readonly) NSTimeInterval currentTime;
@property (nonatomic, readonly) NSTimeInterval totalTime;
@property (nonatomic, readonly) SFBAudioPlayerPlaybackTime playbackTime NS_SWIFT_NAME(time);

- (BOOL)getPlaybackPosition:(nullable SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(nullable SFBAudioPlayerPlaybackTime *)playbackTime NS_REFINED_FOR_SWIFT;

// ========================================
// Seeking
- (BOOL)seekForward;
- (BOOL)seekBackward;

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(forward:));
- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip NS_SWIFT_NAME(seek(backward:));

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds NS_SWIFT_NAME(seek(time:));
- (BOOL)seekToPosition:(float)position NS_SWIFT_NAME(seek(position:));
- (BOOL)seekToFrame:(AVAudioFramePosition)frame NS_SWIFT_NAME(seek(frame:));

@property (nonatomic, readonly) BOOL supportsSeeking;

// ========================================
// Player Event Callbacks
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock decodingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock schedulingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock schedulingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingStartedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderEventBlock renderingFinishedNotificationHandler;
@property (nonatomic, nullable) SFBAudioDecoderErrorBlock decodingErrorNotificationHandler;
@property (nonatomic, nullable) SFBAudioPlayerErrorBlock errorNotificationHandler;

// ========================================
// AVAudioEngine Access
- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block;

@end

NS_ASSUME_NONNULL_END
