/*
 * Copyright (c) 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/mp4tag.h>

/*! @file SetMP4TagFromMetadata.h @brief Utility method for setting \c TagLib::MP4::Tag values from \c Metadata */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::MP4::Tag from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::MP4::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 * @return \c true on success, \c false otherwise
		 */
		bool SetMP4TagFromMetadata(const Metadata& metadata, TagLib::MP4::Tag *tag, bool setAlbumArt = true);

	}
}
