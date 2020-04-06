/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioFormat.h"

@interface SFBAudioFormat ()
{
@package
	AudioStreamBasicDescription _streamDescription;
	SFBAudioChannelLayout *_channelLayout;
}
@end
