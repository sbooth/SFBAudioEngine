/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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
