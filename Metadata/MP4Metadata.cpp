/*
 * Copyright (c) 2006 - 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/tfilestream.h>
#include <taglib/mp4file.h>

#include "MP4Metadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddMP4TagToDictionary.h"
#include "SetMP4TagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterMP4Metadata() __attribute__ ((constructor));
	void RegisterMP4Metadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::MP4Metadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MP4Metadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("m4a"), CFSTR("mp4") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MP4Metadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg-4") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MP4Metadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("m4a"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MP4Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg-4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::MP4Metadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new MP4Metadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::MP4Metadata::MP4Metadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::MP4Metadata::_ReadMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;

	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream((const char *)buf, true));
	if(!stream->isOpen()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for reading."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Input/output error"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::MP4::File file(stream.get());
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("MP4"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->bitsPerSample())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->bitsPerSample());
		switch(properties->codec()) {
			case TagLib::MP4::AudioProperties::AAC:
				CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("AAC"));
				break;
			case TagLib::MP4::AudioProperties::ALAC:
				CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("Apple Lossless"));
				break;
			default:
				break;
		}
	}

	if(file.tag())
		AddMP4TagToDictionary(mMetadata, mPictures, file.tag());

	return true;
}

bool SFB::Audio::MP4Metadata::_WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream((const char *)buf));
	if(!stream->isOpen()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for writing."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Input/output error"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::MP4::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	SetMP4TagFromMetadata(*this, file.tag());

	if(!file.save()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
