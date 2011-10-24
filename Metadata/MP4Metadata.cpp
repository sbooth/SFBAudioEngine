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

#include <mp4v2/mp4v2.h>
#include <mp4v2/itmf_tags.h>
#include <mp4v2/itmf_generic.h>

#include "MP4Metadata.h"
#include "CreateDisplayNameForURL.h"
#include "Logger.h"

#pragma mark Initialization

static void DisableMP4v2Logging() __attribute__ ((constructor));
static void DisableMP4v2Logging()
{
	MP4LogSetLevel(MP4_LOG_NONE);
}

#pragma mark Static Methods

CFArrayRef MP4Metadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("m4a"), CFSTR("mp4") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 2, &kCFTypeArrayCallBacks);
}

CFArrayRef MP4Metadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg-4") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MP4Metadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("m4a"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool MP4Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg-4"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

MP4Metadata::MP4Metadata(CFURLRef url)
	: AudioMetadata(url)
{}

MP4Metadata::~MP4Metadata()
{}

#pragma mark Functionality

bool MP4Metadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);

	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	
	// Open the file for reading
	MP4FileHandle file = MP4Read(reinterpret_cast<const char *>(buf));
	
	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataFileFormatNotRecognizedError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		return false;
	}

	// Read the properties
	if(0 < MP4GetNumberOfTracks(file)) {
		// Should be type 'soun', media data name'mp4a'
		MP4TrackId trackID = MP4FindTrackId(file, 0);

		// Verify this is an MPEG-4 audio file
		if(MP4_INVALID_TRACK_ID == trackID || strncmp("soun", MP4GetTrackType(file, trackID), 4)) {
			MP4Close(file), file = NULL;
			
			if(error) {
				CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																				   0,
																				   &kCFTypeDictionaryKeyCallBacks,
																				   &kCFTypeDictionaryValueCallBacks);
				
				CFStringRef displayName = CreateDisplayNameForURL(mURL);
				CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																   NULL, 
																   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""), 
																   displayName);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedDescriptionKey, 
									 errorString);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedFailureReasonKey, 
									 CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedRecoverySuggestionKey, 
									 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
				
				CFRelease(errorString), errorString = NULL;
				CFRelease(displayName), displayName = NULL;
				
				*error = CFErrorCreate(kCFAllocatorDefault, 
									   AudioMetadataErrorDomain, 
									   AudioMetadataFileFormatNotSupportedError, 
									   errorDictionary);
				
				CFRelease(errorDictionary), errorDictionary = NULL;				
			}
			
			return false;
		}
		
		MP4Duration mp4Duration = MP4GetTrackDuration(file, trackID);
		uint32_t mp4TimeScale = MP4GetTrackTimeScale(file, trackID);
		
		CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &mp4Duration);
		CFDictionarySetValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
		CFRelease(totalFrames), totalFrames = NULL;
		
		CFNumberRef sampleRate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &mp4TimeScale);
		CFDictionarySetValue(mMetadata, kPropertiesSampleRateKey, sampleRate);
		CFRelease(sampleRate), sampleRate = NULL;
		
		double length = static_cast<double>(mp4Duration / mp4TimeScale);
		CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &length);
		CFDictionarySetValue(mMetadata, kPropertiesDurationKey, duration);
		CFRelease(duration), duration = NULL;

		// "mdia.minf.stbl.stsd.*[0].channels"
		int channels = MP4GetTrackAudioChannels(file, trackID);
		CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &channels);
		CFDictionaryAddValue(mMetadata, kPropertiesChannelsPerFrameKey, channelsPerFrame);
		CFRelease(channelsPerFrame), channelsPerFrame = NULL;

		// ALAC files
		if(MP4HaveTrackAtom(file, trackID, "mdia.minf.stbl.stsd.alac")) {
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("Apple Lossless"));
			
			uint64_t sampleSize;
			uint8_t *decoderConfig;
			uint32_t decoderConfigSize;
			if(MP4GetTrackBytesProperty(file, trackID, "mdia.minf.stbl.stsd.alac.alac.decoderConfig", &decoderConfig, &decoderConfigSize) && 28 <= decoderConfigSize) {
				// The ALAC magic cookie seems to have the following layout (28 bytes, BE):
				// Byte 10: Sample size
				// Bytes 25-28: Sample rate
				CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type, decoderConfig + 9);
				CFDictionaryAddValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
				CFRelease(bitsPerChannel), bitsPerChannel = NULL;

				double losslessBitrate = static_cast<double>(mp4TimeScale * channels * sampleSize) / 1000;
				CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
				CFRelease(bitrate), bitrate = NULL;

				free(decoderConfig), decoderConfig = NULL;
			}			
			else if(MP4GetTrackIntegerProperty(file, trackID, "mdia.minf.stbl.stsd.alac.sampleSize", &sampleSize)) {
				CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &sampleSize);
				CFDictionaryAddValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
				CFRelease(bitsPerChannel), bitsPerChannel = NULL;

				double losslessBitrate = static_cast<double>(mp4TimeScale * channels * sampleSize) / 1000;
				CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
				CFRelease(bitrate), bitrate = NULL;
			}
		}

		// AAC files
		if(MP4HaveTrackAtom(file, trackID, "mdia.minf.stbl.stsd.mp4a")) {
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("AAC"));

			// "mdia.minf.stbl.stsd.*.esds.decConfigDescr.avgBitrate"
			uint32_t trackBitrate = MP4GetTrackBitRate(file, trackID) / 1000;
			CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackBitrate);
			CFDictionaryAddValue(mMetadata, kPropertiesBitrateKey, bitrate);
			CFRelease(bitrate), bitrate = NULL;
			
		}
	}
	// No valid tracks in file
	else {
		MP4Close(file), file = NULL;
		
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataFileFormatNotSupportedError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		return false;
	}

	// Read the tags
	const MP4Tags *tags = MP4TagsAlloc();

	if(NULL == tags) {
		MP4Close(file), file = NULL;

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);

		return false;
	}
	
	MP4TagsFetch(tags, file);
	
	// Album title
	if(tags->album) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->album, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataAlbumTitleKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Artist
	if(tags->artist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->artist, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataArtistKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Album Artist
	if(tags->albumArtist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->albumArtist, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataAlbumArtistKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Genre
	if(tags->genre) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->genre, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataGenreKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Release date
	if(tags->releaseDate) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->releaseDate, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataReleaseDateKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Composer
	if(tags->composer) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->composer, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataComposerKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Comment
	if(tags->comments) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->comments, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataCommentKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Track title
	if(tags->name) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->name, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataTitleKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Track number
	if(tags->track) {
		if(tags->track->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->index);
			CFDictionarySetValue(mMetadata, kMetadataTrackNumberKey, num);
			CFRelease(num), num = NULL;
		}
		
		if(tags->track->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->total);
			CFDictionarySetValue(mMetadata, kMetadataTrackTotalKey, num);
			CFRelease(num), num = NULL;
		}
	}
	
	// Disc number
	if(tags->disk) {
		if(tags->disk->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->index);
			CFDictionarySetValue(mMetadata, kMetadataDiscNumberKey, num);
			CFRelease(num), num = NULL;
		}
		
		if(tags->disk->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->total);
			CFDictionarySetValue(mMetadata, kMetadataDiscTotalKey, num);
			CFRelease(num), num = NULL;
		}
	}
	
	// Compilation
	if(tags->compilation)
		CFDictionarySetValue(mMetadata, kMetadataCompilationKey, *(tags->compilation) ? kCFBooleanTrue : kCFBooleanFalse);
	
	// BPM
	if(tags->tempo) {
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->tempo);
		CFDictionarySetValue(mMetadata, kMetadataBPMKey, num);
		CFRelease(num), num = NULL;
	}
	
	// Lyrics
	if(tags->lyrics) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->lyrics, kCFStringEncodingUTF8);
		CFDictionarySetValue(mMetadata, kMetadataLyricsKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Album art
	if(tags->artworkCount) {
		for(uint32_t i = 0; i < tags->artworkCount; ++i) {
			CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(tags->artwork[i].data), tags->artwork[i].size);
			CFDictionarySetValue(mMetadata, kAlbumArtFrontCoverKey, data);
			CFRelease(data), data = NULL;
		}
	}
	
	// ReplayGain
	// Reference loudness
	MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
	if(NULL != items) {
		float referenceLoudnessValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &referenceLoudnessValue)) {
			CFNumberRef referenceLoudness = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &referenceLoudnessValue);
			CFDictionaryAddValue(mMetadata, kReplayGainReferenceLoudnessKey, referenceLoudness);
			CFRelease(referenceLoudness), referenceLoudness = NULL;
		}
		
		MP4ItmfItemListFree(items), items = NULL;
	}
	
	// Track gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_gain");
	if(NULL != items) {
		float trackGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &trackGainValue)) {
			CFNumberRef trackGain = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &trackGainValue);
			CFDictionaryAddValue(mMetadata, kReplayGainTrackGainKey, trackGain);
			CFRelease(trackGain), trackGain = NULL;
		}
		
		MP4ItmfItemListFree(items), items = NULL;
	}		
	
	// Track peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_peak");
	if(NULL != items) {
		float trackPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &trackPeakValue)) {
			CFNumberRef trackPeak = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &trackPeakValue);
			CFDictionaryAddValue(mMetadata, kReplayGainTrackPeakKey, trackPeak);
			CFRelease(trackPeak), trackPeak = NULL;
		}
		
		MP4ItmfItemListFree(items), items = NULL;
	}		
	
	// Album gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_gain");
	if(NULL != items) {
		float albumGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &albumGainValue)) {
			CFNumberRef albumGain = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &albumGainValue);
			CFDictionaryAddValue(mMetadata, kReplayGainAlbumGainKey, albumGain);
			CFRelease(albumGain), albumGain = NULL;
		}
		
		MP4ItmfItemListFree(items), items = NULL;
	}		
	
	// Album peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_peak");
	if(NULL != items) {
		float albumPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &albumPeakValue)) {
			CFNumberRef albumPeak = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &albumPeakValue);
			CFDictionaryAddValue(mMetadata, kReplayGainAlbumPeakKey, albumPeak);
			CFRelease(albumPeak), albumPeak = NULL;
		}
		
		MP4ItmfItemListFree(items), items = NULL;
	}

	// Clean up
	MP4TagsFree(tags), tags = NULL;
	MP4Close(file), file = NULL;

	return true;
}

