/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (Internal)

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


@implementation SFBAudioMetadataSubclassInfo
@end


@implementation SFBAudioMetadata (SubclassRegistration)

static NSMutableArray *_registeredSubclasses = nil;

+ (void) registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void) registerSubclass:(Class)subclass priority:(int)priority
{
	SEL sel = NSSelectorFromString(@"_supportedFileExtensions");
	NSAssert([subclass respondsToSelector:sel], @"%@ is a malformed SFBAudioMetadata subclass: %@ is required but not implemented", NSStringFromClass(subclass), NSStringFromSelector(sel));

	sel = NSSelectorFromString(@"_supportedMIMETypes");
	NSAssert([subclass respondsToSelector:sel], @"%@ is a malformed SFBAudioMetadata subclass: %@ is required but not implemented", NSStringFromClass(subclass), NSStringFromSelector(sel));

	if(_registeredSubclasses == nil) {
		_registeredSubclasses = [NSMutableArray array];
	}

	SFBAudioMetadataSubclassInfo *subclassInfo = [[SFBAudioMetadataSubclassInfo alloc] init];
	subclassInfo.subclass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
		return ((SFBAudioMetadataSubclassInfo *)obj1).priority < ((SFBAudioMetadataSubclassInfo *)obj2).priority;
	}];
}

+ (NSArray *)registeredSubclasses
{
	return _registeredSubclasses;
}

@end


@implementation SFBAudioMetadata (RequiredSubclassMethods)

- (BOOL)_readMetadata:(NSError **)error
{
#pragma unused(error)
	NSAssert(0, @"SFBAudioMetadata subclasses are required to implement _readMetadata:");
	return NO;
}

- (BOOL)_writeMetadata:(NSError **)error
{
#pragma unused(error)
	NSAssert(0, @"SFBAudioMetadata subclasses are required to implement _writeMetadata:");
	return NO;
}

@end
