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

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/trueaudiofile.h>
#include <taglib/tag.h>

#include "TrueAudioMetadata.h"
#include "CFErrorUtilities.h"
#include "TagLibStringUtilities.h"
#include "AddAudioPropertiesToDictionary.h"
#include "AddID3v1TagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "SetID3v1TagFromMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "CFDictionaryUtilities.h"

#pragma mark Static Methods

CFArrayRef TrueAudioMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("tta") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef TrueAudioMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/x-tta") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool TrueAudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("tta"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool TrueAudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-tta"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

TrueAudioMetadata::TrueAudioMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

TrueAudioMetadata::~TrueAudioMetadata()
{}

#pragma mark Functionality

bool TrueAudioMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	
	// TODO: Use unique_ptr once the switch to C++11 STL is made
	std::auto_ptr<TagLib::FileStream> stream(new TagLib::FileStream(reinterpret_cast<const char *>(buf), true));
	if(!stream->isOpen()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for reading."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);

			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	TagLib::TrueAudio::File file(stream.get());
	if(!file.isValid()) {
		if(nullptr != error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a True Audio file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("True Audio"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->bitsPerSample())
			AddIntToDictionary(mMetadata, kPropertiesBitsPerChannelKey, properties->bitsPerSample());
		if(properties->sampleFrames())
			AddIntToDictionary(mMetadata, kPropertiesTotalFramesKey, properties->sampleFrames());
	}

	// Add all tags that are present
	if(file.ID3v1Tag())
		AddID3v1TagToDictionary(mMetadata, file.ID3v1Tag());

	if(file.ID3v2Tag()) {
		std::vector<AttachedPicture *> pictures;
		AddID3v2TagToDictionary(mMetadata, pictures, file.ID3v2Tag());
		for(auto picture : pictures)
			AddSavedPicture(picture);
	}

	return true;
}

bool TrueAudioMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	// TODO: Use unique_ptr once the switch to C++11 STL is made
	std::auto_ptr<TagLib::FileStream> stream(new TagLib::FileStream(reinterpret_cast<const char *>(buf)));
	if(!stream->isOpen()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for writing."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);

			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	TagLib::TrueAudio::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a True Audio file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	// ID3v1 tags are only written if present, but ID3v2 tags are always written

	if(file.ID3v1Tag())
		SetID3v1TagFromMetadata(*this, file.ID3v1Tag());

	SetID3v2TagFromMetadata(*this, file.ID3v2Tag(true));

	if(!file.save()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), "");
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
