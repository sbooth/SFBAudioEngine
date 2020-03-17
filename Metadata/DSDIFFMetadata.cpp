/*
 * Copyright (c) 2018 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/dsdifffile.h>
#include <taglib/tfilestream.h>

#include "AddAudioPropertiesToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "AddTagToDictionary.h"
#include "CFDictionaryUtilities.h"
#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "DSDIFFMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "SetTagFromMetadata.h"
#include "TagLibStringUtilities.h"

namespace {

	void RegisterDSDIFFMetadata() __attribute__ ((constructor));
	void RegisterDSDIFFMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::DSDIFFMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::DSDIFFMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("dff") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::DSDIFFMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/dff") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::DSDIFFMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("dff"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::DSDIFFMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/dff"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::DSDIFFMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new DSDIFFMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::DSDIFFMetadata::DSDIFFMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::DSDIFFMetadata::_ReadMetadata(CFErrorRef *error)
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

	TagLib::DSDIFF::File file(stream.get());
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSDIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSDIFF file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("DSD Interchange File"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->bitsPerSample())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->bitsPerSample());
		if(properties->sampleCount())
			AddLongLongToDictionary(mMetadata, kTotalFramesKey, properties->sampleCount());
	}

	if(file.hasDIINTag())
		AddTagToDictionary(mMetadata, file.DIINTag());

	if(file.hasID3v2Tag())
		AddID3v2TagToDictionary(mMetadata, mPictures, file.ID3v2Tag());

	return true;
}

bool SFB::Audio::DSDIFFMetadata::_WriteMetadata(CFErrorRef *error)
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

	TagLib::DSDIFF::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSDIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSDIFF file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	SetTagFromMetadata(*this, file.DIINTag());
	SetID3v2TagFromMetadata(*this, file.ID3v2Tag());

	if(!file.save()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSDIFF file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
