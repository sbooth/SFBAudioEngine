//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
