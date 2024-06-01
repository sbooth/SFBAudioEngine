//
// Copyright (c) 2020-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Utility category
@interface NSError (SFBURLPresentation)
+ (instancetype)SFB_errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code descriptionFormatStringForURL:(NSString *)descriptionFormatStringForURL url:(NSURL *)url failureReason:(NSString *)failureReason recoverySuggestion:(NSString *)recoverySuggestion;
@end

NS_ASSUME_NONNULL_END
