/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <taglib/apetag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibAPETag)
- (void)addMetadataFromTagLibAPETag:(const TagLib::APE::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetAPETagFromMetadata(SFBAudioMetadata *metadata, TagLib::APE::Tag *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
