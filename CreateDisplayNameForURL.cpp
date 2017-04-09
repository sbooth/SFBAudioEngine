/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <ApplicationServices/ApplicationServices.h>
#endif

#include "CreateDisplayNameForURL.h"
#include "CFWrapper.h"
#include "Logger.h"

CFStringRef
SFB::CreateDisplayNameForURL(CFURLRef url)
{
	assert(nullptr != url);

	CFStringRef displayName = nullptr;

#if !TARGET_OS_IPHONE
	SFB::CFString scheme(CFURLCopyScheme(url));
	if(scheme) {
		bool isFileURL = (kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive));

		if(isFileURL) {
			OSStatus result = LSCopyDisplayNameForURL(url, &displayName);

			if(noErr != result) {
				LOGGER_WARNING("org.sbooth.AudioEngine", "LSCopyDisplayNameForURL failed: " << result);
				displayName = CFURLCopyLastPathComponent(url);
			}
		}
		else {
			displayName = CFURLGetString(url);
			CFRetain(displayName);
		}
	}
	// If scheme is nullptr the URL is probably invalid, but can still be logged
	else {
		displayName = CFURLGetString(url);
		CFRetain(displayName);
	}
#else
	displayName = CFURLGetString(url);
	CFRetain(displayName);
#endif

	return displayName;
}
