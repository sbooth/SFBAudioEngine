/*
 * Copyright (c) 2012 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
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
	: mMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), mChangedMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), mState(ChangeState::Saved)
{
	if(data)
		CFDictionarySetValue(mMetadata, kDataKey, data);

	AddIntToDictionary(mMetadata, kTypeKey, (int)type);

	if(description)
		CFDictionarySetValue(mMetadata, kDescriptionKey, description);
}

#pragma mark External Representations

CFDictionaryRef SFB::Audio::AttachedPicture::CreateDictionaryRepresentation() const
{
	CFMutableDictionaryRef dictionaryRepresentation = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, mMetadata);

	CFIndex count = CFDictionaryGetCount(mChangedMetadata);

	CFTypeRef *keys = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);
	CFTypeRef *values = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);

	CFDictionaryGetKeysAndValues(mChangedMetadata, keys, values);

	for(CFIndex i = 0; i < count; ++i) {
		if(kCFNull == values[i])
			CFDictionaryRemoveValue(dictionaryRepresentation, keys[i]);
		else
			CFDictionarySetValue(dictionaryRepresentation, keys[i], values[i]);
	}

	free(keys), keys = nullptr;
	free(values), values = nullptr;

	return dictionaryRepresentation;
}

bool SFB::Audio::AttachedPicture::SetFromDictionaryRepresentation(CFDictionaryRef dictionary)
{
	if(nullptr == dictionary)
		return false;

	SetValue(kTypeKey, CFDictionaryGetValue(dictionary, kTypeKey));
	SetValue(kDescriptionKey, CFDictionaryGetValue(dictionary, kDescriptionKey));
	SetValue(kDataKey, CFDictionaryGetValue(dictionary, kDataKey));

	return true;
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
	SFB::CFNumber wrapper(kCFNumberIntType, &type);
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
