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
#include <taglib/flacfile.h>
#include <taglib/flacproperties.h>
#include <taglib/id3v2framefactory.h>

#include <ApplicationServices/ApplicationServices.h>

#include "FLACMetadata.h"
#include "Logger.h"
#include "CFErrorUtilities.h"
#include "AddID3v1TagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "AddXiphCommentToDictionary.h"
#include "SetXiphCommentFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

#pragma mark Static Methods

CFArrayRef FLACMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("flac") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef FLACMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/flac") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool FLACMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

FLACMetadata::FLACMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

FLACMetadata::~FLACMetadata()
{}

#pragma mark Functionality

bool FLACMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);

	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
	TagLib::FLAC::File file(stream, TagLib::ID3v2::FrameFactory::instance());

	if(!file.isValid()) {
		if(nullptr != error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a FLAC file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("FLAC"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kPropertiesBitsPerChannelKey, properties->sampleWidth());
		if(properties->sampleFrames())
			AddLongLongToDictionary(mMetadata, kPropertiesTotalFramesKey, properties->sampleFrames());
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

	if(file.xiphComment()) {
		std::vector<AttachedPicture *> pictures;
		AddXiphCommentToDictionary(mMetadata, pictures, file.xiphComment());
		for(auto picture : pictures)
			AddSavedPicture(picture);
	}

	// Add album art
	for(auto iter : file.pictureList()) {
		CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(iter->data().data()), iter->data().size());

		CFStringRef description = nullptr;
		if(!iter->description().isNull())
			description = CFStringCreateWithCString(kCFAllocatorDefault, iter->description().toCString(true), kCFStringEncodingUTF8);

		AttachedPicture *picture = new AttachedPicture(data, static_cast<AttachedPicture::Type>(iter->type()), description);
		AddSavedPicture(picture);

		if(data)
			CFRelease(data), data = nullptr;

		if(description)
			CFRelease(description), description = nullptr;
	}

	return true;
}

bool FLACMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	auto stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf));
	TagLib::FLAC::File file(stream, TagLib::ID3v2::FrameFactory::instance(), false);

	if(!file.isValid()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not a FLAC file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}

		return false;
	}

	SetXiphCommentFromMetadata(*this, file.xiphComment(), false);

	// Remove existing cover art
	file.removePictures();

	// Add album art
	for(auto attachedPicture : GetAttachedPictures()) {
	
		CGImageSourceRef imageSource = CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr);
		if(nullptr == imageSource) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioMetadata.FLAC", "Skipping album art (unable to create image)");
			continue;
		}

		TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture;
		picture->setData(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (TagLib::uint)CFDataGetLength(attachedPicture->GetData())));
		picture->setType(static_cast<TagLib::FLAC::Picture::Type>(attachedPicture->GetType()));
		if(attachedPicture->GetDescription())
			picture->setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));

		// Convert the image's UTI into a MIME type
		CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
		if(mimeType) {
			picture->setMimeType(TagLib::StringFromCFString(mimeType));
			CFRelease(mimeType), mimeType = nullptr;
		}

		// Flesh out the height, width, and depth
		CFDictionaryRef imagePropertiesDictionary = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr);
		if(imagePropertiesDictionary) {
			CFNumberRef imageWidth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelWidth);
			CFNumberRef imageHeight = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelHeight);
			CFNumberRef imageDepth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyDepth);

			int height, width, depth;

			// Ignore numeric conversion errors
			CFNumberGetValue(imageWidth, kCFNumberIntType, &width);
			CFNumberGetValue(imageHeight, kCFNumberIntType, &height);
			CFNumberGetValue(imageDepth, kCFNumberIntType, &depth);

			picture->setHeight(height);
			picture->setWidth(width);
			picture->setColorDepth(depth);

			CFRelease(imagePropertiesDictionary), imagePropertiesDictionary = nullptr;
		}

		file.addPicture(picture);

		CFRelease(imageSource), imageSource = nullptr;
	}

	if(!file.save()) {
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), "");
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
