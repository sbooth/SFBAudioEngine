//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "AddAudioPropertiesToDictionary.h"

#import "SFBAudioProperties.h"

void sfb::addAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary) {
    assert(properties != nil);
    assert(dictionary != nil);

    if (properties->lengthInMilliseconds() != 0) {
        dictionary[SFBAudioPropertiesKeyDuration] = @(properties->lengthInMilliseconds() / 1000.0);
    }

    if (properties->channels() != 0) {
        dictionary[SFBAudioPropertiesKeyChannelCount] = @(properties->channels());
    }

    if (properties->sampleRate() != 0) {
        dictionary[SFBAudioPropertiesKeySampleRate] = @(properties->sampleRate());
    }

    if (properties->bitrate() != 0) {
        dictionary[SFBAudioPropertiesKeyBitrate] = @(properties->bitrate());
    }
}
