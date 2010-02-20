/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>
#include <libkern/OSAtomic.h>
#include <stdexcept>
#include <typeinfo>

#include <wavpack/wavpack.h>

#include "AudioEngineDefines.h"
#include "WavPackMetadata.h"
#include "CreateDisplayNameForURL.h"


// ========================================
// Vorbis comment utilities
// ========================================
static bool
SetWavPackTag(WavpackContext	*wpc,
			  const char		*key,
			  CFStringRef		value)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	// Remove the existing comment with this name
	if(-1 == WavpackDeleteTagItem(wpc, key)) {
		ERR("WavpackDeleteTagItem failed");
		return false;
	}

	// Nothing left to do if value is NULL
	if(NULL == value)
		return true;
	
	CFIndex valueCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)  + 1;
	char valueCString [valueCStringSize];
	
	if(false == CFStringGetCString(value, valueCString, valueCStringSize, kCFStringEncodingUTF8)) {
		ERR("CFStringGetCString failed");
		return false;
	}
	
	if(-1 == WavpackAppendTagItem(wpc, key, valueCString, strlen(valueCString))) {
		ERR("WavpackAppendTagItem failed");
		return false;
	}
	
	return true;
}

static bool
SetWavPackTagNumber(WavpackContext		*wpc,
					const char			*key,
					CFNumberRef			value,
					CFStringRef			format = NULL)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
												NULL, 
												(NULL == format ? CFSTR("%@") : format), 
												value);
	
	bool result = SetWavPackTag(wpc, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = NULL;
	
	return result;
}

static bool
SetWavPackTagBoolean(WavpackContext		*wpc,
					 const char			*key,
					 CFBooleanRef		value)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	if(CFBooleanGetValue(value))
		return SetWavPackTag(wpc, key, CFSTR("1"));
	else
		return SetWavPackTag(wpc, key, CFSTR("0"));
}


#pragma mark Static Methods


CFArrayRef WavPackMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("wv") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef WavPackMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/wavpack") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool WavPackMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wv"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool WavPackMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/wavpack"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


WavPackMetadata::WavPackMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

WavPackMetadata::~WavPackMetadata()
{}


#pragma mark Functionality


