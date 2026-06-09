//
// SPDX-FileCopyrightText: 2014 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBDSDDecoder+Internal.h"

NS_ASSUME_NONNULL_BEGIN

// An SFBDSDDecoder subclass supporting DSF (DSD stream files)
// See http://dsd-guide.com/sites/default/files/white-papers/DSFFileFormatSpec_E.pdf
@interface SFBDSFDecoder : SFBDSDDecoder
@end

NS_ASSUME_NONNULL_END
