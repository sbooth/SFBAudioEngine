/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for reading."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::RIFF::WAV::File file(stream.get());
	if(!file.isValid()) {
		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a WAVE file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
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
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for writing."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::RIFF::WAV::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a WAVE file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
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
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WAVE file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Unable to write metadata"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}
		
		return false;
	}

	return true;
}
