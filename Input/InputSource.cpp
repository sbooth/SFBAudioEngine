/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include "AudioEngineDefines.h"
#include "InputSource.h"
#include "FileInputSource.h"
#include "MemoryMappedFileInputSource.h"
#include "InMemoryFileInputSource.h"


// ========================================
// Error Codes
// ========================================
const CFStringRef	InputSourceErrorDomain			= CFSTR("org.sbooth.SFBAudioEngine.ErrorDomain.InputSource");


#pragma mark Static Methods


InputSource * InputSource::CreateInputSourceForURL(CFURLRef url, int flags, CFErrorRef *error)
{
	assert(NULL != url);
	
	InputSource *inputSource = NULL;
	
	// If this is a file URL, use the extension-based resolvers
	CFStringRef scheme = CFURLCopyScheme(url);
	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));
		
		if(NULL != fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				if(InputSourceFlagMemoryMapFiles & flags)
					inputSource = new MemoryMappedFileInputSource(url);
				else if(InputSourceFlagLoadFilesInMemory & flags)
					inputSource = new InMemoryFileInputSource(url);
				else
					inputSource = new FileInputSource(url);

				if(!inputSource->Open(error))
					delete inputSource, inputSource = NULL;
			}
			else {
				LOG("The requested URL doesn't exist");
				
				if(error) {
					CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																					   32,
																					   &kCFTypeDictionaryKeyCallBacks,
																					   &kCFTypeDictionaryValueCallBacks);
					
					CFStringRef displayName = CFURLCopyLastPathComponent(url);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” does not exist."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("File not found"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file may exist on removable media or may have been deleted."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					*error = CFErrorCreate(kCFAllocatorDefault, 
										   InputSourceErrorDomain, 
										   InputSourceFileNotFoundError,
										   errorDictionary);
					
					CFRelease(errorDictionary), errorDictionary = NULL;				
				}				
			}
		}
		else
			ERR("CFURLCreatePropertyFromResource failed: %i", errorCode);		
		
		CFRelease(fileExists), fileExists = NULL;
	}
	else {
	}
	
	CFRelease(scheme), scheme = NULL;
	
	return inputSource;
}


#pragma mark Creation and Destruction


InputSource::InputSource()
	: mURL(NULL)
{}

InputSource::InputSource(CFURLRef url)
	: mURL(NULL)
{
	assert(NULL != url);
	
	mURL = static_cast<CFURLRef>(CFRetain(url));
}

InputSource::InputSource(const InputSource& rhs)
	: mURL(NULL)
{
	*this = rhs;
}

InputSource::~InputSource()
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
}


#pragma mark Operator Overloads


InputSource& InputSource::operator=(const InputSource& rhs)
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
	
	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));
	
	return *this;
}

