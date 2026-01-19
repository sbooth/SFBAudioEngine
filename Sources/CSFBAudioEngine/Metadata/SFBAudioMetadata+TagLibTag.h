//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioMetadata.h"

#import <taglib/tag.h>

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibTag)
/// Adds metadata from `tag` to `self`
- (void)addMetadataFromTagLibTag:(const TagLib::Tag *)tag;
@end

namespace sfb {

/// Sets values in `tag` using `metadata`
void setTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
