/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/xiphcomment.h>

/*! @file SetXiphCommentFromMetadata.h @brief Utility method for setting \c TagLib::Ogg::XiphComment values from \c Metadata */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::Ogg::XiphComment from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::Ogg::XiphComment to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 * @return \c true on success, \c false otherwise
		 */
		bool SetXiphCommentFromMetadata(const Metadata& metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);

	}
}
