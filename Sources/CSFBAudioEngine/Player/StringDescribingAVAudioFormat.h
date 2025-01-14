//
// Copyright (c) 2024-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <AVFAudio/AVFAudio.h>

namespace SFB {

/// Returns a string describing `format`
NSString * _Nullable StringDescribingAVAudioFormat(AVAudioFormat * _Nullable format, bool includeChannelLayout = true) noexcept;

} /* namespace SFB */
