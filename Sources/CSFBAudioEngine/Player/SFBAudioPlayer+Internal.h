//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioPlayer.h"

#import "AudioPlayer.h"

@interface SFBAudioPlayer ()
{
@package
	SFB::AudioPlayer::unique_ptr _player;
}
@end
