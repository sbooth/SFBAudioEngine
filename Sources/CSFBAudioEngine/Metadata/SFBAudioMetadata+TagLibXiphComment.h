//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioMetadata.h"

#import <taglib/xiphcomment.h>

#import <memory>

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibXiphComment)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag;
@end

namespace sfb {

/// Sets values in `tag` using `metadata`
void setXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);

/// Converts an `SFBAttachedPicture` object to a `TagLib::FLAC::Picture` object
std::unique_ptr<TagLib::FLAC::Picture> ConvertAttachedPictureToFLACPicture(SFBAttachedPicture *attachedPicture);

} /* namespace sfb */

NS_ASSUME_NONNULL_END
