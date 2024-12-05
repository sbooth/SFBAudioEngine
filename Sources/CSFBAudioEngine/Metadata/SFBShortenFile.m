//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBShortenFile.h"

#import "SFBFileInputSource.h"
#import "SFBAudioDecoder.h"

#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"
#import "NSFileHandle+SFBHeaderReading.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameShorten = @"org.sbooth.AudioEngine.File.Shorten";

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

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameShorten;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [fileHandle readHeaderOfLength:SFBShortenDetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isShortenHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	SFBInputSource *inputSource = [[SFBFileInputSource alloc] initWithURL:self.url error:error];
	if(!inputSource || ![inputSource openReturningError:error])
		return NO;

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithInputSource:inputSource decoderName:SFBAudioDecoderNameShorten error:error];
	if(!decoder || ![decoder openReturningError:error])
		return NO;

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"Shorten" forKey:SFBAudioPropertiesKeyFormatName];

	AVAudioFormat *format = decoder.processingFormat;
	[propertiesDictionary setObject:@(format.sampleRate) forKey:SFBAudioPropertiesKeySampleRate];
	[propertiesDictionary setObject:@(format.channelCount) forKey:SFBAudioPropertiesKeyChannelCount];
	[propertiesDictionary setObject:@(format.streamDescription->mBitsPerChannel) forKey:SFBAudioPropertiesKeyBitDepth];
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