bool WavPackMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	
	UInt8 buf [PATH_MAX];
	if(false == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	

	char errorMsg [80];
	WavpackContext *wpc = WavpackOpenFileInput(reinterpret_cast<const char *>(buf), 
											   errorMsg, 
											   OPEN_TAGS, 
											   0);

	if(NULL == wpc) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid WavPack file."), ""), 
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

	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																		  32,
																		  &kCFTypeDictionaryKeyCallBacks,
																		  &kCFTypeDictionaryValueCallBacks);
	for(int i = 0; i < WavpackGetNumTagItems(wpc); ++i) {

		// Get the tag's name
		int nameLen = WavpackGetTagItemIndexed(wpc, 
											   i, 
											   NULL, 
											   0);
		
		if(0 == nameLen)
			continue;
		
		char *nameBuf = static_cast<char *>(calloc(nameLen + 1, sizeof(char)));
		
		if(NULL == nameBuf)
			continue;
		
		nameLen = WavpackGetTagItemIndexed(wpc, 
										   i, 
										   nameBuf, 
										   nameLen + 1);

		// Get the tag's value
		int valueLen = WavpackGetTagItem(wpc, 
										 nameBuf, 
										 NULL, 
										 0);
		
		if(0 == valueLen) {
			free(nameBuf), nameBuf = NULL;
			continue;
		}

		char *valueBuf = static_cast<char *>(calloc(valueLen + 1, sizeof(char)));

		if(NULL == valueBuf) {
			free(nameBuf), nameBuf = NULL;
			continue;
		}
		
		valueLen = WavpackGetTagItem(wpc, 
									 nameBuf, 
									 valueBuf, 
									 valueLen + 1);
		
		// Create the CFString representations
		CFStringRef key = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
														reinterpret_cast<const UInt8 *>(nameBuf),
														nameLen, 
														kCFStringEncodingASCII,
														false,
														kCFAllocatorMalloc);

		CFStringRef value = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, 
														  reinterpret_cast<const UInt8 *>(valueBuf), 
														  valueLen, 
														  kCFStringEncodingUTF8, 
														  false, 
														  kCFAllocatorMalloc);
		
		if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUM"), kCFCompareCaseInsensitive))
			SetAlbumTitle(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
			SetArtist(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
			SetAlbumArtist(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
			SetComposer(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
			SetGenre(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("YEAR"), kCFCompareCaseInsensitive))
			SetReleaseDate(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
			SetComment(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
			SetTitle(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACK"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			SetTrackNumber(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			SetTrackTotal(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
			SetCompilation(CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			SetDiscNumber(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			SetDiscTotal(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
			SetISRC(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
			SetMCN(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			SetReplayGainReferenceLoudness(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			SetReplayGainTrackGain(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			SetReplayGainTrackPeak(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			SetReplayGainAlbumGain(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			SetReplayGainAlbumPeak(number);
			CFRelease(number), number = NULL;
		}
		// Put all unknown tags into the additional metadata
		else
			CFDictionarySetValue(additionalMetadata, key, value);
		
		CFRelease(key), key = NULL;
		CFRelease(value), value = NULL;
	}

	if(CFDictionaryGetCount(additionalMetadata))
		SetAdditionalMetadata(additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = NULL;
	
	WavpackCloseFile(wpc), wpc = NULL;
	
	CFShow(mMetadata);
	
	return true;
}

bool WavPackMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(false == CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	
	char errorMsg [80];
	WavpackContext *wpc = WavpackOpenFileInput(reinterpret_cast<const char *>(buf), 
											   errorMsg, 
											   OPEN_EDIT_TAGS, 
											   0);
	
	if(NULL == wpc) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid WavPack file."), ""), 
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
	
	// Album title
	SetWavPackTag(wpc, "ALBUM", GetAlbumTitle());
	
	// Artist
	SetWavPackTag(wpc, "ARTIST", GetArtist());
	
	// Album Artist
	SetWavPackTag(wpc, "ALBUMARTIST", GetAlbumArtist());
	
	// Composer
	SetWavPackTag(wpc, "COMPOSER", GetComposer());
	
	// Genre
	SetWavPackTag(wpc, "GENRE", GetGenre());
	
	// Date
	SetWavPackTag(wpc, "DATE", GetReleaseDate());
	
	// Comment
	SetWavPackTag(wpc, "DESCRIPTION", GetComment());
	
	// Track title
	SetWavPackTag(wpc, "TITLE", GetTitle());
	
	// Track number
	SetWavPackTagNumber(wpc, "TRACKNUMBER", GetTrackNumber());
	
	// Total tracks
	SetWavPackTagNumber(wpc, "TRACKTOTAL", GetTrackTotal());
	
	// Compilation
	SetWavPackTagBoolean(wpc, "COMPILATION", GetCompilation());
	
	// Disc number
	SetWavPackTagNumber(wpc, "DISCNUMBER", GetDiscNumber());
	
	// Disc total
	SetWavPackTagNumber(wpc, "DISCTOTAL", GetDiscTotal());
	
	// ISRC
	SetWavPackTag(wpc, "ISRC", GetISRC());
	
	// MCN
	SetWavPackTag(wpc, "MCN", GetMCN());
	
	// Additional metadata
	CFDictionaryRef additionalMetadata = GetAdditionalMetadata();
	if(NULL != additionalMetadata) {
		CFIndex count = CFDictionaryGetCount(additionalMetadata);
		
		const void * keys [count];
		const void * values [count];
		
		CFDictionaryGetKeysAndValues(additionalMetadata, 
									 reinterpret_cast<const void **>(keys), 
									 reinterpret_cast<const void **>(values));
		
		for(CFIndex i = 0; i < count; ++i) {
			CFIndex keySize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(reinterpret_cast<CFStringRef>(keys[i])), kCFStringEncodingASCII);
			char key [keySize + 1];
			
			if(false == CFStringGetCString(reinterpret_cast<CFStringRef>(keys[i]), key, keySize + 1, kCFStringEncodingASCII)) {
				ERR("CFStringGetCString failed");
				continue;
			}
			
			SetWavPackTag(wpc, key, reinterpret_cast<CFStringRef>(values[i]));
		}
	}
	
	// ReplayGain info
	SetWavPackTagNumber(wpc, "REPLAYGAIN_REFERENCE_LOUDNESS", GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetWavPackTagNumber(wpc, "REPLAYGAIN_TRACK_GAIN", GetReplayGainReferenceLoudness(), CFSTR("%+2.2f dB"));
	SetWavPackTagNumber(wpc, "REPLAYGAIN_TRACK_PEAK", GetReplayGainTrackGain(), CFSTR("%1.8f"));
	SetWavPackTagNumber(wpc, "REPLAYGAIN_ALBUM_GAIN", GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetWavPackTagNumber(wpc, "REPLAYGAIN_ALBUM_PEAK", GetReplayGainAlbumPeak(), CFSTR("%1.8f"));
	
	if(false == WavpackWriteTag(wpc)) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid WavPack file."), ""), 
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
	
	if(NULL != WavpackCloseFile(wpc)) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid WavPack file."), ""), 
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
	
	return true;
}
