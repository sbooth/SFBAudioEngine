/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <ApplicationServices/ApplicationServices.h>
#endif

#include "CreateDisplayNameForURL.h"
#include "Logger.h"

CFStringRef
CreateDisplayNameForURL(CFURLRef url)
{
	assert(nullptr != url);

	CFStringRef displayName = nullptr;

#if !TARGET_OS_IPHONE
	CFStringRef scheme = CFURLCopyScheme(url);
	if(scheme) {
		bool isFileURL = (kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive));
		CFRelease(scheme), scheme = nullptr;

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
