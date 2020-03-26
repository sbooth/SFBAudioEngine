/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioFile+Internal.h"

@implementation SFBAudioFileInputOutputHandlerInfo
@end

@implementation SFBAudioFile (SFBAudioFileInputOutputHandling)

static NSMutableArray *_registeredInputOutputHandlers = nil;

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

+ (NSArray<id<SFBAudioFileInputOutputHandling>> *)registeredInputOutputHandlers
{
	return _registeredInputOutputHandlers;
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
