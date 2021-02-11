//
// Copyright (c) 2006 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// A class supporting a repeating segment of a decoder
NS_SWIFT_NAME(LoopableRegionDecoder) @interface SFBLoopableRegionDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBLoopableRegionDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized \c SFBLoopableRegionDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param repeatCount The number of times to repeat
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized \c SFBLoopableRegionDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized \c SFBLoopableRegionDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param repeatCount The number of times to repeat
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized \c SFBLoopableRegionDecoder object for the given decoder or \c nil on failure
/// @param decoder The decoder
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified decoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized \c SFBLoopableRegionDecoder object for the given decoder or \c nil on failure
/// @param decoder The decoder
/// @param framePosition The starting frame position
/// @param frameLength The number of frames to play
/// @param repeatCount The number of times to repeat
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBLoopableRegionDecoder object for the specified decoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
