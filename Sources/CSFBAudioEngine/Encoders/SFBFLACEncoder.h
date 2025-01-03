//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioEncoder+Internal.h"

NS_ASSUME_NONNULL_BEGIN

// An SFBAudioEncoder subclass supporting FLAC
@interface SFBFLACEncoder : SFBAudioEncoder
@end

// An SFBAudioEncoder subclass supporting Ogg FLAC
@interface SFBOggFLACEncoder : SFBFLACEncoder
@end

NS_ASSUME_NONNULL_END
