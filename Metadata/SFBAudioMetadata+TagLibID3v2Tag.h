/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <taglib/id3v2tag.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v2Tag)
- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
