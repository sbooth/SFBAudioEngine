/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioProperties.h"

void SFB::Audio::AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary)
{
	NSCParameterAssert(properties != nil);
	NSCParameterAssert(dictionary != nil);

	if(properties->length())
		dictionary[SFBAudioPropertiesKeyDuration] = @(properties->length());

	if(properties->channels())
		dictionary[SFBAudioPropertiesKeyChannelsPerFrame] = @(properties->channels());

	if(properties->sampleRate())
		dictionary[SFBAudioPropertiesKeySampleRate] = @(properties->sampleRate());

	if(properties->bitrate())
		dictionary[SFBAudioPropertiesKeyBitrate] = @(properties->bitrate());
}
