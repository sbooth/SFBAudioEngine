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

/*! @file CFWrapper.h @brief A  wrapper around a Core Foundation object */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*!
	 * @brief A wrapper around a Core Foundation object
	 *
	 * \c CFWrapper simplifies the use of CFTypes in C++ by wrapping a CF object, ensuring
	 * \c CFRelease will be called when the \c CFWrapper goes out of scope.
	 * @tparam T A \c CFType
	 */
	template <typename T>
	class CFWrapper
	{
	public:
		
		// ========================================
		/*! @name Creation and Destruction */
		//@{

		/*! @brief Create a new \c CFWrapper */
		inline CFWrapper()						: CFWrapper(nullptr)					{}

		/*!
		 * @brief Create a new \c CFWrapper
		 * @note The \c CFWrapper takes ownership of \c object
		 * @param object The object to wrap
		 */
		inline CFWrapper(T object)				: CFWrapper(object, true)				{}

		/*!
		 * @brief Create a new \c CFWrapper
		 * @param object The object to wrap
		 * @param release Whether this \c CFWrapper should take ownership of \c object
		 */
		CFWrapper(T object, bool release)		: mObject(object), mRelease(release)	{}


		/*! @brief Create a new \c CFWrapper */
		CFWrapper(CFWrapper&& rhs)
			: mObject(rhs.mObject), mRelease(rhs.mRelease)
		{
			rhs.mObject = nullptr;
		}

		/*! @brief Create a new \c CFWrapper */
		CFWrapper(const CFWrapper& rhs)
			: mObject(rhs.mObject), mRelease(rhs.mRelease)
		{
			if(mObject && mRelease)
				CFRetain(mObject);
		}

		/*! @brief Destroy this \c CFWrapper and ensure \c CFRelease() is called if necessary */
		~CFWrapper()
		{
			if(mObject && mRelease)
				CFRelease(mObject);
			mObject = nullptr;
		}

		//@}


		/*! @name Assignment */
		//@{

		/*!
		 * @brief Replace the wrapped object
		 * @note The \c CFWrapper takes ownership of \c rhs
		 * @param rhs The object to wrap
		 */
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

		/*! @brief Replace the wrapped object */
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

		/*! @brief Replace the wrapped object */
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

		//@}


		// ========================================
		/*! @name Pointer management */
		//@{

		/*! @brief Relinquish ownership of the wrapped object and return it */
		inline T Relinquish()
		{
			T object = mObject;
			mObject = nullptr;

			return object;
		}

		//@}


		// ========================================
		/*! @name Equality testing */
		//@{

		/*! @brief Test two \c CFWrapper objects for equality using \c CFEqual() */
		inline bool operator==(const CFWrapper& rhs) const		{ return CFEqual(mObject, rhs.mObject); }

		/*! @brief Test two \c CFWrapper objects for inequality */
		inline bool operator!=(const CFWrapper& rhs) const		{ return !operator==(rhs); }

		//@}


		// ========================================
		/*! @name CoreFoundation object access */
		//@{

		/*! @brief Check whether the wrapped object is \c nullptr */
		inline operator bool() const							{ return nullptr != mObject; }

		/*! @brief Get the wrapped object */
		inline operator T() const								{ return mObject; }

		
		/*! @brief Get the wrapped object */
		inline T Object() const									{ return mObject; }

		//@}

	private:
		T mObject;				/*!< The Core Foundation object */
		bool mRelease;			/*!< Whether \c CFRelease should be called on destruction or reassignment */
	};

	// ========================================
	// Typedefs for common CoreFoundation types

	typedef CFWrapper<CFTypeRef> CFType;										/*!< @brief A wrapped \c CFTypeRef */
	typedef CFWrapper<CFDataRef> CFData;										/*!< @brief A wrapped \c CFDataRef */
	typedef CFWrapper<CFMutableDataRef> CFMutableData;							/*!< @brief A wrapped \c CFMutableDataRef */
	typedef CFWrapper<CFStringRef> CFString;									/*!< @brief A wrapped \c CFStringRef */
	typedef CFWrapper<CFMutableStringRef> CFMutableString;						/*!< @brief A wrapped \c CFMutableStringRef */
	typedef CFWrapper<CFAttributedStringRef> CFAttributedString;				/*!< @brief A wrapped \c CFAttributedStringRef */
	typedef CFWrapper<CFMutableAttributedStringRef> CFMutableAttributedString;	/*!< @brief A wrapped \c CFMutableAttributedStringRef */
	typedef CFWrapper<CFDictionaryRef> CFDictionary;							/*!< @brief A wrapped \c CFDictionaryRef */
	typedef CFWrapper<CFMutableDictionaryRef> CFMutableDictionary;				/*!< @brief A wrapped \c CFMutableDictionaryRef */
	typedef CFWrapper<CFArrayRef> CFArray;										/*!< @brief A wrapped \c CFArrayRef */
	typedef CFWrapper<CFMutableArrayRef> CFMutableArray;						/*!< @brief A wrapped \c CFMutableArrayRef */
	typedef CFWrapper<CFSetRef> CFSet;											/*!< @brief A wrapped \c CFSetRef */
	typedef CFWrapper<CFMutableSetRef> CFMutableSet;							/*!< @brief A wrapped \c CFMutableSetRef */
	typedef CFWrapper<CFBagRef> CFBag;											/*!< @brief A wrapped \c CFBagRef */
	typedef CFWrapper<CFMutableBagRef> CFMutableBag;							/*!< @brief A wrapped \c CFMutableBagRef */
	typedef CFWrapper<CFPropertyListRef> CFPropertyList;						/*!< @brief A wrapped \c CFPropertyListRef */
	typedef CFWrapper<CFBitVectorRef> CFBitVector;								/*!< @brief A wrapped \c CFBitVectorRef */
	typedef CFWrapper<CFMutableBitVectorRef> CFMutableBitVector;				/*!< @brief A wrapped \c CFMutableBitVectorRef */
	typedef CFWrapper<CFCharacterSetRef> CFCharacterSet;						/*!< @brief A wrapped \c CFCharacterSetRef */
	typedef CFWrapper<CFMutableCharacterSetRef> CFMutableCharacterSet;			/*!< @brief A wrapped \c CFMutableCharacterSetRef */
	typedef CFWrapper<CFURLRef> CFURL;											/*!< @brief A wrapped \c CFURLRef */
	typedef CFWrapper<CFUUIDRef> CFUUID;										/*!< @brief A wrapped \c CFUUIDRef */
	typedef CFWrapper<CFNumberRef> CFNumber;									/*!< @brief A wrapped \c CFNumberRef */
	typedef CFWrapper<CFBooleanRef> CFBoolean;									/*!< @brief A wrapped \c CFBooleanRef */
	typedef CFWrapper<CFErrorRef> CFError;										/*!< @brief A wrapped \c CFErrorRef */
	typedef CFWrapper<CFDateRef> CFDate;										/*!< @brief A wrapped \c CFDateRef */
#if !TARGET_OS_IPHONE
	typedef CFWrapper<SecKeychainItemRef> SecKeychainItem;						/*!< @brief A wrapped \c SecKeychainItemRef */
	typedef CFWrapper<SecCertificateRef> SecCertificate;						/*!< @brief A wrapped \c SecCertificateRef */
	typedef CFWrapper<SecTransformRef> SecTransform;							/*!< @brief A wrapped \c SecTransformRef */
	typedef CFWrapper<CGImageSourceRef> CGImageSource;							/*!< @brief A wrapped \c CGImageSourceRef */
#endif

}

/*! @cond */

template <typename T>
std::ostream& operator<<(std::ostream& out, SFB::CFWrapper<T> obj)
{
	out << (T)obj;
	return out;
}

/*! @endcond */
