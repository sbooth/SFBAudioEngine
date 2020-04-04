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

static NSMutableArray *_registeredInputOutputHandlers = nil;

#pragma mark Supported File Formats

+ (NSSet<NSString *> *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedPathExtensions = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}

	return result;
}

+ (NSSet<NSString *> *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedMIMETypes = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}

	return result;
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedPathExtensions = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedMIMETypes = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:lowercaseMIMEType])
			return YES;
	}

	return NO;
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

@implementation SFBAudioFileInputOutputHandlerInfo
@end

@implementation SFBAudioFile (SFBAudioFileInputOutputHandling)

+ (void)registerInputOutputHandler:(Class)handler
{
	[self registerInputOutputHandler:handler priority:0];
}

+ (void)registerInputOutputHandler:(Class)handler priority:(int)priority
{
	NSAssert([handler conformsToProtocol:NSProtocolFromString(@"SFBAudioFileInputOutputHandling")], @"Unable to register class '%@' as an input/output handler because it does not conform to protocol SFBAudioFileInputOutputHandling", NSStringFromClass(handler));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredInputOutputHandlers = [NSMutableArray array];
	});

	SFBAudioFileInputOutputHandlerInfo *handlerInfo = [[SFBAudioFileInputOutputHandlerInfo alloc] init];
	handlerInfo.klass = handler;
	handlerInfo.priority = priority;

	[_registeredInputOutputHandlers addObject:handlerInfo];
	[_registeredInputOutputHandlers sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
		return ((SFBAudioFileInputOutputHandlerInfo *)obj1).priority < ((SFBAudioFileInputOutputHandlerInfo *)obj2).priority;
	}];
}

+ (id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForURL:(NSURL *)url
{
	// Use extension-based type resolvers for files
	NSString *scheme = url.scheme;

	if([scheme caseInsensitiveCompare:@"file"] != NSOrderedSame)
		return nil;

	// TODO: Handle MIME types?

	return [self inputOutputHandlerForPathExtension:url.pathExtension.lowercaseString];
}

+ (id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForPathExtension:(NSString *)extension
{
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedPathExtensions = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return [[handlerInfo.klass alloc] init];
	}

	return nil;
}

+ (id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForMIMEType:(NSString *)mimeType
{
	for(SFBAudioFileInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedMIMETypes = [(id <SFBAudioFileInputOutputHandling>)handlerInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return [[handlerInfo.klass alloc] init];
	}

	return nil;
}

@end
