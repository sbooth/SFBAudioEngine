/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioProperties.h"

// Key names for the properties dictionary
SFBAudioPropertiesKey const SFBAudioPropertiesKeyFormatName			= @"Format Name";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyTotalFrames		= @"Total Frames";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyChannelsPerFrame	= @"Channels Per Frame";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitsPerChannel		= @"Bits Per Channel";
SFBAudioPropertiesKey const SFBAudioPropertiesKeySampleRate			= @"Sample Rate";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyDuration			= @"Duration";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitrate			= @"Bitrate";

@interface SFBAudioProperties ()
{
@private
	NSDictionary *_properties;
}
@end

@implementation SFBAudioProperties

- (instancetype)init
{
	if((self = [super init]))
		_properties = [NSDictionary dictionary];
	return self;
}

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation
{
	if((self = [super init])) {
		NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
		dictionary[SFBAudioPropertiesKeyFormatName] 		= dictionaryRepresentation[SFBAudioPropertiesKeyFormatName];
		dictionary[SFBAudioPropertiesKeyTotalFrames] 		= dictionaryRepresentation[SFBAudioPropertiesKeyTotalFrames];
		dictionary[SFBAudioPropertiesKeyChannelsPerFrame] 	= dictionaryRepresentation[SFBAudioPropertiesKeyChannelsPerFrame];
		dictionary[SFBAudioPropertiesKeyBitsPerChannel] 	= dictionaryRepresentation[SFBAudioPropertiesKeyBitsPerChannel];
		dictionary[SFBAudioPropertiesKeySampleRate] 		= dictionaryRepresentation[SFBAudioPropertiesKeySampleRate];
		dictionary[SFBAudioPropertiesKeyDuration] 			= dictionaryRepresentation[SFBAudioPropertiesKeyDuration];
		dictionary[SFBAudioPropertiesKeyBitrate] 			= dictionaryRepresentation[SFBAudioPropertiesKeyBitrate];
		_properties = [dictionary copy];
	}
	return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	return self;
}

- (NSString *)formatName
{
	return [_properties objectForKey:SFBAudioPropertiesKeyFormatName];
}

- (NSNumber *)totalFrames
{
	return [_properties objectForKey:SFBAudioPropertiesKeyTotalFrames];
}

- (NSNumber *)channelsPerFrame
{
	return [_properties objectForKey:SFBAudioPropertiesKeyChannelsPerFrame];
}

- (NSNumber *)bitsPerChannel
{
	return [_properties objectForKey:SFBAudioPropertiesKeyBitsPerChannel];
}

- (NSNumber *)sampleRate
{
	return [_properties objectForKey:SFBAudioPropertiesKeySampleRate];
}

- (NSNumber *)duration
{
	return [_properties objectForKey:SFBAudioPropertiesKeyDuration];
}

- (NSNumber *)bitrate
{
	return [_properties objectForKey:SFBAudioPropertiesKeyBitrate];
}

- (NSDictionary *)dictionaryRepresentation
{
	return _properties;
}

@end
