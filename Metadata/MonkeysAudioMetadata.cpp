/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/apefile.h>
#include <taglib/tag.h>

#include "MonkeysAudioMetadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddID3v1TagToDictionary.h"
#include "AddAPETagToDictionary.h"
#include "SetID3v1TagFromMetadata.h"
#include "SetAPETagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterMonkeysAudioMetadata() __attribute__ ((constructor));
	void RegisterMonkeysAudioMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::MonkeysAudioMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MonkeysAudioMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ape") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MonkeysAudioMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/monkeys-audio") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MonkeysAudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ape"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MonkeysAudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/monkeys-audio"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::MonkeysAudioMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new MonkeysAudioMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::MonkeysAudioMetadata::MonkeysAudioMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::MonkeysAudioMetadata::_ReadMetadata(CFErrorRef *error)
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

	TagLib::APE::File file(stream.get());
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("Monkey's Audio"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->bitsPerSample())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->bitsPerSample());
		if(properties->sampleFrames())
			AddIntToDictionary(mMetadata, kTotalFramesKey, (int)properties->sampleFrames());
	}

	if(file.ID3v1Tag())
		AddID3v1TagToDictionary(mMetadata, file.ID3v1Tag());

	if(file.APETag())
		AddAPETagToDictionary(mMetadata, mPictures, file.APETag());

	return true;
}

bool SFB::Audio::MonkeysAudioMetadata::_WriteMetadata(CFErrorRef *error)
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

	TagLib::APE::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a Monkey's Audio file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	// ID3v1 tags are only written if present, but an APE tag is always written

	if(file.ID3v1Tag())
		SetID3v1TagFromMetadata(*this, file.ID3v1Tag());

	SetAPETagFromMetadata(*this, file.APETag(true));

	if(!file.save()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Monkey's Audio file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
