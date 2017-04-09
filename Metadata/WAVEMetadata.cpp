/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/wavfile.h>

#include "WAVEMetadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddTagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "SetTagFromMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterWAVEMetadata() __attribute__ ((constructor));
	void RegisterWAVEMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::WAVEMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::WAVEMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wave"), CFSTR("wav") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::WAVEMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wave") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::WAVEMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wave"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wav"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::WAVEMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wave"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::WAVEMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new WAVEMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::WAVEMetadata::WAVEMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::WAVEMetadata::_ReadMetadata(CFErrorRef *error)
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

	TagLib::RIFF::WAV::File file(stream.get());
	if(!file.isValid()) {
		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a WAVE file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("WAVE"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->sampleWidth());
		if(properties->sampleFrames())
			AddIntToDictionary(mMetadata, kTotalFramesKey, (int)properties->sampleFrames());
	}

	if(file.InfoTag())
		AddTagToDictionary(mMetadata, file.InfoTag());

	if(file.ID3v2Tag())
		AddID3v2TagToDictionary(mMetadata, mPictures, file.ID3v2Tag());

	return true;
}

bool SFB::Audio::WAVEMetadata::_WriteMetadata(CFErrorRef *error)
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

	TagLib::RIFF::WAV::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a WAVE file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
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
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
