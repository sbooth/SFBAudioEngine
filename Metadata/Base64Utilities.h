/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <taglib/tbytevector.h>

/*! @file Base64Utilities.h @brief Base 64 conversion methods */

/*! @brief \c Taglib's encompassing namespace */
namespace TagLib {

	/*! @brief Encode the bytes in \c input to base 64 */
	ByteVector EncodeBase64(const ByteVector& input);

	/*! @brief Decode the bytes in \c input from base 64 */
	ByteVector DecodeBase64(const ByteVector& input);

}
