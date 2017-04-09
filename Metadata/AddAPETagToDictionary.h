/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <vector>
#include <taglib/apetag.h>

/*! @file AddAPETagToDictionary.h @brief Utility method for adding \c TagLib::APE::Tag contents to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		class AttachedPicture;

		/*!
		 * @brief Add the metadata specified in the \c TagLib::APE::Tag instance to \c dictionary
		 * @param dictionary A \c CFMutableDictionaryRef to receive the metadata
		 * @param attachedPictures A \c std::vector to receive the attached pictures
		 * @param properties The tag
		 * @return \c true on success, \c false otherwise
		 */
		bool AddAPETagToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::APE::Tag *tag);

	}
}
