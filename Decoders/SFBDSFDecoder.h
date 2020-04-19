/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBDSDDecoder+Internal.h"

NS_ASSUME_NONNULL_BEGIN

// An SFBAudioDecoder subclass supporting DSF (DSD stream files)
// See http://dsd-guide.com/sites/default/files/white-papers/DSFFileFormatSpec_E.pdf
@interface SFBDSFDecoder : SFBDSDDecoder
@end

NS_ASSUME_NONNULL_END
