//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioMetadata.h"

#import <taglib/id3v2tag.h>

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v2Tag)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag;
@end

namespace sfb {

/// Sets values in `tag` using `metadata`
void setID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
