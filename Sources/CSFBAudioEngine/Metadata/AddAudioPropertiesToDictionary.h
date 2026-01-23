//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <taglib/audioproperties.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace sfb {

/// Adds `properties` to `dictionary`
void addAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
