/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <Security/Security.h>

#include "Base64Utilities.h"
#include "CFWrapper.h"

TagLib::ByteVector TagLib::DecodeBase64(const TagLib::ByteVector& input)
{
	SFB::CFError error;
	SFB::SecTransform decoder(SecDecodeTransformCreate(kSecBase64Encoding, &error));
    if(!decoder) {
		os_log_debug(OS_LOG_DEFAULT, "SecDecodeTransformCreate failed: %{public}@", error.Object());
		return {};
	}

	SFB::CFData sourceData(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull));
	if(!sourceData)
		return {};

    if(!SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, sourceData, &error)) {
		os_log_debug(OS_LOG_DEFAULT, "SecTransformSetAttribute failed: %{public}@", error.Object());
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
		os_log_debug(OS_LOG_DEFAULT, "SecEncodeTransformCreate failed: %{public}@", error.Object());
		return {};
	}

	SFB::CFData sourceData(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull));
	if(!sourceData)
		return {};

    if(!SecTransformSetAttribute(encoder, kSecTransformInputAttributeName, sourceData, &error)) {
		os_log_debug(OS_LOG_DEFAULT, "SecTransformSetAttribute failed: %{public}@", error.Object());
		return {};
	}

	SFB::CFData encodedData((CFDataRef)SecTransformExecute(encoder, &error));
	if(!encodedData) {
		os_log_debug(OS_LOG_DEFAULT, "SecTransformExecute failed: %{public}@", error.Object());
		return {};
	}

	return {(const char *)CFDataGetBytePtr((CFDataRef)encodedData), (size_t)CFDataGetLength((CFDataRef)encodedData)};
}
