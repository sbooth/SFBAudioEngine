//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <taglib/xiphcomment.h>

#pragma clang diagnostic pop

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibXiphComment)
- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);
	}
}

NS_ASSUME_NONNULL_END
