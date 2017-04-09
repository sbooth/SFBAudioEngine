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

/*! @file AddID3v1TagToDictionary.h @brief Utility method for adding \c TagLib::ID3v1::Tag contents to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Add the metadata specified in the \c TagLib::ID3v1::Tag instance to \c dictionary
		 * @param dictionary A \c CFMutableDictionaryRef to receive the metadata
		 * @param attachedPictures A \c std::vector to receive the attached pictures
		 * @param properties The tag
		 * @return \c true on success, \c false otherwise
		 */
		bool AddID3v1TagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::ID3v1::Tag *tag);

	}
}
