//
// Copyright (c) 2010 - 2024 Stephen F. Booth <me@sbooth.org>
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
		void SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);
		std::unique_ptr<TagLib::FLAC::Picture> ConvertAttachedPictureToFLACPicture(SFBAttachedPicture *attachedPicture);
	}
}

NS_ASSUME_NONNULL_END
