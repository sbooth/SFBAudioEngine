/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioBufferList.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioBufferList ()
{
@package
	AudioBufferList *_bufferList;
}
@end

NS_ASSUME_NONNULL_END
