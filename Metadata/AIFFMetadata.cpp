/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/aifffile.h>

#include "AIFFMetadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddID3v2TagToDictionary.h"
#include "SetID3v2TagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterAIFFMetadata() __attribute__ ((constructor));
	void RegisterAIFFMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::AIFFMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::AIFFMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("aiff"), CFSTR("aif") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::AIFFMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/aiff") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::AIFFMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("aiff"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("aif"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::AIFFMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/aiff"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::AIFFMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new AIFFMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::AIFFMetadata::AIFFMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::AIFFMetadata::_ReadMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
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

	TagLib::RIFF::AIFF::File file(stream.get());
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid AIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an AIFF file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("AIFF"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->sampleWidth());
		if(properties->sampleFrames())
			AddLongLongToDictionary(mMetadata, kTotalFramesKey, properties->sampleFrames());
	}

	if(file.tag())
		AddID3v2TagToDictionary(mMetadata, mPictures, file.tag());

	return true;
}

bool SFB::Audio::AIFFMetadata::_WriteMetadata(CFErrorRef *error)
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

	TagLib::RIFF::AIFF::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid AIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an AIFF file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	SetID3v2TagFromMetadata(*this, file.tag());

	if(!file.save()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid AIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
