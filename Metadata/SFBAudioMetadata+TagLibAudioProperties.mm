/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibAudioProperties)

- (void)addAudioPropertiesFromTagLibAudioProperties:(const TagLib::AudioProperties *)properties
{
	NSParameterAssert(properties != nil);

	if(properties->length())
		self.duration = @(properties->length());

	if(properties->channels())
		self.channelsPerFrame = @(properties->channels());

	if(properties->sampleRate())
		self.sampleRate = @(properties->sampleRate());

	if(properties->bitrate())
		self.bitrate = @(properties->bitrate());
}

@end
