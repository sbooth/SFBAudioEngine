//
// Copyright (c) 2010 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <taglib/xiphcomment.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibXiphComment)
/// Adds metadata and album art from @c tag to @c self
- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag;
/// Adds album art from @c pictureList to @c self
- (void)addAlbumArtFromTagLibFLACPictureList:(TagLib::List<TagLib::FLAC::Picture *>)pictureList;
@end

namespace SFB {
	namespace Audio {
		void SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
