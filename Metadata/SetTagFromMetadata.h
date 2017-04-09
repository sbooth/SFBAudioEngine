/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/tag.h>

/*! @file SetTagFromMetadata.h @brief Utility method for setting \c TagLib::Tag values from \c Metadata */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class Metadata;

		/*!
		 * @brief Set the values in a \c TagLib::Tag from \c Metadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::Tag to receive the metadata
		 * @return \c true on success, \c false otherwise
		 */
		bool SetTagFromMetadata(const Metadata& metadata, TagLib::Tag *tag);

	}
}
