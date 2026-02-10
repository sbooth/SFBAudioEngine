//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "AudioPlayer.h"
#import "SFBAudioPlayer.h"

@interface SFBAudioPlayer () {
  @package
    /// The underlying AudioPlayer instance
    sfb::AudioPlayer::unique_ptr _player;
}
@end
