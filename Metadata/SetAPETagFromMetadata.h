/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/apetag.h>

#import "SFBAudioMetadata.h"

/*! @file SetAPETagFromMetadata.h @brief Utility method for setting \c TagLib::APE::Tag values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::APE::Tag from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::APE::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 */
		void SetAPETagFromMetadata(SFBAudioMetadata *metadata, TagLib::APE::Tag *tag, bool setAlbumArt = true);

	}
}

NS_ASSUME_NONNULL_END
