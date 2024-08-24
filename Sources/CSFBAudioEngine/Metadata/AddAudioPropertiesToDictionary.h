//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/audioproperties.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace SFB {
	namespace Audio {
		void AddAudioPropertiesToDictionary(const TagLib::AudioProperties *properties, NSMutableDictionary *dictionary);
	}
}

NS_ASSUME_NONNULL_END
