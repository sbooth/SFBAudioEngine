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
#include <taglib/apefile.h>
#include <taglib/tag.h>

#include "MonkeysAudioMetadata.h"
#include "CFErrorUtilities.h"
#include "AddID3v1TagToDictionary.h"
#include "AddAPETagToDictionary.h"
#include "SetAPETagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

#pragma mark Static Methods

CFArrayRef MonkeysAudioMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ape") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MonkeysAudioMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/monkeys-audio") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MonkeysAudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ape"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MonkeysAudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/monkeys-audio"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MonkeysAudioMetadata::MonkeysAudioMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

MonkeysAudioMetadata::~MonkeysAudioMetadata()
{}

#pragma mark Functionality

bool MonkeysAudioMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
	TagLib::APE::File file(stream);
	
	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("Monkey's Audio"));
	
	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);
		
		if(properties->bitsPerSample())
			AddIntToDictionary(mMetadata, kPropertiesBitsPerChannelKey, properties->bitsPerSample());
		if(properties->sampleFrames())
			AddIntToDictionary(mMetadata, kPropertiesTotalFramesKey, properties->sampleFrames());
	}

	if(file.ID3v1Tag())
		AddID3v1TagToDictionary(mMetadata, file.ID3v1Tag());

	if(file.APETag()) {
		std::vector<AttachedPicture *> pictures;
		AddAPETagToDictionary(mMetadata, pictures, file.APETag());
		for(auto picture : pictures)
			AddSavedPicture(picture);
	}

	return true;
}

bool MonkeysAudioMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf));
	TagLib::APE::File file(stream, false);
	
	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// Although both ID3v1 and APE tags are read, only APE tags are written
	if(file.APETag())
		SetAPETagFromMetadata(*this, file.APETag());

	if(!file.save()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Unable to write metadata"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	MergeChangedMetadataIntoMetadata();
	
	return true;
}
