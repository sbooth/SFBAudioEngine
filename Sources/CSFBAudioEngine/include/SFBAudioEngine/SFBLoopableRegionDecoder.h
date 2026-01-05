//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// A class supporting a repeating segment of a decoder
NS_SWIFT_NAME(LoopableRegionDecoder) @interface SFBLoopableRegionDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBLoopableRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBLoopableRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter repeatCount: The number of times to repeat
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized `SFBLoopableRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBLoopableRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter repeatCount: The number of times to repeat
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized `SFBLoopableRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBLoopableRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter framePosition: The starting frame position
/// - parameter frameLength: The number of frames to play
/// - parameter repeatCount: The number of times to repeat
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBLoopableRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
