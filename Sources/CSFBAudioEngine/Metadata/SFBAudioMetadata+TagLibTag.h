//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibTag)
/// Adds metadata from `tag` to `self`
- (void)addMetadataFromTagLibTag:(const TagLib::Tag *)tag;
@end

namespace SFB {
namespace Audio {

/// Sets values in `tag` using `metadata`
void SetTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
