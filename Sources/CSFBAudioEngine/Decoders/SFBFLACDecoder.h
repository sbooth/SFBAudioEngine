//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioDecoder+Internal.h"

NS_ASSUME_NONNULL_BEGIN

// An SFBAudioDecoder subclass supporting FLAC
@interface SFBFLACDecoder : SFBAudioDecoder
@end

// An SFBAudioDecoder subclass supporting Ogg FLAC
@interface SFBOggFLACDecoder : SFBFLACDecoder
@end

NS_ASSUME_NONNULL_END