bool MP4Metadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	// Open the file for modification
	MP4FileHandle file = MP4Modify(reinterpret_cast<const char *>(buf));
	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not an MPEG file"), ""));
			
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
	
	// Read the tags
	const MP4Tags *tags = MP4TagsAlloc();

	if(NULL == tags) {
		MP4Close(file), file = NULL;
		
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
	
		return false;
	}
	
	MP4TagsFetch(tags, file);
	
	// Album Title
	CFStringRef str = GetAlbumTitle();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetAlbum(tags, cString);
	}
	else
		MP4TagsSetAlbum(tags, NULL);

	// Artist
	str = GetArtist();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetArtist(tags, cString);
	}
	else
		MP4TagsSetArtist(tags, NULL);
	
	// Album Artist
	str = GetAlbumArtist();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetAlbumArtist(tags, cString);
	}
	else
		MP4TagsSetAlbumArtist(tags, NULL);

	// Genre
	str = GetGenre();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetGenre(tags, cString);
	}
	else
		MP4TagsSetGenre(tags, NULL);
	
	// Release date
	str = GetReleaseDate();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetReleaseDate(tags, cString);
	}
	else
		MP4TagsSetReleaseDate(tags, NULL);
	
	// Composer
	str = GetComposer();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetComposer(tags, cString);
	}
	else
		MP4TagsSetComposer(tags, NULL);
	
	// Comment
	str = GetComment();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetComments(tags, cString);
	}
	else
		MP4TagsSetComments(tags, NULL);
	
	// Track title
	str = GetTitle();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetName(tags, cString);
	}
	else
		MP4TagsSetName(tags, NULL);

	// Track number and total
	MP4TagTrack trackInfo;
	memset(&trackInfo, 0, sizeof(MP4TagTrack));

	if(GetTrackNumber())
		CFNumberGetValue(GetTrackNumber(), kCFNumberSInt32Type, &trackInfo.index);

	if(GetTrackTotal())
		CFNumberGetValue(GetTrackTotal(), kCFNumberSInt32Type, &trackInfo.total);
	
	MP4TagsSetTrack(tags, &trackInfo);

	// Disc number and total
	MP4TagDisk discInfo;
	memset(&discInfo, 0, sizeof(MP4TagDisk));
		
	if(GetDiscNumber())
		CFNumberGetValue(GetDiscNumber(), kCFNumberSInt32Type, &discInfo.index);
	
	if(GetDiscTotal())
		CFNumberGetValue(GetDiscTotal(), kCFNumberSInt32Type, &discInfo.total);
	
	MP4TagsSetDisk(tags, &discInfo);

	// Compilation
	if(GetCompilation()) {
		uint8_t comp = CFBooleanGetValue(GetCompilation());
		MP4TagsSetCompilation(tags, &comp);
	}
	else
		MP4TagsSetCompilation(tags, NULL);

	// BPM
	if(GetBPM()) {
		uint16_t BPM;
		CFNumberGetValue(GetBPM(), kCFNumberSInt16Type, &BPM);
		MP4TagsSetTempo(tags, &BPM);
	}
	else
		MP4TagsSetTempo(tags, NULL);

	// Lyrics
	str = GetLyrics();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}
		
		MP4TagsSetLyrics(tags, cString);
	}
	else
		MP4TagsSetLyrics(tags, NULL);

	// Album art
	CFDataRef artData = GetFrontCoverArt();
	
	if(artData) {		
		MP4TagArtwork artwork;
		
		artwork.data = reinterpret_cast<void *>(const_cast<UInt8 *>(CFDataGetBytePtr(artData)));
		artwork.size = static_cast<uint32_t>(CFDataGetLength(artData));
		artwork.type = MP4_ART_UNDEFINED;
		
		MP4TagsAddArtwork(tags, &artwork);
	}
	
	// Save our changes
	MP4TagsStore(tags, file);
	
	// Replay Gain
	// Reference loudness
	if(GetReplayGainReferenceLoudness()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainReferenceLoudness(), kCFNumberFloatType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue() failed");
			return false;
		}

		char value [8];
		snprintf(value, sizeof(value), "%2.1f dB", f);

		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(NULL != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_reference_loudness");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));
		}
	}
	else {
		MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
		if(items) {
			for(uint32_t i = 0; i < items->size; ++i)
				MP4ItmfRemoveItem(file, items->elements + i);
		}
		MP4ItmfItemListFree(items), items = NULL;
	}

	// Track gain
	if(GetReplayGainTrackGain()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainTrackGain(), kCFNumberFloatType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue() failed");
			return false;
		}
		
		char value [10];
		snprintf(value, sizeof(value), "%+2.2f dB", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(NULL != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_track_gain");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));
		}
	}
	else {
		MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_gain");
		if(items) {
			for(uint32_t i = 0; i < items->size; ++i)
				MP4ItmfRemoveItem(file, items->elements + i);
		}
		MP4ItmfItemListFree(items), items = NULL;
	}

	// Track peak
	if(GetReplayGainTrackPeak()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainTrackPeak(), kCFNumberFloatType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue() failed");
			return false;
		}
		
		char value [12];
		snprintf(value, sizeof(value), "%1.8f", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(NULL != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_track_peak");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));
		}
	}
	else {
		MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_peak");
		if(items) {
			for(uint32_t i = 0; i < items->size; ++i)
				MP4ItmfRemoveItem(file, items->elements + i);
		}
		MP4ItmfItemListFree(items), items = NULL;
	}

	// Album gain
	if(GetReplayGainAlbumGain()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainAlbumGain(), kCFNumberFloatType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue() failed");
			return false;
		}
		
		char value [10];
		snprintf(value, sizeof(value), "%+2.2f dB", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(NULL != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_album_gain");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));
		}
	}
	else {
		MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_gain");
		if(items) {
			for(uint32_t i = 0; i < items->size; ++i)
				MP4ItmfRemoveItem(file, items->elements + i);
		}
		MP4ItmfItemListFree(items), items = NULL;
	}
	
	// Album peak
	if(GetReplayGainAlbumPeak()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainAlbumPeak(), kCFNumberFloatType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue() failed");
			return false;
		}
		
		char value [12];
		snprintf(value, sizeof(value), "%1.8f", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(NULL != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_album_peak");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));
		}
	}
	else {
		MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_peak");
		if(items) {
			for(uint32_t i = 0; i < items->size; ++i)
				MP4ItmfRemoveItem(file, items->elements + i);
		}
		MP4ItmfItemListFree(items), items = NULL;
	}

	// Clean up
	MP4TagsFree(tags), tags = NULL;
	MP4Close(file), file = NULL;

	MergeChangedMetadataIntoMetadata();
	
	return true;
}
