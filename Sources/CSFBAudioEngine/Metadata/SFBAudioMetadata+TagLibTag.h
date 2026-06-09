//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
