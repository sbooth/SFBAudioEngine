/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/itfile.h>
#include <taglib/xmfile.h>
#include <taglib/s3mfile.h>
#include <taglib/modfile.h>

#include "MODMetadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"
#include "AddAudioPropertiesToDictionary.h"
#include "AddTagToDictionary.h"

namespace {

	void RegisterMODMetadata() __attribute__ ((constructor));
	void RegisterMODMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::MODMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MODMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("it"), CFSTR("xm"), CFSTR("s3m"), CFSTR("mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 4, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MODMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/it"), CFSTR("audio/xm"), CFSTR("audio/s3m"), CFSTR("audio/mod"), CFSTR("audio/x-mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 5, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MODMetadata::HandlesFilesWithExtension(CFStringRef extension)
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

bool SFB::Audio::MODMetadata::HandlesMIMEType(CFStringRef mimeType)
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

SFB::Audio::Metadata::unique_ptr SFB::Audio::MODMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new MODMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::MODMetadata::MODMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::MODMetadata::_ReadMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	SFB::CFString pathExtension(CFURLCopyPathExtension(mURL));
	if(!pathExtension)
		return false;

	bool fileIsValid = false;
	if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("it"), kCFCompareCaseInsensitive)) {
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

		TagLib::IT::File file(stream.get());
		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("MOD (Impulse Tracker)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("xm"), kCFCompareCaseInsensitive)) {
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

		TagLib::XM::File file(stream.get());
		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("MOD (Extended Module)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("s3m"), kCFCompareCaseInsensitive)) {
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

		TagLib::S3M::File file(stream.get());
		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("MOD (ScreamTracker III)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}
	else if(kCFCompareEqualTo == CFStringCompare(pathExtension, CFSTR("mod"), kCFCompareCaseInsensitive)) {
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

		TagLib::Mod::File file(stream.get());
		if(file.isValid()) {
			fileIsValid = true;
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("MOD (Protracker)"));

			if(file.audioProperties())
				AddAudioPropertiesToDictionary(mMetadata, file.audioProperties());

			if(file.tag())
				AddTagToDictionary(mMetadata, file.tag());
		}
	}

	if(!fileIsValid) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}

bool SFB::Audio::MODMetadata::_WriteMetadata(CFErrorRef */*error*/)
{
	LOGGER_NOTICE("org.sbooth.AudioEngine.AudioMetadata.MOD", "Writing of MOD metadata is not supported");

	return false;
}
