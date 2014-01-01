/*
 *  Copyright (C) 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Security/Security.h>

#include "Base64Utilities.h"
#include "CFWrapper.h"
#include "Logger.h"

TagLib::ByteVector TagLib::DecodeBase64(const TagLib::ByteVector& input)
{
	ByteVector result;

	CFErrorRef error;
	SFB::SecTransform decoder = SecDecodeTransformCreate(kSecBase64Encoding, &error);
    if(!decoder) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecDecodeTransformCreate failed: " << error);
		return TagLib::ByteVector::null;
	}

	SFB::CFData sourceData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull);
	if(!sourceData)
		return TagLib::ByteVector::null;

    if(!SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, sourceData, &error)) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformSetAttribute failed: " << error);
		return TagLib::ByteVector::null;
	}

	SFB::CFData decodedData = (CFDataRef)SecTransformExecute(decoder, &error);
	if(!decodedData)
		return TagLib::ByteVector::null;

	result.setData((const char *)CFDataGetBytePtr((CFDataRef)decodedData), (TagLib::uint)CFDataGetLength((CFDataRef)decodedData));

	return result;
}

TagLib::ByteVector TagLib::EncodeBase64(const TagLib::ByteVector& input)
{
	ByteVector result;

	CFErrorRef error;
	SFB::SecTransform encoder = SecEncodeTransformCreate(kSecBase64Encoding, &error);
    if(nullptr == encoder) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecEncodeTransformCreate failed: " << error);
		return TagLib::ByteVector::null;
	}

	SFB::CFData sourceData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull);
	if(!sourceData)
		return TagLib::ByteVector::null;

    if(!SecTransformSetAttribute(encoder, kSecTransformInputAttributeName, sourceData, &error)) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformSetAttribute failed: " << error);
		return TagLib::ByteVector::null;
	}

	SFB::CFData encodedData = (CFDataRef)SecTransformExecute(encoder, &error);
	if(!encodedData) {
		LOGGER_WARNING("org.sbooth.AudioEngine", "SecTransformExecute failed: " << error);
		return TagLib::ByteVector::null;
	}

	result.setData((const char *)CFDataGetBytePtr((CFDataRef)encodedData), (TagLib::uint)CFDataGetLength((CFDataRef)encodedData));

	return result;
}
