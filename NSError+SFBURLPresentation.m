/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "NSError+SFBURLPresentation.h"

@implementation NSError (SFBURLDisplayNameMethods)

+ (instancetype)SFB_errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code descriptionFormatStringForURL:(NSString *)descriptionFormatStringForURL url:(NSURL *)url failureReason:(NSString *)failureReason recoverySuggestion:(NSString *)recoverySuggestion
{
	NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];

	NSString *displayName = nil;
	if([url getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:nil])
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, displayName];
	else
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, url.lastPathComponent];

	userInfo[NSLocalizedFailureReasonErrorKey] = failureReason;
	userInfo[NSLocalizedRecoverySuggestionErrorKey] = recoverySuggestion;

	return [NSError errorWithDomain:domain code:code userInfo:userInfo];
}

@end
