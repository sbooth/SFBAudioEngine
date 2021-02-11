//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/tag.h>

#pragma clang diagnostic pop

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
