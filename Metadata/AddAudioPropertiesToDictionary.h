//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/audioproperties.h>

#pragma clang diagnostic pop

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace SFB {
	namespace Audio {
		void AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);
	}
}

NS_ASSUME_NONNULL_END
