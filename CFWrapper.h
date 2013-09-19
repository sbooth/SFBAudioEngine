/*
 *  Copyright (C) 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#pragma once

#include <iostream>

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <Security/Security.h>
# include <ImageIO/ImageIO.h>
#endif

namespace SFB {

	// ========================================
	// A wrapper around a Core Foundation object
	// CFWrapper simplifies the use of CFTypes in C++ by wrapping a CF object, ensuring
	// CFRelease will be called when the object goes out of scope.
	// ========================================
	template <typename T>
	class CFWrapper
	{
	public:
		
		// ========================================
		// Creation and Destruction

		// If release is true or omitted then object will be released when this object goes out of scope
		inline CFWrapper()						: CFWrapper(nullptr)					{}
		inline CFWrapper(T object)				: CFWrapper(object, true)				{}
		CFWrapper(T object, bool release)		: mObject(object), mRelease(release)	{}

		CFWrapper(CFWrapper&& rhs)
			: mObject(rhs.mObject), mRelease(rhs.mRelease)
		{
			rhs.mObject = nullptr;
		}

		CFWrapper(const CFWrapper& rhs)
			: mObject(rhs.mObject), mRelease(rhs.mRelease)
		{
			if(mObject && mRelease)
				CFRetain(mObject);
		}

		~CFWrapper()
		{
			if(mObject && mRelease)
				CFRelease(mObject);
			mObject = nullptr;
		}

		// This object will take ownership of T (i.e., it will consume a reference)
		CFWrapper& operator=(const T& rhs)
		{
			if(mObject != rhs) {
				if(mObject && mRelease)
					CFRelease(mObject);

				mObject = rhs;
				mRelease = true;
			}
			
			return *this;
		}

		CFWrapper& operator=(const CFWrapper& rhs)
		{
			if(mObject != rhs.mObject) {
				if(mObject && mRelease)
					CFRelease(mObject);

				mObject = rhs.mObject;
				mRelease = rhs.mRelease;

				if(mObject && mRelease)
					CFRetain(mObject);
			}
			
			return *this;
		}

		CFWrapper& operator=(CFWrapper&& rhs)
		{
			if(mObject != rhs.mObject) {
				if(mObject && mRelease)
					CFRelease(mObject);

				mObject = rhs.mObject;
				mRelease = rhs.mRelease;

				rhs.mObject = nullptr;
			}

			return *this;
		}

		// ========================================
		// Pointer management

		inline T Relinquish()
		{
			T object = mObject;
			mObject = nullptr;

			return object;
		}

		// ========================================
		// Equality testing

		// Convenience methods for equality testing (wraps CFEqual)
		inline bool operator==(const CFWrapper& rhs) const		{ return CFEqual(mObject, rhs.mObject); }
		inline bool operator!=(const CFWrapper& rhs) const		{ return !operator==(rhs); }

		// ========================================
		// CoreFoundation object access

		inline operator bool() const							{ return nullptr != mObject; }
		inline operator T() const								{ return mObject; }

	private:
		T mObject;
		bool mRelease;
	};

	// ========================================
	// Typedefs for common CoreFoundation types

	typedef CFWrapper<CFTypeRef> CFType;
	typedef CFWrapper<CFDataRef> CFData;
	typedef CFWrapper<CFMutableDataRef> CFMutableData;
	typedef CFWrapper<CFStringRef> CFString;
	typedef CFWrapper<CFMutableStringRef> CFMutableString;
	typedef CFWrapper<CFAttributedStringRef> CFAttributedString;
	typedef CFWrapper<CFMutableAttributedStringRef> CFMutableAttributedString;
	typedef CFWrapper<CFDictionaryRef> CFDictionary;
	typedef CFWrapper<CFMutableDictionaryRef> CFMutableDictionary;
	typedef CFWrapper<CFArrayRef> CFArray;
	typedef CFWrapper<CFMutableArrayRef> CFMutableArray;
	typedef CFWrapper<CFSetRef> CFSet;
	typedef CFWrapper<CFMutableSetRef> CFMutableSet;
	typedef CFWrapper<CFBagRef> CFBag;
	typedef CFWrapper<CFMutableBagRef> CFMutableBag;
	typedef CFWrapper<CFPropertyListRef> CFPropertyList;
	typedef CFWrapper<CFBitVectorRef> CFBitVector;
	typedef CFWrapper<CFMutableBitVectorRef> CFMutableBitVector;
	typedef CFWrapper<CFCharacterSetRef> CFCharacterSet;
	typedef CFWrapper<CFMutableCharacterSetRef> CFMutableCharacterSet;
	typedef CFWrapper<CFURLRef> CFURL;
	typedef CFWrapper<CFUUIDRef> CFUUID;
	typedef CFWrapper<CFNumberRef> CFNumber;
	typedef CFWrapper<CFBooleanRef> CFBoolean;
	typedef CFWrapper<CFErrorRef> CFError;
	typedef CFWrapper<CFDateRef> CFDate;
#if !TARGET_OS_IPHONE
	typedef CFWrapper<SecKeychainItemRef> SecKeychainItem;
	typedef CFWrapper<SecCertificateRef> SecCertificate;
	typedef CFWrapper<SecTransformRef> SecTransform;
	typedef CFWrapper<CGImageSourceRef> CGImageSource;
#endif

}

template <typename T>
std::ostream& operator<<(std::ostream& out, SFB::CFWrapper<T> obj)
{
	out << (T)obj;
	return out;
}
