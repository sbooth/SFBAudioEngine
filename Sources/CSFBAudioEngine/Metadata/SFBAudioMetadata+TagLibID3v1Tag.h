//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/id3v1tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v1Tag)
/// Adds metadata from `tag` to `self`
- (void)addMetadataFromTagLibID3v1Tag:(const TagLib::ID3v1::Tag *)tag;
@end

namespace SFB {
namespace Audio {

/// Sets values in `tag` using `metadata`
void SetID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
