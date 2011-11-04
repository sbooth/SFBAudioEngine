/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Stephen F. Booth <me@sbooth.org>
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

#include <wavpack/wavpack.h>

#include "WavPackMetadata.h"
#include "CreateDisplayNameForURL.h"
#include "Logger.h"

// ========================================
// WavPack comment utilities
// ========================================
static bool
SetWavPackTag(WavpackContext	*wpc,
			  const char		*key,
			  CFStringRef		value)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	// Remove the existing comment with this name
	WavpackDeleteTagItem(wpc, key);

	// Nothing left to do if value is NULL
	if(NULL == value)
		return true;
	
	CFIndex valueCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)  + 1;
	char valueCString [valueCStringSize];
	
	if(!CFStringGetCString(value, valueCString, valueCStringSize, kCFStringEncodingUTF8)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.WavPack", "CFStringGetCString() failed");
		return false;
	}
	
	if(!WavpackAppendTagItem(wpc, key, valueCString, static_cast<int>(strlen(valueCString)))) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.WavPack", "WavpackAppendTagItem() failed");
		return false;
	}
	
	return true;
}

static bool
SetWavPackTagNumber(WavpackContext		*wpc,
					const char			*key,
					CFNumberRef			value)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
												NULL, 
												CFSTR("%@"), 
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

	if(NULL == value)
		return SetWavPackTag(wpc, key, NULL);
	else if(CFBooleanGetValue(value))
		return SetWavPackTag(wpc, key, CFSTR("1"));
	else
		return SetWavPackTag(wpc, key, CFSTR("0"));
}

static bool
SetWavPackTagDouble(WavpackContext		*wpc,
					const char			*key,
					CFNumberRef			value,
					CFStringRef			format = NULL)
{
	assert(NULL != wpc);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value) {
		double f;
		if(!CFNumberGetValue(value, kCFNumberDoubleType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.WavPack", "CFNumberGetValue() failed");
			return false;
		}
		
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
												NULL, 
												NULL == format ? CFSTR("%f") : format, 
												f);
	}
	
	bool result = SetWavPackTag(wpc, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = NULL;
	
	return result;
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
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("wv"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool WavPackMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
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
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	

	char errorMsg [80];
	WavpackContext *wpc = WavpackOpenFileInput(reinterpret_cast<const char *>(buf), 
											   errorMsg, 
											   OPEN_TAGS, 
											   0);

	if(NULL == wpc) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a WavPack file"), ""));
			
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

	// Add the audio properties
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("WavPack"));

	uint32_t sampleRate = WavpackGetSampleRate(wpc);
	if(sampleRate) {
		CFNumberRef rate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &sampleRate);
		CFDictionarySetValue(mMetadata, kPropertiesSampleRateKey, rate);
		CFRelease(rate), rate = NULL;
	}

	uint32_t numSamples = WavpackGetNumSamples(wpc);
	if(numSamples) {
		CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &numSamples);
		CFDictionarySetValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
		CFRelease(totalFrames), totalFrames = NULL;		
	}
	
	if(sampleRate && numSamples) {
		double length = static_cast<double>(numSamples / sampleRate);
		CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &length);
		CFDictionarySetValue(mMetadata, kPropertiesDurationKey, duration);
		CFRelease(duration), duration = NULL;
	}
	
	int channels = WavpackGetNumChannels(wpc);
	if(channels) {
		CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &channels);
		CFDictionarySetValue(mMetadata, kPropertiesChannelsPerFrameKey, channelsPerFrame);
		CFRelease(channelsPerFrame), channelsPerFrame = NULL;
	}

	int bitsPerSample = WavpackGetBitsPerSample(wpc);
	if(bitsPerSample) {
		CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitsPerSample);
		CFDictionarySetValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
		CFRelease(bitsPerChannel), bitsPerChannel = NULL;
	}
	
	double averageBitrate = WavpackGetAverageBitrate(wpc, 1);
	if(averageBitrate) {
		averageBitrate /= 1000;
		CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &averageBitrate);
		CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
		CFRelease(bitrate), bitrate = NULL;
	}

	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																		  0,
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
			CFDictionarySetValue(mMetadata, kMetadataAlbumTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataAlbumArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataComposerKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataGenreKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("YEAR"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataReleaseDateKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataCommentKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACK"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataTrackNumberKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataTrackTotalKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataCompilationKey, CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataDiscNumberKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataDiscTotalKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("LYRICS"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataLyricsKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("BPM"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataBPMKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("RATING"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			CFDictionarySetValue(mMetadata, kMetadataRatingKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataISRCKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(mMetadata, kMetadataMCNKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			CFDictionarySetValue(mMetadata, kReplayGainReferenceLoudnessKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			CFDictionarySetValue(mMetadata, kReplayGainTrackGainKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			CFDictionarySetValue(mMetadata, kReplayGainTrackPeakKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			CFDictionarySetValue(mMetadata, kReplayGainAlbumGainKey, number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			CFDictionarySetValue(mMetadata, kReplayGainAlbumPeakKey, number);
			CFRelease(number), number = NULL;
		}
		// Put all unknown tags into the additional metadata
		else
			CFDictionarySetValue(additionalMetadata, key, value);
		
		CFRelease(key), key = NULL;
		CFRelease(value), value = NULL;
	}

	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(mMetadata, kMetadataAdditionalMetadataKey, additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = NULL;
	
	WavpackCloseFile(wpc), wpc = NULL;
	
	return true;
}

