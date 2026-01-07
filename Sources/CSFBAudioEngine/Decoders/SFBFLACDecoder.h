//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBPCMDecoder+Internal.h"

NS_ASSUME_NONNULL_BEGIN

// An SFBPCMDecoder subclass supporting FLAC
@interface SFBFLACDecoder : SFBPCMDecoder
@end

// An SFBPCMDecoder subclass supporting Ogg FLAC
@interface SFBOggFLACDecoder : SFBFLACDecoder
@end

NS_ASSUME_NONNULL_END
