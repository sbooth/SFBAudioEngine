//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/id3v2tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v2Tag)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag;
@end

namespace SFB {
namespace Audio {

/// Sets values in `tag` using `metadata`
void SetID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
