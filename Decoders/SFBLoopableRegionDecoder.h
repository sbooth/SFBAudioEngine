/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBPCMDecoding.h"

NS_ASSUME_NONNULL_BEGIN

/// A class supporting a repeating segment of a decoder
NS_SWIFT_NAME(LoopableRegionDecoder) @interface SFBLoopableRegionDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
