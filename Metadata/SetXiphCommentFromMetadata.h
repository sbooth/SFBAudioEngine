/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/xiphcomment.h>

#import "SFBAudioMetadata.h"

/*! @file SetXiphCommentFromMetadata.h @brief Utility method for setting \c TagLib::Ogg::XiphComment values from \c SFBAudioMetadata */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Set the values in a \c TagLib::Ogg::XiphComment from \c SFBAudioMetadata
		 * @param metadata The metadata
		 * @param tag A \c TagLib::Ogg::XiphComment to receive the metadata
		 * @param setAlbumArt Whether to set album art
		 */
		void SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt = true);

	}
}

NS_ASSUME_NONNULL_END
