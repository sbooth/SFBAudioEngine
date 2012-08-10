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

#include <mp4v2/mp4v2.h>
#include <mp4v2/itmf_tags.h>
#include <mp4v2/itmf_generic.h>

#include "MP4Metadata.h"
#include "CFErrorUtilities.h"
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
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("m4a"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool MP4Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
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
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataFileFormatNotRecognizedError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// Read the properties
	if(0 < MP4GetNumberOfTracks(file)) {
		// Should be type 'soun', media data name'mp4a'
		MP4TrackId trackID = MP4FindTrackId(file, 0);

		// Verify this is an MPEG-4 audio file
		if(MP4_INVALID_TRACK_ID == trackID || strncmp("soun", MP4GetTrackType(file, trackID), 4)) {
			MP4Close(file), file = nullptr;
			
			if(error) {
				CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), "");
				CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), "");
				CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
				
				*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataFileFormatNotSupportedError, description, mURL, failureReason, recoverySuggestion);
				
				CFRelease(description), description = nullptr;
				CFRelease(failureReason), failureReason = nullptr;
				CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
			}
			
			return false;
		}
		
		MP4Duration mp4Duration = MP4GetTrackDuration(file, trackID);
		uint32_t mp4TimeScale = MP4GetTrackTimeScale(file, trackID);
		
		CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &mp4Duration);
		CFDictionarySetValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
		CFRelease(totalFrames), totalFrames = nullptr;
		
		CFNumberRef sampleRate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &mp4TimeScale);
		CFDictionarySetValue(mMetadata, kPropertiesSampleRateKey, sampleRate);
		CFRelease(sampleRate), sampleRate = nullptr;
		
		double length = static_cast<double>(mp4Duration / mp4TimeScale);
		CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &length);
		CFDictionarySetValue(mMetadata, kPropertiesDurationKey, duration);
		CFRelease(duration), duration = nullptr;

		// "mdia.minf.stbl.stsd.*[0].channels"
		int channels = MP4GetTrackAudioChannels(file, trackID);
		CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &channels);
		CFDictionaryAddValue(mMetadata, kPropertiesChannelsPerFrameKey, channelsPerFrame);
		CFRelease(channelsPerFrame), channelsPerFrame = nullptr;

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
				CFRelease(bitsPerChannel), bitsPerChannel = nullptr;

				double losslessBitrate = static_cast<double>(mp4TimeScale * channels * decoderConfig[9]) / 1000;
				CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
				CFRelease(bitrate), bitrate = nullptr;

				free(decoderConfig), decoderConfig = nullptr;
			}			
			else if(MP4GetTrackIntegerProperty(file, trackID, "mdia.minf.stbl.stsd.alac.sampleSize", &sampleSize)) {
				CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &sampleSize);
				CFDictionaryAddValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
				CFRelease(bitsPerChannel), bitsPerChannel = nullptr;

				double losslessBitrate = static_cast<double>(mp4TimeScale * channels * sampleSize) / 1000;
				CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
				CFRelease(bitrate), bitrate = nullptr;
			}
		}

		// AAC files
		if(MP4HaveTrackAtom(file, trackID, "mdia.minf.stbl.stsd.mp4a")) {
			CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("AAC"));

			// "mdia.minf.stbl.stsd.*.esds.decConfigDescr.avgBitrate"
			uint32_t trackBitrate = MP4GetTrackBitRate(file, trackID) / 1000;
			CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackBitrate);
			CFDictionaryAddValue(mMetadata, kPropertiesBitrateKey, bitrate);
			CFRelease(bitrate), bitrate = nullptr;
			
		}
	}
	// No valid tracks in file
	else {
		MP4Close(file), file = nullptr;
		
		if(error) {
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataFileFormatNotSupportedError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}

	// Read the tags
	const MP4Tags *tags = MP4TagsAlloc();

	if(nullptr == tags) {
		MP4Close(file), file = nullptr;

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}
	
	MP4TagsFetch(tags, file);
	
	// Album title
	if(tags->album) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->album, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataAlbumTitleKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Artist
	if(tags->artist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->artist, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataArtistKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Album Artist
	if(tags->albumArtist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->albumArtist, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataAlbumArtistKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Genre
	if(tags->genre) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->genre, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataGenreKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Release date
	if(tags->releaseDate) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->releaseDate, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataReleaseDateKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Composer
	if(tags->composer) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->composer, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataComposerKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Comment
	if(tags->comments) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->comments, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataCommentKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Track title
	if(tags->name) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->name, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataTitleKey, str);
			CFRelease(str), str = nullptr;
		}
	}
	
	// Track number
	if(tags->track) {
		if(tags->track->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->index);
			CFDictionarySetValue(mMetadata, kMetadataTrackNumberKey, num);
			CFRelease(num), num = nullptr;
		}
		
		if(tags->track->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->total);
			CFDictionarySetValue(mMetadata, kMetadataTrackTotalKey, num);
			CFRelease(num), num = nullptr;
		}
	}
	
	// Disc number
	if(tags->disk) {
		if(tags->disk->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->index);
			CFDictionarySetValue(mMetadata, kMetadataDiscNumberKey, num);
			CFRelease(num), num = nullptr;
		}
		
		if(tags->disk->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->total);
			CFDictionarySetValue(mMetadata, kMetadataDiscTotalKey, num);
			CFRelease(num), num = nullptr;
		}
	}
	
	// Compilation
	if(tags->compilation)
		CFDictionarySetValue(mMetadata, kMetadataCompilationKey, *(tags->compilation) ? kCFBooleanTrue : kCFBooleanFalse);
	
	// BPM
	if(tags->tempo) {
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->tempo);
		CFDictionarySetValue(mMetadata, kMetadataBPMKey, num);
		CFRelease(num), num = nullptr;
	}
	
	// Lyrics
	if(tags->lyrics) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->lyrics, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataLyricsKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->sortName) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->sortName, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataTitleSortOrderKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->sortAlbum) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->sortAlbum, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataAlbumTitleSortOrderKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->sortArtist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->sortArtist, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataArtistSortOrderKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->sortAlbumArtist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->sortAlbumArtist, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataAlbumArtistSortOrderKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->sortComposer) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->sortComposer, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataComposerSortOrderKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	if(tags->grouping) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->grouping, kCFStringEncodingUTF8);
		if(str) {
			CFDictionarySetValue(mMetadata, kMetadataGroupingKey, str);
			CFRelease(str), str = nullptr;
		}
	}

	// Album art
	if(tags->artworkCount) {
		for(uint32_t i = 0; i < tags->artworkCount; ++i) {
			CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(tags->artwork[i].data), tags->artwork[i].size);

			AttachedPicture *picture = new AttachedPicture(data);
			AddSavedPicture(picture);
			
			CFRelease(data), data = nullptr;
		}
	}
	
	// ReplayGain
	// Reference loudness
	MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
	if(nullptr != items) {
		float referenceLoudnessValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &referenceLoudnessValue)) {
			CFNumberRef referenceLoudness = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &referenceLoudnessValue);
			CFDictionaryAddValue(mMetadata, kReplayGainReferenceLoudnessKey, referenceLoudness);
			CFRelease(referenceLoudness), referenceLoudness = nullptr;
		}
		
		MP4ItmfItemListFree(items), items = nullptr;
	}
	
	// Track gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_gain");
	if(nullptr != items) {
		float trackGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &trackGainValue)) {
			CFNumberRef trackGain = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &trackGainValue);
			CFDictionaryAddValue(mMetadata, kReplayGainTrackGainKey, trackGain);
			CFRelease(trackGain), trackGain = nullptr;
		}
		
		MP4ItmfItemListFree(items), items = nullptr;
	}		
	
	// Track peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_peak");
	if(nullptr != items) {
		float trackPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &trackPeakValue)) {
			CFNumberRef trackPeak = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &trackPeakValue);
			CFDictionaryAddValue(mMetadata, kReplayGainTrackPeakKey, trackPeak);
			CFRelease(trackPeak), trackPeak = nullptr;
		}
		
		MP4ItmfItemListFree(items), items = nullptr;
	}		
	
	// Album gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_gain");
	if(nullptr != items) {
		float albumGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &albumGainValue)) {
			CFNumberRef albumGain = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &albumGainValue);
			CFDictionaryAddValue(mMetadata, kReplayGainAlbumGainKey, albumGain);
			CFRelease(albumGain), albumGain = nullptr;
		}
		
		MP4ItmfItemListFree(items), items = nullptr;
	}		
	
	// Album peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_peak");
	if(nullptr != items) {
		float albumPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf(reinterpret_cast<const char *>(items->elements[0].dataList.elements[0].value), "%f", &albumPeakValue)) {
			CFNumberRef albumPeak = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &albumPeakValue);
			CFDictionaryAddValue(mMetadata, kReplayGainAlbumPeakKey, albumPeak);
			CFRelease(albumPeak), albumPeak = nullptr;
		}
		
		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Clean up
	MP4TagsFree(tags), tags = nullptr;
	MP4Close(file), file = nullptr;

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
			CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), "");
			CFStringRef failureReason = CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), "");
			CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, mURL, failureReason, recoverySuggestion);
			
			CFRelease(description), description = nullptr;
			CFRelease(failureReason), failureReason = nullptr;
			CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
		}
		
		return false;
	}
	
	// Read the tags
	const MP4Tags *tags = MP4TagsAlloc();

	if(nullptr == tags) {
		MP4Close(file), file = nullptr;
		
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
	
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
		MP4TagsSetAlbum(tags, nullptr);

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
		MP4TagsSetArtist(tags, nullptr);
	
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
		MP4TagsSetAlbumArtist(tags, nullptr);

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
		MP4TagsSetGenre(tags, nullptr);
	
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
		MP4TagsSetReleaseDate(tags, nullptr);
	
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
		MP4TagsSetComposer(tags, nullptr);
	
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
		MP4TagsSetComments(tags, nullptr);
	
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
		MP4TagsSetName(tags, nullptr);

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
		MP4TagsSetCompilation(tags, nullptr);

	// BPM
	if(GetBPM()) {
		uint16_t BPM;
		CFNumberGetValue(GetBPM(), kCFNumberSInt16Type, &BPM);
		MP4TagsSetTempo(tags, &BPM);
	}
	else
		MP4TagsSetTempo(tags, nullptr);

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
		MP4TagsSetLyrics(tags, nullptr);

	// Title sort order
	str = GetTitleSortOrder();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetSortName(tags, cString);
	}
	else
		MP4TagsSetSortName(tags, nullptr);

	// Album title sort order
	str = GetAlbumTitleSortOrder();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetSortAlbum(tags, cString);
	}
	else
		MP4TagsSetSortAlbum(tags, nullptr);

	// Artist sort order
	str = GetArtistSortOrder();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetSortArtist(tags, cString);
	}
	else
		MP4TagsSetSortArtist(tags, nullptr);

	// Album artist sort order
	str = GetAlbumArtistSortOrder();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetSortAlbumArtist(tags, cString);
	}
	else
		MP4TagsSetSortAlbumArtist(tags, nullptr);

	// Composer sort order
	str = GetComposerSortOrder();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetSortComposer(tags, cString);
	}
	else
		MP4TagsSetSortComposer(tags, nullptr);

	// Grouping
	str = GetGrouping();

	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];

		if(!CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;			
		}

		MP4TagsSetGrouping(tags, cString);
	}
	else
		MP4TagsSetGrouping(tags, nullptr);

	// Remove existing front cover art
	for(uint32_t i = 0; i < tags->artworkCount; ++i)
		MP4TagsRemoveArtwork(tags, i);
	
	// Add album art
	for(auto attachedPicture : GetAttachedPictures()) {
		MP4TagArtwork artwork;
		CFDataRef data = attachedPicture->GetData();
		if(data) {
			artwork.data = reinterpret_cast<void *>(const_cast<UInt8 *>(CFDataGetBytePtr(data)));
			artwork.size = static_cast<uint32_t>(CFDataGetLength(data));
			artwork.type = MP4_ART_UNDEFINED;
			
			MP4TagsAddArtwork(tags, &artwork);
		}
	}

	// Save our changes
	MP4TagsStore(tags, file);
	MP4TagsFree(tags), tags = nullptr;

	// Replay Gain
	// Reference loudness
	MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	if(GetReplayGainReferenceLoudness()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainReferenceLoudness(), kCFNumberFloatType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue returned an approximation");

		char value [8];
		snprintf(value, sizeof(value), "%2.1f dB", f);

		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_reference_loudness");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Track gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_gain");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	if(GetReplayGainTrackGain()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainTrackGain(), kCFNumberFloatType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue returned an approximation");
		
		char value [10];
		snprintf(value, sizeof(value), "%+2.2f dB", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_track_gain");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Track peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_peak");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	if(GetReplayGainTrackPeak()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainTrackPeak(), kCFNumberFloatType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue returned an approximation");
		
		char value [12];
		snprintf(value, sizeof(value), "%1.8f", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_track_peak");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Album gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_gain");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	if(GetReplayGainAlbumGain()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainAlbumGain(), kCFNumberFloatType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue returned an approximation");
		
		char value [10];
		snprintf(value, sizeof(value), "%+2.2f dB", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_album_gain");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}
	
	// Album peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_peak");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	if(GetReplayGainAlbumPeak()) {
		float f;
		if(!CFNumberGetValue(GetReplayGainAlbumPeak(), kCFNumberFloatType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFNumberGetValue returned an approximation");
		
		char value [12];
		snprintf(value, sizeof(value), "%1.8f", f);
		
		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("replaygain_album_peak");
			
			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = reinterpret_cast<uint8_t *>(strdup(value));
			item->dataList.elements[0].valueSize = static_cast<uint32_t>(strlen(value));

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Clean up
	MP4Close(file), file = nullptr;

	MergeChangedMetadataIntoMetadata();
	
	return true;
}
