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

#include <taglib/tfilestream.h>
#include <taglib/mpegfile.h>
#include <taglib/mpegproperties.h>
#include <taglib/xingheader.h>

#include "MP3Metadata.h"
#include "CFErrorUtilities.h"
#include "AddAPETagToDictionary.h"
#include "AddID3v1TagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "SetID3v2TagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

#pragma mark Static Methods

CFArrayRef MP3Metadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mp3") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MP3Metadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MP3Metadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp3"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool MP3Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MP3Metadata::MP3Metadata(CFURLRef url)
	: AudioMetadata(url)
{}

MP3Metadata::~MP3Metadata()
{}

#pragma mark Functionality

bool MP3Metadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
	TagLib::MPEG::File file(stream, TagLib::ID3v2::FrameFactory::instance());
	
	if(!file.isValid()) {
		if(nullptr != error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MP3"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		// TODO: Is this too much information?
#if 0
		switch(properties->version()) {
			case TagLib::MPEG::Header::Version1:
				switch(properties->layer()) {
					case 1:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-1 Layer I"));		break;
					case 2:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-1 Layer II"));	break;
					case 3:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-1 Layer III"));	break;
				}
				break;
			case TagLib::MPEG::Header::Version2:
				switch(properties->layer()) {
					case 1:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2 Layer I"));		break;
					case 2:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2 Layer II"));	break;
					case 3:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2 Layer III"));	break;
				}
				break;
			case TagLib::MPEG::Header::Version2_5:
				switch(properties->layer()) {
					case 1:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2.5 Layer I"));	break;
					case 2:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2.5 Layer II"));	break;
					case 3:		CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("MPEG-2.5 Layer III"));	break;
				}
				break;
		}
#endif

		if(properties->xingHeader() && properties->xingHeader()->totalFrames())
			AddIntToDictionary(mMetadata, kPropertiesTotalFramesKey, properties->xingHeader()->totalFrames());
	}

	if(file.APETag()) {
		std::vector<AttachedPicture *> pictures;
		AddAPETagToDictionary(mMetadata, pictures, file.APETag());
		for(auto picture : pictures)
			AddSavedPicture(picture);
	}

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

bool MP3Metadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf));
	TagLib::MPEG::File file(stream, TagLib::ID3v2::FrameFactory::instance(), false);
	
	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	SetID3v2TagFromMetadata(*this, file.ID3v2Tag(true));

	if(!file.save()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG file."), "");
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
