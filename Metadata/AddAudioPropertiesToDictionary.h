/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/audioproperties.h>

/*! @file AddAudioPropertiesToDictionary.h @brief Utility method for adding \c TagLib::AudioProperties contents to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	namespace Audio {

		/*!
		 * @brief Add the properties specified in the \c TagLib::AudioProperties instance to \c dictionary
		 * @param dictionary The dictionary
		 * @param properties The audio properties
		 * @return \c true on success, \c false otherwise
		 */
		bool AddAudioPropertiesToDictionary(CFMutableDictionaryRef dictionary, const TagLib::AudioProperties *properties);

	}
}
