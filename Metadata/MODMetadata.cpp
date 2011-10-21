/*
 *  Copyright (C) 2011 Stephen F. Booth <me@sbooth.org>
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

#include <dumb/dumb.h>

#include "MODMetadata.h"
#include "CreateDisplayNameForURL.h"
#include "Logger.h"

#define DUMB_SAMPLE_RATE	44100
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16

#pragma mark Static Methods

CFArrayRef MODMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("it"), CFSTR("xm"), CFSTR("s3m"), CFSTR("mod") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 4, &kCFTypeArrayCallBacks);
}

CFArrayRef MODMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/it"), CFSTR("audio/xm"), CFSTR("audio/s3m"), CFSTR("audio/mod"), CFSTR("audio/x-mod") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 5, &kCFTypeArrayCallBacks);
}

bool MODMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mod"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MODMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mod"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-mod"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MODMetadata::MODMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

MODMetadata::~MODMetadata()
{}

#pragma mark Functionality

bool MODMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	dumb_register_stdfiles();

	DUH *duh = dumb_load_it(reinterpret_cast<const char *>(buf));
	if(NULL == duh)
		duh = dumb_load_xm(reinterpret_cast<const char *>(buf));
	if(NULL == duh)
		duh = dumb_load_s3m(reinterpret_cast<const char *>(buf));
	if(NULL == duh)
		duh = dumb_load_mod(reinterpret_cast<const char *>(buf));

	if(NULL == duh) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		return false;
	}

	long totalFramesValue = duh_get_length(duh);
	CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &totalFramesValue);
	CFDictionarySetValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
	CFRelease(totalFrames), totalFrames = NULL;
	
	double sampleRateValue = DUMB_SAMPLE_RATE;
	CFNumberRef sampleRate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &sampleRateValue);
	CFDictionarySetValue(mMetadata, kPropertiesSampleRateKey, sampleRate);
	CFRelease(sampleRate), sampleRate = NULL;

	int channelsValue = DUMB_CHANNELS;
	CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &channelsValue);
	CFDictionarySetValue(mMetadata, kPropertiesChannelsPerFrameKey, channelsPerFrame);
	CFRelease(channelsPerFrame), channelsPerFrame = NULL;

	int bitDepthValue = DUMB_BIT_DEPTH;
	CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitDepthValue);
	CFDictionarySetValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
	CFRelease(bitsPerChannel), bitsPerChannel = NULL;

	double durationValue = static_cast<double>(totalFramesValue / sampleRateValue);
	CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &durationValue);
	CFDictionarySetValue(mMetadata, kPropertiesDurationKey, duration);
	CFRelease(duration), duration = NULL;

	const char *tag = duh_get_tag(duh, "TITLE");
	if(tag) {
		CFStringRef title = CFStringCreateWithCString(kCFAllocatorDefault, tag, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataTitleKey, title);
		CFRelease(title), title = NULL;
	}

	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MOD"));

	if(duh)
		unload_duh(duh), duh = NULL;
	
	return true;
}

bool MODMetadata::WriteMetadata(CFErrorRef */*error*/)
{
	LOGGER_NOTICE("org.sbooth.AudioEngine.AudioMetadata.MOD", "Writing of MOD metadata is not supported");

	return false;
}
