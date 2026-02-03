//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <Foundation/Foundation.h>

CF_EXTERN_C_BEGIN

/// Returns an `NSError` object with the `NSLocalizedDescriptionKey` set to `format` after formatting according to the
/// current locale
NSError *_Nonnull SFBErrorWithLocalizedDescription(NSErrorDomain _Nonnull domain, NSInteger code,
                                                   NSString *_Nonnull format, NSDictionary *_Nullable userInfo, ...)
        NS_FORMAT_FUNCTION(3, 5);

CF_EXTERN_C_END
