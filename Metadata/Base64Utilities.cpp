/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

TagLib::ByteVector TagLib::DecodeBase64(const TagLib::ByteVector& input)
{
	ByteVector result;

	CFErrorRef error;
	SecTransformRef decoder = SecDecodeTransformCreate(kSecBase64Encoding, &error);
    if(nullptr == decoder) {
		CFShow(error); 
		return TagLib::ByteVector::null;
	}

	CFDataRef sourceData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull);
	if(nullptr == sourceData) {
		CFRelease(decoder), decoder = nullptr;

		return TagLib::ByteVector::null;
	}

    if(!SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, sourceData, &error)) {
		CFShow(error); 

		CFRelease(sourceData), sourceData = nullptr;
		CFRelease(decoder), decoder = nullptr;

		return TagLib::ByteVector::null;
	}

	CFTypeRef decodedData = SecTransformExecute(decoder, &error);
	if(nullptr == decodedData) {
		CFShow(error); 

		CFRelease(sourceData), sourceData = nullptr;
		CFRelease(decoder), decoder = nullptr;

		return TagLib::ByteVector::null;
	}

	result.setData((const char *)CFDataGetBytePtr((CFDataRef)decodedData), (TagLib::uint)CFDataGetLength((CFDataRef)decodedData));

	CFRelease(decodedData), decodedData = nullptr;
	CFRelease(sourceData), sourceData = nullptr;
	CFRelease(decoder), decoder = nullptr;
	
	return result;
}

TagLib::ByteVector TagLib::EncodeBase64(const TagLib::ByteVector& input)
{
	ByteVector result;

	CFErrorRef error;
	SecTransformRef encoder = SecEncodeTransformCreate(kSecBase64Encoding, &error);
    if(nullptr == encoder) {
		CFShow(error); 
		return TagLib::ByteVector::null;
	}

	CFDataRef sourceData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)input.data(), (CFIndex)input.size(), kCFAllocatorNull);
	if(nullptr == sourceData) {
		CFRelease(encoder), encoder = nullptr;
		
		return TagLib::ByteVector::null;
	}

    if(!SecTransformSetAttribute(encoder, kSecTransformInputAttributeName, sourceData, &error)) {
		CFShow(error); 
		
		CFRelease(sourceData), sourceData = nullptr;
		CFRelease(encoder), encoder = nullptr;
		
		return TagLib::ByteVector::null;
	}

	CFTypeRef encodedData = SecTransformExecute(encoder, &error);
	if(nullptr == encodedData) {
		CFShow(error); 
		
		CFRelease(sourceData), sourceData = nullptr;
		CFRelease(encoder), encoder = nullptr;
		
		return TagLib::ByteVector::null;
	}

	result.setData((const char *)CFDataGetBytePtr((CFDataRef)encodedData), (TagLib::uint)CFDataGetLength((CFDataRef)encodedData));

	CFRelease(encodedData), encodedData = nullptr;
	CFRelease(sourceData), sourceData = nullptr;
	CFRelease(encoder), encoder = nullptr;

	return result;
}
