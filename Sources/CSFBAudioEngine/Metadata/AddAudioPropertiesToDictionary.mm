//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "AddAudioPropertiesToDictionary.h"

#import "SFBAudioProperties.h"

void sfb::addAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary) {
    assert(properties != nil);
    assert(dictionary != nil);

    if (const auto lengthInMilliseconds = properties->lengthInMilliseconds(); lengthInMilliseconds != 0) {
        dictionary[SFBAudioPropertiesKeyDuration] = @(lengthInMilliseconds / 1000.0);
    }

    if (const auto channels = properties->channels(); channels != 0) {
        dictionary[SFBAudioPropertiesKeyChannelCount] = @(channels);
    }

    if (const auto sampleRate = properties->sampleRate(); sampleRate != 0) {
        dictionary[SFBAudioPropertiesKeySampleRate] = @(sampleRate);
    }

    if (const auto bitrate = properties->bitrate(); bitrate != 0) {
        dictionary[SFBAudioPropertiesKeyBitrate] = @(bitrate);
    }
}
