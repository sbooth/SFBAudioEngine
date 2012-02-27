/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <taglib/tfilestream.h>
#include <taglib/itfile.h>
#include <taglib/xmfile.h>
#include <taglib/s3mfile.h>
#include <taglib/modfile.h>

#include "MODMetadata.h"
#include "CFErrorUtilities.h"
#include "Logger.h"
#include "AddAudioPropertiesToDictionary.h"
#include "AddTagToDictionary.h"

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
	if(nullptr == extension)
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
	if(nullptr == mimeType)
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

	CFStringRef pathExtension = CFURLCopyPathExtension(mURL);
	if(nullptr == pathExtension)
		return false;

	bool fileIsValid = false;
	if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("it"), kCFCompareCaseInsensitive)) {
		auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
		TagLib::IT::File file(stream);

		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MOD (Impulse Tracker)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("xm"), kCFCompareCaseInsensitive)) {
		auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
		TagLib::XM::File file(stream);

		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MOD (Extended Module)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("s3m"), kCFCompareCaseInsensitive)) {
		auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
		TagLib::S3M::File file(stream);

		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MOD (ScreamTracker III)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("mod"), kCFCompareCaseInsensitive)) {
		auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
		TagLib::Mod::File file(stream);

		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MOD (Protracker)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}

	CFRelease(pathExtension), pathExtension = nullptr;

	if(!fileIsValid) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a MOD file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	return true;
}

bool MODMetadata::WriteMetadata(CFErrorRef */*error*/)
{
	LOGGER_NOTICE("org.sbooth.AudioEngine.AudioMetadata.MOD", "Writing of MOD metadata is not supported");

	return false;
}
