//
// Copyright (c) 2020-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSError+SFBURLPresentation.h"

@implementation NSError (SFBURLPresentation)

+ (instancetype)SFB_errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code descriptionFormatStringForURL:(NSString *)descriptionFormatStringForURL url:(NSURL *)url failureReason:(NSString *)failureReason recoverySuggestion:(NSString *)recoverySuggestion
{
	NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];

	NSString *displayName = nil;
	if([url getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:nil] && displayName)
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, displayName];
	else
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, url.lastPathComponent];

	userInfo[NSLocalizedFailureReasonErrorKey] = failureReason;
	userInfo[NSLocalizedRecoverySuggestionErrorKey] = recoverySuggestion;

	userInfo[NSURLErrorKey] = url;

	return [NSError errorWithDomain:domain code:code userInfo:userInfo];
}

@end
