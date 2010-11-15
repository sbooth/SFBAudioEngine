/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "AddAudioPropertiesToDictionary.h"
#include "AudioMetadata.h"

bool
AddAudioPropertiesToDictionary(CFMutableDictionaryRef dictionary, const TagLib::AudioProperties *properties)
{
	assert(NULL != dictionary);
	assert(NULL != properties);
	
	if(0 != properties->length()) {
		int value = properties->length();
		CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
		CFDictionarySetValue(dictionary, kPropertiesDurationKey, duration);
		CFRelease(duration), duration = NULL;
	}

	if(0 != properties->channels()) {
		int value = properties->channels();
		CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
		CFDictionarySetValue(dictionary, kPropertiesChannelsPerFrameKey, channelsPerFrame);
		CFRelease(channelsPerFrame), channelsPerFrame = NULL;
	}

	if(0 != properties->sampleRate()) {
		int value = properties->sampleRate();
		CFNumberRef sampleRate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
		CFDictionarySetValue(dictionary, kPropertiesSampleRateKey, sampleRate);
		CFRelease(sampleRate), sampleRate = NULL;
	}

	if(0 != properties->bitrate()) {
		int value = properties->bitrate();
		CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
		CFDictionarySetValue(dictionary, kPropertiesBitrateKey, bitrate);
		CFRelease(bitrate), bitrate = NULL;
	}
	
	return true;
}
