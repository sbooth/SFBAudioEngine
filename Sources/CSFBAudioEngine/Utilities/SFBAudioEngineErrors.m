//
// Copyright (c) 2024-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioEngineErrors.h"

NSErrorDomain const SFBAudioEngineErrorDomain = @"org.sbooth.AudioEngine";

@interface SFBAudioEngineErrorDomainRegistration : NSObject
@end

@implementation SFBAudioEngineErrorDomainRegistration

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioEngineErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBAudioEngineErrorCodeInternalError:
					return NSLocalizedString(@"An internal or unspecified error occurred.", @"");
				case SFBAudioEngineErrorCodeFileNotFound:
					return NSLocalizedString(@"The requested file was not found.", @"");
				case SFBAudioEngineErrorCodeInputOutput:
					return NSLocalizedString(@"An input/output error occurred.", @"");
				case SFBAudioEngineErrorCodeInvalidFormat:
					return NSLocalizedString(@"The format is invalid or unknown.", @"");
				case SFBAudioEngineErrorCodeUnsupportedFormat:
					return NSLocalizedString(@"The format is recognized but not supported.", @"");
				case SFBAudioEngineErrorCodeFormatNotSupported:
					return NSLocalizedString(@"The format is not supported for this operation.", @"");
				case SFBAudioEngineErrorCodeUnknownDecoder:
					return NSLocalizedString(@"The decoder is unknown.", @"");
				case SFBAudioEngineErrorCodeDecodingError:
					return NSLocalizedString(@"A decoding error occurred.", @"");
				case SFBAudioEngineErrorCodeSeekError:
					return NSLocalizedString(@"A seek error occurred.", @"");
				case SFBAudioEngineErrorCodeUnknownEncoder:
					return NSLocalizedString(@"The encoder is unknown.", @"");
				case SFBAudioEngineErrorCodeUnknownFormatName:
					return NSLocalizedString(@"The format name is unknown.", @"");
				case SFBAudioEngineErrorCodeNotSeekable:
					return NSLocalizedString(@"The input source is not seekable.", @"");
				case SFBAudioEngineErrorCodeInsufficientSamples:
					return NSLocalizedString(@"Insufficient samples for analysis.", @"");
			}
		}
		return nil;
	}];
}

@end
