/*
 * Copyright (c) 2018 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/mp4tag.h>

#import "SFBAudioMetadata.h"

/*! @file SetMP4TagFromMetadata.h @brief Utility function for setting \c TagLib::MP4::Tag values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::MP4::Tag from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::MP4::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 */
		void SetMP4TagFromMetadata(SFBAudioMetadata *metadata, TagLib::MP4::Tag *tag, bool setAlbumArt = true);

	}
}

NS_ASSUME_NONNULL_END
