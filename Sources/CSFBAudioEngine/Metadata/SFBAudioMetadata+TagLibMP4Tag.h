//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/mp4tag.h>

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
