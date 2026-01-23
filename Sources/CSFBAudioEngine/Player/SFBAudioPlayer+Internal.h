//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "AudioPlayer.h"
#import "SFBAudioPlayer.h"

#import <memory>

@interface SFBAudioPlayer () {
  @package
    /// The underlying AudioPlayer instance
    sfb::AudioPlayer::unique_ptr _player;
}
@end
