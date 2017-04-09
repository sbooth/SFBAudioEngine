/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/tag.h>

/*! @file AddTagToDictionary.h @brief Utility method for adding \c TagLib::Tag contents to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Add the metadata specified in the \c TagLib::Tag instance to \c dictionary
		 * @param dictionary A \c CFMutableDictionaryRef to receive the metadata
		 * @param properties The tag
		 * @return \c true on success, \c false otherwise
		 */
		bool AddTagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::Tag *tag);

	}
}
