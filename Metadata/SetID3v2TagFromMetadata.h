/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/id3v2tag.h>

#import "SFBAudioMetadata.h"

/*! @file SetID3v2TagFromMetadata.h @brief Utility function for setting \c TagLib::ID3v2::Tag values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::ID3v2::Tag from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::ID3v2::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 */
		void SetID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);

	}
}

NS_ASSUME_NONNULL_END
