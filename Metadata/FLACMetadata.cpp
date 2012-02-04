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
#include "CreateDisplayNameForURL.h"
#include "AddID3v1TagToDictionary.h"
#include "AddID3v2TagToDictionary.h"
#include "AddXiphCommentToDictionary.h"
#include "SetXiphCommentFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "TagLibStringFromCFString.h"

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
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
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

	TagLib::IOStream *stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf), true);
	TagLib::FLAC::File file(stream, TagLib::ID3v2::FrameFactory::instance());

	if(!file.isValid()) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));

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

	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("FLAC"));

	if(file.audioProperties()) {
		TagLib::FLAC::Properties *properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);

		int sampleWidth = properties->sampleWidth();
		CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &sampleWidth);
		CFDictionaryAddValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
		CFRelease(bitsPerChannel), bitsPerChannel = NULL;

		unsigned long long sampleFrames = properties->sampleFrames();
		CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &sampleFrames);
		CFDictionaryAddValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
		CFRelease(totalFrames), totalFrames = NULL;
	}

	// Add all tags that are present
	if(file.ID3v1Tag())
		AddID3v1TagToDictionary(mMetadata, file.ID3v1Tag());

	if(file.ID3v2Tag())
		AddID3v2TagToDictionary(mMetadata, file.ID3v2Tag());

	if(file.xiphComment())
		AddXiphCommentToDictionary(mMetadata, file.xiphComment());

	// Add album art
	TagLib::List<TagLib::FLAC::Picture *> pictures = file.pictureList();
	for(TagLib::List<TagLib::FLAC::Picture *>::ConstIterator iter = pictures.begin(); iter != pictures.end(); ++iter) {
		TagLib::FLAC::Picture *picture = *iter;
		CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(picture->data().data()), picture->data().size());
		CFDictionarySetValue(mMetadata, kAlbumArtFrontCoverKey, data);
		CFRelease(data), data = NULL;
	}

	return true;
}

bool FLACMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	TagLib::IOStream *stream = new TagLib::FileStream(reinterpret_cast<const char *>(buf));
	TagLib::FLAC::File file(stream, TagLib::ID3v2::FrameFactory::instance(), false);

	if(!file.isValid()) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));

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

	SetXiphCommentFromMetadata(*this, file.xiphComment());

	if(GetFrontCoverArt()) {

#if 0
		// Remove existing front cover art
		TagLib::List<TagLib::FLAC::Picture *> pictures = file.pictureList();
		for(TagLib::List<TagLib::FLAC::Picture *>::ConstIterator iter = pictures.begin(); iter != pictures.end(); ++iter) {
			TagLib::FLAC::Picture *picture = *iter;
			if(TagLib::FLAC::Picture::Other == picture->type())
				file.removePicture(picture);
		}
#endif
		CFDataRef frontCoverData = GetFrontCoverArt();

		CGImageSourceRef imageSource = CGImageSourceCreateWithData(frontCoverData, NULL);
		if(NULL == imageSource)
			return false;

		TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture;
		picture->setData(TagLib::ByteVector((const char *)CFDataGetBytePtr(frontCoverData), (TagLib::uint)CFDataGetLength(frontCoverData)));

		// Convert the image's UTI into a MIME type
		CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
		if(mimeType) {
			picture->setMimeType(TagLib::StringFromCFString(mimeType));
			CFRelease(mimeType), mimeType = NULL;
		}

		// Flesh out the height, width, and depth
		CFDictionaryRef imagePropertiesDictionary = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, NULL);
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

			CFRelease(imagePropertiesDictionary), imagePropertiesDictionary = NULL;
		}

		file.addPicture(picture);
	}

	if(!file.save()) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
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
