/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

//! An SFBAudioDecoder subclass supporting decoding a repeating segment of a decoder
NS_SWIFT_NAME(LoopableRegionDecoder) @interface SFBLoopableRegionDecoder : SFBAudioDecoder
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
