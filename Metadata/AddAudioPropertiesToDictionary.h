/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <taglib/audioproperties.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace SFB {
	namespace Audio {
		void AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);
	}
}

NS_ASSUME_NONNULL_END