bool WavPackMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	
	char errorMsg [80];
	WavpackContext *wpc = WavpackOpenFileInput(reinterpret_cast<const char *>(buf), 
											   errorMsg, 
											   OPEN_EDIT_TAGS, 
											   0);
	
	if(NULL == wpc) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not a WavPack file"), ""));
			
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
	
	// Standard tags
	SetWavPackTag(wpc, "ALBUM", GetAlbumTitle());
	SetWavPackTag(wpc, "ARTIST", GetArtist());
	SetWavPackTag(wpc, "ALBUMARTIST", GetAlbumArtist());
	SetWavPackTag(wpc, "COMPOSER", GetComposer());
	SetWavPackTag(wpc, "GENRE", GetGenre());
	SetWavPackTag(wpc, "DATE", GetReleaseDate());
	SetWavPackTag(wpc, "DESCRIPTION", GetComment());
	SetWavPackTag(wpc, "TITLE", GetTitle());
	SetWavPackTagNumber(wpc, "TRACKNUMBER", GetTrackNumber());
	SetWavPackTagNumber(wpc, "TRACKTOTAL", GetTrackTotal());
	SetWavPackTagBoolean(wpc, "COMPILATION", GetCompilation());
	SetWavPackTagNumber(wpc, "DISCNUMBER", GetDiscNumber());
	SetWavPackTagNumber(wpc, "DISCTOTAL", GetDiscTotal());
	SetWavPackTag(wpc, "LYRICS", GetLyrics());
	SetWavPackTagNumber(wpc, "BPM", GetBPM());
	SetWavPackTagNumber(wpc, "RATING", GetRating());
	SetWavPackTag(wpc, "ISRC", GetISRC());
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
			
			if(!CFStringGetCString(reinterpret_cast<CFStringRef>(keys[i]), key, keySize + 1, kCFStringEncodingASCII)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.WavPack", "CFStringGetCString() failed");
				continue;
			}
			
			SetWavPackTag(wpc, key, reinterpret_cast<CFStringRef>(values[i]));
		}
	}
	
	// ReplayGain info
	SetWavPackTagDouble(wpc, "REPLAYGAIN_REFERENCE_LOUDNESS", GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetWavPackTagDouble(wpc, "REPLAYGAIN_TRACK_GAIN", GetReplayGainReferenceLoudness(), CFSTR("%+2.2f dB"));
	SetWavPackTagDouble(wpc, "REPLAYGAIN_TRACK_PEAK", GetReplayGainTrackGain(), CFSTR("%1.8f"));
	SetWavPackTagDouble(wpc, "REPLAYGAIN_ALBUM_GAIN", GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetWavPackTagDouble(wpc, "REPLAYGAIN_ALBUM_PEAK", GetReplayGainAlbumPeak(), CFSTR("%1.8f"));
	
	if(!WavpackWriteTag(wpc)) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""), 
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
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid WavPack file."), ""), 
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
