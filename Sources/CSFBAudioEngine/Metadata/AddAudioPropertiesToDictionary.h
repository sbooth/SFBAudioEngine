//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import <Foundation/Foundation.h>

#import <taglib/audioproperties.h>

NS_ASSUME_NONNULL_BEGIN

namespace sfb {

/// Adds `properties` to `dictionary`
void addAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
