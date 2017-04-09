/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/id3v2tag.h>

/*! @file SetID3v2TagFromMetadata.h @brief Utility method for setting \c TagLib::ID3v2::Tag values from \c Metadata */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::ID3v2::Tag from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::ID3v2::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 * @return \c true on success, \c false otherwise
		 */
		bool SetID3v2TagFromMetadata(const Metadata& metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt = true);

	}
}
