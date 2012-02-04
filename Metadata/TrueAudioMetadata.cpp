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

#include <taglib/tfilestream.h>
#include <taglib/trueaudiofile.h>
#include <taglib/tag.h>

#include "TrueAudioMetadata.h"
#include "CreateDisplayNameForURL.h"
#include "TagLibStringFromCFString.h"
#include "AddAudioPropertiesToDictionary.h"

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
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("tta"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool TrueAudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
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
	
	TagLib::IOStream *stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
	TagLib::TrueAudio::File file(stream);

	if(!file.isValid()) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a True Audio file"), ""));

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
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("True Audio"));

	if(file.audioProperties()) {
		TagLib::TrueAudio::Properties *properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->bitsPerSample()) {
			TagLib::uint bitDepth = properties->bitsPerSample();
			CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitDepth);
			CFDictionaryAddValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
			CFRelease(bitsPerChannel), bitsPerChannel = NULL;			
		}

		if(properties->sampleFrames()) {
			TagLib::uint sampleFrames = properties->sampleFrames();
			CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &sampleFrames);
			CFDictionaryAddValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
			CFRelease(totalFrames), totalFrames = NULL;			
		}
	}

	// Album title
	if(!file.tag()->album().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, file.tag()->album().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataAlbumTitleKey, str);
		CFRelease(str), str = NULL;
	}

	// Artist
	if(!file.tag()->artist().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, file.tag()->artist().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataArtistKey, str);
		CFRelease(str), str = NULL;
	}

	// Genre
	if(!file.tag()->genre().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, file.tag()->genre().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataGenreKey, str);
		CFRelease(str), str = NULL;
	}

	// Year
	if(file.tag()->year()) {
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), file.tag()->year());
		CFDictionarySetValue(mMetadata, kMetadataReleaseDateKey, str);
		CFRelease(str), str = NULL;
	}

	// Comment
	if(!file.tag()->comment().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, file.tag()->comment().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataCommentKey, str);
		CFRelease(str), str = NULL;
	}

	// Track title
	if(!file.tag()->title().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, file.tag()->title().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataTitleKey, str);
		CFRelease(str), str = NULL;
	}

	// Track number
	if(file.tag()->track()) {
		int trackNum = file.tag()->track();
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
		CFDictionarySetValue(mMetadata, kMetadataTrackNumberKey, num);
		CFRelease(num), num = NULL;
	}

	return true;
}

bool TrueAudioMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	TagLib::IOStream *stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf));
	TagLib::TrueAudio::File file(stream, false);

	if(!file.isValid()) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a True Audio file"), ""));

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

	// Album title
	file.tag()->setAlbum(TagLib::StringFromCFString(GetAlbumTitle()));

	// Artist
	file.tag()->setArtist(TagLib::StringFromCFString(GetArtist()));

	// Genre
	file.tag()->setGenre(TagLib::StringFromCFString(GetGenre()));

	// Year
	file.tag()->setYear(CFStringGetIntValue(GetReleaseDate()));

	// Comment
	file.tag()->setComment(TagLib::StringFromCFString(GetComment()));

	// Track title
	file.tag()->setTitle(TagLib::StringFromCFString(GetTitle()));

	// Track number
	int trackNum = 0;
	if(GetTrackNumber())
		CFNumberGetValue(GetTrackNumber(), kCFNumberIntType, &trackNum);

	file.tag()->setTrack(trackNum);

	if(!file.save()) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid True Audio file."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));

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

	MergeChangedMetadataIntoMetadata();

	return true;
}
