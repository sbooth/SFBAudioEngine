//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/apetag.h>

#pragma clang diagnostic pop

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
