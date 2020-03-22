/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/tag.h>

#import "SFBAudioMetadata.h"

/*! @file SetTagFromMetadata.h @brief Utility function for setting \c TagLib::Tag values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::Tag from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::Tag to receive the metadata
		 */
		void SetTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag);

	}
}

NS_ASSUME_NONNULL_END
