/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioProperties.h"

void SFB::Audio::AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary)
{
	NSCParameterAssert(properties != nil);
	NSCParameterAssert(dictionary != nil);

	if(properties->length())
		dictionary[SFBAudioPropertiesDurationKey] = @(properties->length());

	if(properties->channels())
		dictionary[SFBAudioPropertiesChannelsPerFrameKey] = @(properties->channels());

	if(properties->sampleRate())
		dictionary[SFBAudioPropertiesSampleRateKey] = @(properties->sampleRate());

	if(properties->bitrate())
		dictionary[SFBAudioPropertiesBitrateKey] = @(properties->bitrate());
}
