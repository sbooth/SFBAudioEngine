//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import <taglib/xiphcomment.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibXiphComment)
/// Adds metadata and album art from `tag` to `self`
- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag;
@end

namespace SFB {
namespace Audio {

/// Sets values in `tag` using `metadata`
void SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);

/// Converts an `SFBAttachedPicture` object to a `TagLib::FLAC::Picture` object
std::unique_ptr<TagLib::FLAC::Picture> ConvertAttachedPictureToFLACPicture(SFBAttachedPicture *attachedPicture);

} /* namespace Audio */
} /* namespace SFB */

NS_ASSUME_NONNULL_END
