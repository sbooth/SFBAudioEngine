//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <AVFAudio/AVFAudio.h>

namespace SFB {

/// Returns `true` if `lhs` and `rhs` are equivalent
///
/// Channel layouts are considered equivalent if:
/// 1) Both channel layouts are `nil`
/// 2) One channel layout is `nil` and the other has a mono or stereo layout tag
/// 3) `kAudioFormatProperty_AreChannelLayoutsEquivalent` is true
bool AVAudioChannelLayoutsAreEquivalent(AVAudioChannelLayout * _Nullable lhs, AVAudioChannelLayout * _Nullable rhs) noexcept;

} /* namespace SFB */
