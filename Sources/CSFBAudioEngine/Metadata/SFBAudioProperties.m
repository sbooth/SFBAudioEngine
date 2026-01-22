//
// Copyright (c) 2006-2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioProperties.h"

// Key names for the properties dictionary
SFBAudioPropertiesKey const SFBAudioPropertiesKeyFormatName = @"Format Name";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyFrameLength = @"Frame Length";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyChannelCount = @"Channel Count";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitDepth = @"Bit Depth";
SFBAudioPropertiesKey const SFBAudioPropertiesKeySampleRate = @"Sample Rate";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyDuration = @"Duration";
SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitrate = @"Bitrate";

@interface SFBAudioProperties () {
  @private
    NSDictionary *_properties;
}
@end

@implementation SFBAudioProperties

- (instancetype)init {
    if ((self = [super init]))
        _properties = [NSDictionary dictionary];
    return self;
}

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation {
    if ((self = [super init])) {
        NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
        dictionary[SFBAudioPropertiesKeyFormatName] = dictionaryRepresentation[SFBAudioPropertiesKeyFormatName];
        dictionary[SFBAudioPropertiesKeyFrameLength] = dictionaryRepresentation[SFBAudioPropertiesKeyFrameLength];
        dictionary[SFBAudioPropertiesKeyChannelCount] = dictionaryRepresentation[SFBAudioPropertiesKeyChannelCount];
        dictionary[SFBAudioPropertiesKeyBitDepth] = dictionaryRepresentation[SFBAudioPropertiesKeyBitDepth];
        dictionary[SFBAudioPropertiesKeySampleRate] = dictionaryRepresentation[SFBAudioPropertiesKeySampleRate];
        dictionary[SFBAudioPropertiesKeyDuration] = dictionaryRepresentation[SFBAudioPropertiesKeyDuration];
        dictionary[SFBAudioPropertiesKeyBitrate] = dictionaryRepresentation[SFBAudioPropertiesKeyBitrate];
        _properties = [dictionary copy];
    }
    return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone {
#pragma unused(zone)
    return self;
}

- (NSString *)formatName {
    return [_properties objectForKey:SFBAudioPropertiesKeyFormatName];
}

- (NSNumber *)frameLength {
    return [_properties objectForKey:SFBAudioPropertiesKeyFrameLength];
}

- (NSNumber *)channelCount {
    return [_properties objectForKey:SFBAudioPropertiesKeyChannelCount];
}

- (NSNumber *)bitDepth {
    return [_properties objectForKey:SFBAudioPropertiesKeyBitDepth];
}

- (NSNumber *)sampleRate {
    return [_properties objectForKey:SFBAudioPropertiesKeySampleRate];
}

- (NSNumber *)duration {
    return [_properties objectForKey:SFBAudioPropertiesKeyDuration];
}

- (NSNumber *)bitrate {
    return [_properties objectForKey:SFBAudioPropertiesKeyBitrate];
}

#pragma mark - External Representation

- (NSDictionary *)dictionaryRepresentation {
    return _properties;
}

#pragma mark - Dictionary-Like Interface

- (id)objectForKey:(SFBAudioPropertiesKey)key {
    return [_properties objectForKey:key];
}

- (id)valueForKey:(SFBAudioPropertiesKey)key {
    return [_properties valueForKey:key];
}

- (id)objectForKeyedSubscript:(SFBAudioPropertiesKey)key {
    return _properties[key];
}

@end
