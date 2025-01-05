//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/apetag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibAPETag)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibAPETag:(const TagLib::APE::Tag *)tag;
@end

namespace SFB {
namespace Audio {

/// Sets values in `tag` using `metadata`
void SetAPETagFromMetadata(SFBAudioMetadata *metadata, TagLib::APE::Tag *tag, bool setAlbumArt = true);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
