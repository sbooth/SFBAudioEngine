//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <taglib/audioproperties.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace SFB {
namespace Audio {

/// Adds `properties` to `dictionary`
void AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
