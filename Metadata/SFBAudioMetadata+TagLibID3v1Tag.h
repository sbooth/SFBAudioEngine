/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

// Ignore warnings about TagLib::ID3v1::StringHandler virtual functions but non-virtual dtor

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"

#import <taglib/id3v1tag.h>

#pragma clang diagnostic pop

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibID3v1Tag)
- (void)addMetadataFromTagLibID3v1Tag:(const TagLib::ID3v1::Tag *)tag;
@end

namespace SFB {
	namespace Audio {
		void SetID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag);
	}
}

NS_ASSUME_NONNULL_END
