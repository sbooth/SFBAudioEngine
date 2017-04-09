/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/apetag.h>

/*! @file SetAPETagFromMetadata.h @brief Utility method for setting \c TagLib::APE::Tag values from \c Metadata */


/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::APE::Tag from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::APE::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 * @return \c true on success, \c false otherwise
		 */
		bool SetAPETagFromMetadata(const Metadata& metadata, TagLib::APE::Tag *tag, bool setAlbumArt = true);

	}
}
