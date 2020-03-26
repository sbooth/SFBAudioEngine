/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioProperties.h"

// Key names for the properties dictionary
NSString * const SFBAudioPropertiesFormatNameKey				= @"Format Name";
NSString * const SFBAudioPropertiesTotalFramesKey				= @"Total Frames";
NSString * const SFBAudioPropertiesChannelsPerFrameKey			= @"Channels Per Frame";
NSString * const SFBAudioPropertiesBitsPerChannelKey			= @"Bits Per Channel";
NSString * const SFBAudioPropertiesSampleRateKey				= @"Sample Rate";
NSString * const SFBAudioPropertiesDurationKey					= @"Duration";
NSString * const SFBAudioPropertiesBitrateKey					= @"Bitrate";

@interface SFBAudioProperties ()
{
@private
	NSDictionary *_properties;
}
@end

@implementation SFBAudioProperties

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation
{
	if((self = [super init])) {
		NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
		dictionary[SFBAudioPropertiesFormatNameKey] 		= dictionaryRepresentation[SFBAudioPropertiesFormatNameKey];
		dictionary[SFBAudioPropertiesTotalFramesKey] 		= dictionaryRepresentation[SFBAudioPropertiesTotalFramesKey];
		dictionary[SFBAudioPropertiesChannelsPerFrameKey] 	= dictionaryRepresentation[SFBAudioPropertiesChannelsPerFrameKey];
		dictionary[SFBAudioPropertiesBitsPerChannelKey] 	= dictionaryRepresentation[SFBAudioPropertiesBitsPerChannelKey];
		dictionary[SFBAudioPropertiesSampleRateKey] 		= dictionaryRepresentation[SFBAudioPropertiesSampleRateKey];
		dictionary[SFBAudioPropertiesDurationKey] 			= dictionaryRepresentation[SFBAudioPropertiesDurationKey];
		dictionary[SFBAudioPropertiesBitrateKey] 			= dictionaryRepresentation[SFBAudioPropertiesBitrateKey];
		_properties = [dictionary copy];
	}
	return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	SFBAudioProperties *result = [[[self class] alloc] init];
	result->_properties = _properties;
	return result;
}

- (NSString *)formatName
{
	return [_properties objectForKey:SFBAudioPropertiesFormatNameKey];
}

- (NSNumber *)totalFrames
{
	return [_properties objectForKey:SFBAudioPropertiesTotalFramesKey];
}

- (NSNumber *)channelsPerFrame
{
	return [_properties objectForKey:SFBAudioPropertiesChannelsPerFrameKey];
}

- (NSNumber *)bitsPerChannel
{
	return [_properties objectForKey:SFBAudioPropertiesBitsPerChannelKey];
}

- (NSNumber *)sampleRate
{
	return [_properties objectForKey:SFBAudioPropertiesSampleRateKey];
}

- (NSNumber *)duration
{
	return [_properties objectForKey:SFBAudioPropertiesDurationKey];
}

- (NSNumber *)bitrate
{
	return [_properties objectForKey:SFBAudioPropertiesBitrateKey];
}

- (NSDictionary *)dictionaryRepresentation
{
	return _properties;
}

@end
