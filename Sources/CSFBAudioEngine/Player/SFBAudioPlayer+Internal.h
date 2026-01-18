//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import "SFBAudioPlayer.h"
#import "AudioPlayer.h"

@interface SFBAudioPlayer ()
{
@package
	/// The underlying AudioPlayer instance
	sfb::AudioPlayer::unique_ptr _player;
}
@end
