/*
 * Copyright (c) 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <vector>
#include <taglib/mp4tag.h>

/*! @file AddMP4TagToDictionary.h @brief Utility method for adding \c TagLib::MP4::Tag contents to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class AttachedPicture;

		/*!
		 * @brief Add the metadata specified in the \c TagLib::MP4::Tag instance to \c dictionary
		 * @param dictionary A \c CFMutableDictionaryRef to receive the metadata
		 * @param properties The tag
		 * @return \c true on success, \c false otherwise
		 */
		bool AddMP4TagToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::MP4::Tag *tag);

	}
}
