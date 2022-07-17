//
// Copyright (c) 2010 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <taglib/tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibTag)
- (void)addMetadataFromTagLibTag:(const TagLib::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag);
	}
}

NS_ASSUME_NONNULL_END
