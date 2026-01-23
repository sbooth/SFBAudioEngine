//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioMetadata.h"

#import <taglib/id3v1tag.h>

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v1Tag)
/// Adds metadata from `tag` to `self`
- (void)addMetadataFromTagLibID3v1Tag:(const TagLib::ID3v1::Tag *)tag;
@end

namespace sfb {

/// Sets values in `tag` using `metadata`
void setID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
