//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/mp4tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibMP4Tag)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibMP4Tag:(const TagLib::MP4::Tag *)tag;
@end

namespace sfb {

/// Sets values in `tag` using `metadata`
void setMP4TagFromMetadata(SFBAudioMetadata *metadata, TagLib::MP4::Tag *tag, bool setAlbumArt = true);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
