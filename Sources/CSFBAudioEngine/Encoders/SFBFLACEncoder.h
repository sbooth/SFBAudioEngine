//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
