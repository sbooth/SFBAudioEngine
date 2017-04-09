/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <Security/Security.h>

#include "Base64Utilities.h"
#include "CFWrapper.h"
#include "Logger.h"

TagLib::ByteVector TagLib::DecodeBase64(const TagLib::ByteVector& input)
{
	SFB::CFError error;
	SFB::SecTransform decoder(SecDecodeTransformCreate(kSecBase64Encoding, &error));
    if(!decoder) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecDecodeTransformCreate failed: " << error);
		return {};
	}

	SFB::CFData sourceData(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull));
	if(!sourceData)
		return {};

    if(!SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, sourceData, &error)) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformSetAttribute failed: " << error);
		return {};
	}

	SFB::CFData decodedData((CFDataRef)SecTransformExecute(decoder, &error));
	if(!decodedData)
		return {};

	return {(const char *)CFDataGetBytePtr((CFDataRef)decodedData), (size_t)CFDataGetLength((CFDataRef)decodedData)};
}

TagLib::ByteVector TagLib::EncodeBase64(const TagLib::ByteVector& input)
{
	SFB::CFError error;
	SFB::SecTransform encoder(SecEncodeTransformCreate(kSecBase64Encoding, &error));
    if(!encoder) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecEncodeTransformCreate failed: " << error);
		return {};
	}

	SFB::CFData sourceData(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull));
	if(!sourceData)
		return {};

    if(!SecTransformSetAttribute(encoder, kSecTransformInputAttributeName, sourceData, &error)) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformSetAttribute failed: " << error);
		return {};
	}

	SFB::CFData encodedData((CFDataRef)SecTransformExecute(encoder, &error));
	if(!encodedData) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformExecute failed: " << error);
		return {};
	}

	return {(const char *)CFDataGetBytePtr((CFDataRef)encodedData), (size_t)CFDataGetLength((CFDataRef)encodedData)};
}
