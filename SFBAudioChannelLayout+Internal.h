/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioChannelLayout.h"

@interface SFBAudioChannelLayout ()
{
@package
	AudioChannelLayout *_layout;
}
@end
