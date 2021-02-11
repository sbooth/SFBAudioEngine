//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioProperties.h"

void SFB::Audio::AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary)
{
	NSCParameterAssert(properties != nil);
	NSCParameterAssert(dictionary != nil);

	if(properties->length())
		dictionary[SFBAudioPropertiesKeyDuration] = @(properties->length());

	if(properties->channels())
		dictionary[SFBAudioPropertiesKeyChannelCount] = @(properties->channels());

	if(properties->sampleRate())
		dictionary[SFBAudioPropertiesKeySampleRate] = @(properties->sampleRate());

	if(properties->bitrate())
		dictionary[SFBAudioPropertiesKeyBitrate] = @(properties->bitrate());
}
