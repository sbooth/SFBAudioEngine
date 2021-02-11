//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBShortenFile.h"

#import "SFBFileInputSource.h"
#import "SFBShortenDecoder.h"

#import "NSError+SFBURLPresentation.h"

@implementation SFBShortenFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"shn"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-shorten"];
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	SFBInputSource *inputSource = [[SFBFileInputSource alloc] initWithURL:self.url error:error];
	if(!inputSource || ![inputSource openReturningError:error])
		return NO;

	SFBShortenDecoder *decoder = [[SFBShortenDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder || ![decoder openReturningError:error])
		return NO;

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"Shorten" forKey:SFBAudioPropertiesKeyFormatName];

	AVAudioFormat *format = decoder.processingFormat;
	[propertiesDictionary setObject:@(format.sampleRate) forKey:SFBAudioPropertiesKeySampleRate];
	[propertiesDictionary setObject:@(format.channelCount) forKey:SFBAudioPropertiesKeyChannelCount];
	[propertiesDictionary setObject:@(format.streamDescription->mBitsPerChannel) forKey:SFBAudioPropertiesKeyBitsPerChannel];
	[propertiesDictionary setObject:@(decoder.frameLength) forKey:SFBAudioPropertiesKeyFrameLength];
	[propertiesDictionary setObject:@(decoder.frameLength / format.sampleRate) forKey:SFBAudioPropertiesKeyDuration];

	self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
	self.metadata = [[SFBAudioMetadata alloc] init];
	return YES;
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	os_log_error(gSFBAudioFileLog, "Writing Shorten metadata is not supported");

	if(error)
		*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
										 code:SFBAudioFileErrorCodeInputOutput
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
										  url:self.url
								failureReason:NSLocalizedString(@"Unable to write metadata", @"")
						   recoverySuggestion:NSLocalizedString(@"Writing Shorten metadata is not supported.", @"")];
	return NO;
}

@end
