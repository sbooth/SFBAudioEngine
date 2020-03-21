/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

// Ignore warnings about TagLib::ID3v1::StringHandler virtual functions but non-virtual dtor

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"

#include <taglib/id3v1tag.h>

#pragma clang diagnostic pop

#import "SFBAudioMetadata.h"

/*! @file SetID3v1TagFromMetadata.h @brief Utility method for setting \c TagLib::ID3v1::Tag values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::ID3v1::Tag from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::ID3v1::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 */
		void SetID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag);

	}
}

NS_ASSUME_NONNULL_END
