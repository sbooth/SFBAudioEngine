/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import OSLog;

#import "SFBAudioFile.h"
#import "SFBAudioFile+Internal.h"

// NSError domain for AudioFile and subclasses
NSErrorDomain const SFBAudioFileErrorDomain = @"org.sbooth.AudioEngine.AudioFile";

@implementation SFBAudioFile

#pragma mark Supported File Formats

+ (NSSet<NSString *> *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in self.registeredInputOutputHandlers) {
		NSSet *supportedPathExtensions = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}

	return result;
}

+ (NSSet<NSString *> *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in self.registeredInputOutputHandlers) {
		NSSet *supportedMIMETypes = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}

	return result;
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	return [self.supportedPathExtensions containsObject:extension.lowercaseString];
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	return [self.supportedMIMETypes containsObject:mimeType.lowercaseString];
}

#pragma mark Creation

+ (instancetype)audioFileWithURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioFile *audioFile = [[SFBAudioFile alloc] initWithURL:url];
	if(![audioFile readPropertiesAndMetadataReturningError:error])
		return nil;
	return audioFile;
}

- (instancetype)initWithURL:(NSURL *)url
{
	if((self = [super init]))
		self.url = url;
	return self;
}

#pragma mark Reading and Writing

- (BOOL)readPropertiesAndMetadataReturningError:(NSError * _Nullable *)error
{
	id<SFBAudioFileInputOutputHandling> handler = [SFBAudioFile inputOutputHandlerForURL:self.url];
	return [handler readAudioPropertiesAndMetadataFromURL:self.url toAudioFile:self error:error];
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	id<SFBAudioFileInputOutputHandling> handler = [SFBAudioFile inputOutputHandlerForURL:self.url];
	return [handler writeAudioMetadata:self.metadata toURL:self.url error:error];
}

@end
