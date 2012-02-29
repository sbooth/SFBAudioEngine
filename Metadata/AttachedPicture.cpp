/*
 *  Copyright (C) 2012 Stephen F. Booth <me@sbooth.org>
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

#include "AttachedPicture.h"
#include "CFDictionaryUtilities.h"

// ========================================
// Key names for the metadata dictionary
// ========================================
const CFStringRef	kAttachedPictureTypeKey					= CFSTR("Picture Type");
const CFStringRef	kAttachedPictureDescriptionKey			= CFSTR("Picture Description");
const CFStringRef	kAttachedPictureDataKey					= CFSTR("Picture Data");

AttachedPicture::AttachedPicture(CFDataRef data, AttachedPicture::Type type, CFStringRef description)
	: mState(0)
{
	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	mChangedMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if(data)
		CFDictionarySetValue(mMetadata, kAttachedPictureDataKey, data);

	AddIntToDictionary(mMetadata, kAttachedPictureTypeKey, static_cast<int>(type));

	if(description)
		CFDictionarySetValue(mMetadata, kAttachedPictureDescriptionKey, description);
}

AttachedPicture::~AttachedPicture()
{
	if(mMetadata)
		CFRelease(mMetadata), mMetadata = nullptr;
	
	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = nullptr;
}

#pragma mark Type-Specific Access

AttachedPicture::Type AttachedPicture::GetType() const
{
	AttachedPicture::Type type = Type::Other;
	CFNumberRef wrapper = GetNumberValue(kAttachedPictureTypeKey);
	if(wrapper)
		CFNumberGetValue(wrapper, kCFNumberIntType, &type);
	return type;
}

void AttachedPicture::SetType(Type type)
{
	CFNumberRef wrapper = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type);
	SetValue(kAttachedPictureTypeKey, wrapper);
	CFRelease(wrapper), wrapper = nullptr;
}

CFStringRef AttachedPicture::GetDescription() const
{
	return GetStringValue(kAttachedPictureDescriptionKey);
}

void AttachedPicture::SetDescription(CFStringRef description)
{
	SetValue(kAttachedPictureDescriptionKey, description);
}

CFDataRef AttachedPicture::GetData() const
{
	return GetDataValue(kAttachedPictureDataKey);
}

void AttachedPicture::SetData(CFDataRef data)
{
	SetValue(kAttachedPictureDataKey, data);
}

#pragma mark Type-Specific Access

CFStringRef AttachedPicture::GetStringValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFStringGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return reinterpret_cast<CFStringRef>(value);
}

CFNumberRef AttachedPicture::GetNumberValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return reinterpret_cast<CFNumberRef>(value);
}

CFDataRef AttachedPicture::GetDataValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFDataGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return reinterpret_cast<CFDataRef>(value);
}

#pragma mark Generic Access

CFTypeRef AttachedPicture::GetValue(CFStringRef key) const
{
	if(nullptr == key)
		return nullptr;

	if(CFDictionaryContainsKey(mChangedMetadata, key)) {
		CFTypeRef value = CFDictionaryGetValue(mChangedMetadata, key);
		return (kCFNull == value ? nullptr : value);
	}

	return CFDictionaryGetValue(mMetadata, key);
}

void AttachedPicture::SetValue(CFStringRef key, CFTypeRef value)
{
	if(nullptr == key)
		return;

	if(nullptr == value) {
		if(CFDictionaryContainsKey(mMetadata, key))
			CFDictionarySetValue(mChangedMetadata, key, kCFNull);
		else
			CFDictionaryRemoveValue(mChangedMetadata, key);
	}
	else {
		if(CFDictionaryContainsKey(mChangedMetadata, key)) {
			CFTypeRef savedValue = CFDictionaryGetValue(mMetadata, key);
			if(nullptr != savedValue && CFEqual(savedValue, value))
				CFDictionaryRemoveValue(mChangedMetadata, key);
			else
				CFDictionarySetValue(mChangedMetadata, key, value);
		}
		else
			CFDictionarySetValue(mChangedMetadata, key, value);
	}
}

void AttachedPicture::MergeChangedMetadataIntoMetadata()
{
	CFIndex count = CFDictionaryGetCount(mChangedMetadata);

	CFTypeRef *keys = static_cast<CFTypeRef *>(malloc(sizeof(CFTypeRef) * count));
	CFTypeRef *values = static_cast<CFTypeRef *>(malloc(sizeof(CFTypeRef) * count));

	CFDictionaryGetKeysAndValues(mChangedMetadata, keys, values);

	for(CFIndex i = 0; i < count; ++i) {
		if(kCFNull == values[i])
			CFDictionaryRemoveValue(mMetadata, keys[i]);
		else
			CFDictionarySetValue(mMetadata, keys[i], values[i]);
	}

	free(keys), keys = nullptr;
	free(values), values = nullptr;

	CFDictionaryRemoveAllValues(mChangedMetadata);
}
