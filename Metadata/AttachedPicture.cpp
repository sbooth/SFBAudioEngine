/*
 *  Copyright (C) 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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
const CFStringRef SFB::Audio::AttachedPicture::kTypeKey					= CFSTR("Picture Type");
const CFStringRef SFB::Audio::AttachedPicture::kDescriptionKey			= CFSTR("Picture Description");
const CFStringRef SFB::Audio::AttachedPicture::kDataKey					= CFSTR("Picture Data");

SFB::Audio::AttachedPicture::AttachedPicture(CFDataRef data, AttachedPicture::Type type, CFStringRef description)
	: mState(ChangeState::Saved)
{
	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	mChangedMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if(data)
		CFDictionarySetValue(mMetadata, kDataKey, data);

	AddIntToDictionary(mMetadata, kTypeKey, (int)type);

	if(description)
		CFDictionarySetValue(mMetadata, kDescriptionKey, description);
}

#pragma mark Type-Specific Access

SFB::Audio::AttachedPicture::Type SFB::Audio::AttachedPicture::GetType() const
{
	AttachedPicture::Type type = Type::Other;
	CFNumberRef wrapper = GetNumberValue(kTypeKey);
	if(wrapper)
		CFNumberGetValue(wrapper, kCFNumberIntType, &type);
	return type;
}

void SFB::Audio::AttachedPicture::SetType(Type type)
{
	SFB::CFNumber wrapper = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type);
	SetValue(kTypeKey, wrapper);
}

CFStringRef SFB::Audio::AttachedPicture::GetDescription() const
{
	return GetStringValue(kDescriptionKey);
}

void SFB::Audio::AttachedPicture::SetDescription(CFStringRef description)
{
	SetValue(kDescriptionKey, description);
}

CFDataRef SFB::Audio::AttachedPicture::GetData() const
{
	return GetDataValue(kDataKey);
}

void SFB::Audio::AttachedPicture::SetData(CFDataRef data)
{
	SetValue(kDataKey, data);
}

#pragma mark Type-Specific Access

CFStringRef SFB::Audio::AttachedPicture::GetStringValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFStringGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFStringRef)value;
}

CFNumberRef SFB::Audio::AttachedPicture::GetNumberValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFNumberRef)value;
}

CFDataRef SFB::Audio::AttachedPicture::GetDataValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);

	if(nullptr == value)
		return nullptr;

	if(CFDataGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFDataRef)value;
}

#pragma mark Generic Access

CFTypeRef SFB::Audio::AttachedPicture::GetValue(CFStringRef key) const
{
	if(nullptr == key)
		return nullptr;

	if(CFDictionaryContainsKey(mChangedMetadata, key)) {
		CFTypeRef value = CFDictionaryGetValue(mChangedMetadata, key);
		return (kCFNull == value ? nullptr : value);
	}

	return CFDictionaryGetValue(mMetadata, key);
}

void SFB::Audio::AttachedPicture::SetValue(CFStringRef key, CFTypeRef value)
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

void SFB::Audio::AttachedPicture::MergeChangedMetadataIntoMetadata()
{
	CFIndex count = CFDictionaryGetCount(mChangedMetadata);

	CFTypeRef *keys = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);
	CFTypeRef *values = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);

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
