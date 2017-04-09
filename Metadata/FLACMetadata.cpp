/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/flacfile.h>
#include <taglib/flacproperties.h>
#include <taglib/id3v2framefactory.h>

#include <ApplicationServices/ApplicationServices.h>

#include "FLACMetadata.h"
#include "Logger.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddID3v1TagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "AddXiphCommentToDictionary.h"
#include "SetID3v1TagFromMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "SetXiphCommentFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterFLACMetadata() __attribute__ ((constructor));
	void RegisterFLACMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::FLACMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::FLACMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("flac") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::FLACMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/flac") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::FLACMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::FLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::FLACMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new FLACMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::FLACMetadata::FLACMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::FLACMetadata::_ReadMetadata(CFErrorRef *error)
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

	TagLib::FLAC::File file(stream.get(), TagLib::ID3v2::FrameFactory::instance());
	if(!file.isValid()) {
		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("FLAC"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->sampleWidth());
		if(properties->sampleFrames())
			AddLongLongToDictionary(mMetadata, kTotalFramesKey, (long)properties->sampleFrames());
	}

	// Add all tags that are present
	if(file.ID3v1Tag())
		AddID3v1TagToDictionary(mMetadata, file.ID3v1Tag());

	if(file.ID3v2Tag())
		AddID3v2TagToDictionary(mMetadata, mPictures, file.ID3v2Tag());

	if(file.xiphComment())
		AddXiphCommentToDictionary(mMetadata, mPictures, file.xiphComment());

	// Add album art
	for(auto iter : file.pictureList()) {
		SFB::CFData data((const UInt8 *)iter->data().data(), (CFIndex)iter->data().size());

		SFB::CFString description;
		if(!iter->description().isEmpty())
			description = CFString(iter->description().toCString(true), kCFStringEncodingUTF8);

		mPictures.push_back(std::make_shared<AttachedPicture>(data, (AttachedPicture::Type)iter->type(), description));
	}

	return true;
}

bool SFB::Audio::FLACMetadata::_WriteMetadata(CFErrorRef *error)
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

	TagLib::FLAC::File file(stream.get(), false, TagLib::AudioProperties::Average, TagLib::ID3v2::FrameFactory::instance());
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	// ID3v1 and ID3v2 tags are only written if present, but a Xiph comment is always written

	if(file.ID3v1Tag())
		SetID3v1TagFromMetadata(*this, file.ID3v1Tag());

	if(file.ID3v2Tag())
		SetID3v2TagFromMetadata(*this, file.ID3v2Tag());

	SetXiphCommentFromMetadata(*this, file.xiphComment(true), false);

	// Remove existing cover art
	file.removePictures();

	// Add album art
	for(auto attachedPicture : GetAttachedPictures()) {

		SFB::CGImageSource imageSource(CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr));
		if(!imageSource) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioMetadata.FLAC", "Skipping album art (unable to create image)");
			continue;
		}

		TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture;
		picture->setData(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (size_t)CFDataGetLength(attachedPicture->GetData())));
		picture->setType((TagLib::FLAC::Picture::Type)attachedPicture->GetType());
		if(attachedPicture->GetDescription())
			picture->setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));

		// Convert the image's UTI into a MIME type
		SFB::CFString mimeType(UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType));
		if(mimeType)
			picture->setMimeType(TagLib::StringFromCFString(mimeType));

		// Flesh out the height, width, and depth
		SFB::CFDictionary imagePropertiesDictionary(CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr));
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
		}

		file.addPicture(picture);
	}

	if(!file.save()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	return true;
}
