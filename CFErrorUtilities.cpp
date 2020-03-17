/*
 * Copyright (c) 2012 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "CreateDisplayNameForURL.h"

CFErrorRef SFB::CreateError(CFStringRef domain, CFIndex code, CFStringRef description, CFStringRef failureReason, CFStringRef recoverySuggestion)
{
	if(nullptr == domain)
		return nullptr;

	SFB::CFMutableDictionary errorDictionary(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if(!errorDictionary)
		return nullptr;

	if(description)
		CFDictionarySetValue(errorDictionary, kCFErrorLocalizedDescriptionKey, description);

	if(failureReason)
		CFDictionarySetValue(errorDictionary, kCFErrorLocalizedFailureReasonKey, failureReason);

	if(recoverySuggestion)
		CFDictionarySetValue(errorDictionary, kCFErrorLocalizedRecoverySuggestionKey, recoverySuggestion);

	return CFErrorCreate(kCFAllocatorDefault, domain, code, errorDictionary);
}

CFErrorRef SFB::CreateErrorForURL(CFStringRef domain, CFIndex code, CFStringRef descriptionFormatStringForURL, CFURLRef url, CFStringRef failureReason, CFStringRef recoverySuggestion)
{
	if(nullptr == domain)
		return nullptr;

	SFB::CFMutableDictionary errorDictionary(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if(!errorDictionary)
		return nullptr;

	if(descriptionFormatStringForURL && url) {
		CFDictionarySetValue(errorDictionary, kCFErrorURLKey, url);

		SFB::CFString displayName(CreateDisplayNameForURL(url));
		if(displayName) {
			SFB::CFString description(nullptr, descriptionFormatStringForURL, displayName.Object());
			if(description)
				CFDictionarySetValue(errorDictionary, kCFErrorLocalizedDescriptionKey, description);
		}
	}

	if(failureReason)
		CFDictionarySetValue(errorDictionary, kCFErrorLocalizedFailureReasonKey, failureReason);

	if(recoverySuggestion)
		CFDictionarySetValue(errorDictionary, kCFErrorLocalizedRecoverySuggestionKey, recoverySuggestion);

	return CFErrorCreate(kCFAllocatorDefault, domain, code, errorDictionary);
}
