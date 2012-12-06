/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include <taglib/wavfile.h>

#include "WAVEMetadata.h"
#include "CFErrorUtilities.h"
#include "AddTagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "SetTagFromMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

#pragma mark Static Methods

CFArrayRef WAVEMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wave"), CFSTR("wav") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 2, &kCFTypeArrayCallBacks);
}

CFArrayRef WAVEMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wave") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool WAVEMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wave"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wav"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool WAVEMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wave"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

WAVEMetadata::WAVEMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

WAVEMetadata::~WAVEMetadata()
{}

#pragma mark Functionality

bool WAVEMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
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

	TagLib::RIFF::WAV::File file(stream.get());
	if(!file.isValid()) {
		if(nullptr != error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a WAVE file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("WAVE"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);
		
		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kPropertiesBitsPerChannelKey, properties->sampleWidth());
		if(properties->sampleFrames())
			AddIntToDictionary(mMetadata, kPropertiesTotalFramesKey, properties->sampleFrames());
	}

	if(file.InfoTag())
		AddTagToDictionary(mMetadata, file.InfoTag());

	if(file.ID3v2Tag()) {
		std::vector<AttachedPicture *> pictures;
		AddID3v2TagToDictionary(mMetadata, pictures, file.ID3v2Tag());
		for(auto picture : pictures)
			AddSavedPicture(picture);
	}
	
	return true;
}

bool WAVEMetadata::WriteMetadata(CFErrorRef *error)
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

	TagLib::RIFF::WAV::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a WAVE file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// An Info tag is only written if present, but ID3v2 tags are always written

	// TODO: Should other field names from the Info tag be handled?
	if(file.InfoTag())
		SetTagFromMetadata(*this, file.InfoTag());

	SetID3v2TagFromMetadata(*this, file.ID3v2Tag());

	if(!file.save()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
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
