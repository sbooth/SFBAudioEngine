/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AddAudioPropertiesToDictionary.h"
#include "AudioMetadata.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddAudioPropertiesToDictionary(CFMutableDictionaryRef dictionary, const TagLib::AudioProperties *properties)
{
	if(nullptr == dictionary || nullptr == properties)
		return false;

	if(properties->length())
		AddIntToDictionary(dictionary, Metadata::kDurationKey, properties->length());

	if(properties->channels())
		AddIntToDictionary(dictionary, Metadata::kChannelsPerFrameKey, properties->channels());

	if(properties->sampleRate())
		AddIntToDictionary(dictionary, Metadata::kSampleRateKey, properties->sampleRate());

	if(properties->bitrate())
		AddIntToDictionary(dictionary, Metadata::kBitrateKey, properties->bitrate());

	return true;
}
