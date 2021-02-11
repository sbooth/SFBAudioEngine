//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/mp4tag.h>

#pragma clang diagnostic pop

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibMP4Tag)
- (void)addMetadataFromTagLibMP4Tag:(const TagLib::MP4::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetMP4TagFromMetadata(SFBAudioMetadata *metadata, TagLib::MP4::Tag *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
