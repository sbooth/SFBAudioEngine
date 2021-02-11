//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/id3v2tag.h>

#pragma clang diagnostic pop

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v2Tag)
- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
