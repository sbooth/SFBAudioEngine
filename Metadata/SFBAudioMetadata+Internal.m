/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (SFBAudioMetadataInternal)

- (void)setFormatName:(NSString *)formatName
{
	[_metadata setObject:[formatName copy] forKey:SFBAudioMetadataFormatNameKey];
}

- (void)setTotalFrames:(NSNumber *)totalFrames
{
	[_metadata setObject:totalFrames forKey:SFBAudioMetadataTotalFramesKey];
}

- (void)setChannelsPerFrame:(NSNumber *)channelsPerFrame
{
	[_metadata setObject:channelsPerFrame forKey:SFBAudioMetadataChannelsPerFrameKey];
}

- (void)setBitsPerChannel:(NSNumber *)bitsPerChannel
{
	[_metadata setObject:bitsPerChannel forKey:SFBAudioMetadataBitsPerChannelKey];
}

- (void)setSampleRate:(NSNumber *)sampleRate
{
	[_metadata setObject:sampleRate forKey:SFBAudioMetadataSampleRateKey];
}

- (void)setDuration:(NSNumber *)duration
{
	[_metadata setObject:duration forKey:SFBAudioMetadataDurationKey];
}

- (void)setBitrate:(NSNumber *)bitrate
{
	[_metadata setObject:bitrate forKey:SFBAudioMetadataBitrateKey];
}

@end


@implementation SFBAudioMetadataInputOutputHandlerInfo
@end

@implementation SFBAudioMetadata (SFBAudioMetadataInputOutputHandling)

static NSMutableArray *_registeredInputOutputHandlers = nil;

+ (void)registerInputOutputHandler:(Class)reader
{
	[self registerInputOutputHandler:reader priority:0];
}

+ (void)registerInputOutputHandler:(Class)reader priority:(int)priority
{
	NSAssert([reader conformsToProtocol:NSProtocolFromString(@"SFBAudioMetadataInputOutputHandling")], @"Unable to register class '%@' as a InputOutput handler because it does not conform to protocol SFBAudioMetadataInputOutputHandling", NSStringFromClass(reader));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredInputOutputHandlers = [NSMutableArray array];
	});

	SFBAudioMetadataInputOutputHandlerInfo *handlerInfo = [[SFBAudioMetadataInputOutputHandlerInfo alloc] init];
	handlerInfo.klass = reader;
	handlerInfo.priority = priority;

	[_registeredInputOutputHandlers addObject:handlerInfo];
	[_registeredInputOutputHandlers sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
		return ((SFBAudioMetadataInputOutputHandlerInfo *)obj1).priority < ((SFBAudioMetadataInputOutputHandlerInfo *)obj2).priority;
	}];
}

+ (NSArray<id<SFBAudioMetadataInputOutputHandling>> *)registeredInputOutputHandlers
{
	return _registeredInputOutputHandlers;
}

+ (id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForURL:(NSURL *)url
{
	// Use extension-based type resolvers for files
	NSString *scheme = url.scheme;

	if([scheme caseInsensitiveCompare:@"file"] != NSOrderedSame)
		return nil;

	// TODO: Handle MIME types?

	return [self inputOutputHandlerForPathExtension:url.pathExtension.lowercaseString];
}

+ (id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForPathExtension:(NSString *)extension
{
	for(SFBAudioMetadataInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedPathExtensions = [(id <SFBAudioMetadataInputOutputHandling>)handlerInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return [[handlerInfo.klass alloc] init];
	}

	return nil;
}

+ (id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForMIMEType:(NSString *)mimeType
{
	for(SFBAudioMetadataInputOutputHandlerInfo *handlerInfo in _registeredInputOutputHandlers) {
		NSSet *supportedMIMETypes = [(id <SFBAudioMetadataInputOutputHandling>)handlerInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return [[handlerInfo.klass alloc] init];
	}

	return nil;
}

@end
