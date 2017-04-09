/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

// Ignore warnings about TagLib::ID3v1::StringHandler virtual functions but non-virtual dtor

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"

#include <taglib/id3v1tag.h>

#pragma clang diagnostic pop

/*! @file SetID3v1TagFromMetadata.h @brief Utility method for setting \c TagLib::ID3v1::Tag values from \c Metadata */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::ID3v1::Tag from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::ID3v1::Tag to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 * @return \c true on success, \c false otherwise
		 */
		bool SetID3v1TagFromMetadata(const Metadata& metadata, TagLib::ID3v1::Tag *tag);

	}
}
