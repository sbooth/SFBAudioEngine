/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <mp4v2/mp4v2.h>
#include <mp4v2/itmf_tags.h>
#include <mp4v2/itmf_generic.h>

#include "MP4Metadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

namespace {

	void RegisterMP4Metadata() __attribute__ ((constructor));
	void RegisterMP4Metadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::MP4Metadata>();
	}

#pragma mark Initialization

	static void DisableMP4v2Logging() __attribute__ ((constructor));
	static void DisableMP4v2Logging()
	{
		MP4LogSetLevel(MP4_LOG_NONE);
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MP4Metadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("m4a"), CFSTR("mp4") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MP4Metadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg-4") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MP4Metadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("m4a"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MP4Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg-4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::MP4Metadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new MP4Metadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::MP4Metadata::MP4Metadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::MP4Metadata::_ReadMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;

	// Open the file for reading
	MP4FileHandle file = MP4Read((const char *)buf);

	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::FileFormatNotRecognizedError, description, mURL, failureReason, recoverySuggestion);
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
				SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
				SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
				SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

				*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::FileFormatNotSupportedError, description, mURL, failureReason, recoverySuggestion);
			}

			return false;
		}

		MP4Duration mp4Duration = MP4GetTrackDuration(file, trackID);
		uint32_t mp4TimeScale = MP4GetTrackTimeScale(file, trackID);

		SFB::CFNumber totalFrames(kCFNumberLongLongType, &mp4Duration);
		CFDictionarySetValue(mMetadata, kTotalFramesKey, totalFrames);

		SFB::CFNumber sampleRate(kCFNumberIntType, &mp4TimeScale);
		CFDictionarySetValue(mMetadata, kSampleRateKey, sampleRate);

		double length = (double)mp4Duration / (double)mp4TimeScale;
		SFB::CFNumber duration(kCFNumberDoubleType, &length);
		CFDictionarySetValue(mMetadata, kDurationKey, duration);

		// "mdia.minf.stbl.stsd.*[0].channels"
		int channels = MP4GetTrackAudioChannels(file, trackID);
		SFB::CFNumber channelsPerFrame(kCFNumberIntType, &channels);
		CFDictionaryAddValue(mMetadata, kChannelsPerFrameKey, channelsPerFrame);

		// ALAC files
		if(MP4HaveTrackAtom(file, trackID, "mdia.minf.stbl.stsd.alac")) {
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("Apple Lossless"));

			uint64_t sampleSize;
			uint8_t *decoderConfig;
			uint32_t decoderConfigSize;
			if(MP4GetTrackBytesProperty(file, trackID, "mdia.minf.stbl.stsd.alac.alac.decoderConfig", &decoderConfig, &decoderConfigSize) && 28 <= decoderConfigSize) {
				// The ALAC magic cookie seems to have the following layout (28 bytes, BE):
				// Byte 10: Sample size
				// Bytes 25-28: Sample rate
				SFB::CFNumber bitsPerChannel(kCFNumberSInt8Type, decoderConfig + 9);
				CFDictionaryAddValue(mMetadata, kBitsPerChannelKey, bitsPerChannel);

				double losslessBitrate = (double)(mp4TimeScale * (unsigned int)channels * decoderConfig[9]) / 1000.0;
				SFB::CFNumber bitrate(kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kBitrateKey, bitrate);

				free(decoderConfig), decoderConfig = nullptr;
			}
			else if(MP4GetTrackIntegerProperty(file, trackID, "mdia.minf.stbl.stsd.alac.sampleSize", &sampleSize)) {
				SFB::CFNumber bitsPerChannel(kCFNumberLongLongType, &sampleSize);
				CFDictionaryAddValue(mMetadata, kBitsPerChannelKey, bitsPerChannel);

				double losslessBitrate = (double)(mp4TimeScale * (unsigned int)channels * sampleSize) / 1000.0;
				SFB::CFNumber bitrate(kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kBitrateKey, bitrate);
			}
		}

		// AAC files
		if(MP4HaveTrackAtom(file, trackID, "mdia.minf.stbl.stsd.mp4a")) {
			CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("AAC"));

			// "mdia.minf.stbl.stsd.*.esds.decConfigDescr.avgBitrate"
			uint32_t trackBitrate = MP4GetTrackBitRate(file, trackID) / 1000;
			SFB::CFNumber bitrate(kCFNumberIntType, &trackBitrate);
			CFDictionaryAddValue(mMetadata, kBitrateKey, bitrate);

		}
	}
	// No valid tracks in file
	else {
		MP4Close(file), file = nullptr;

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::FileFormatNotSupportedError, description, mURL, failureReason, recoverySuggestion);
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
		SFB::CFString str(tags->album, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kAlbumTitleKey, str);
	}

	// Artist
	if(tags->artist) {
		SFB::CFString str(tags->artist, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kArtistKey, str);
	}

	// Album Artist
	if(tags->albumArtist) {
		SFB::CFString str(tags->albumArtist, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kAlbumArtistKey, str);
	}

	// Genre
	if(tags->genre) {
		SFB::CFString str(tags->genre, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kGenreKey, str);
	}

	// Release date
	if(tags->releaseDate) {
		SFB::CFString str(tags->releaseDate, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kReleaseDateKey, str);
	}

	// Composer
	if(tags->composer) {
		SFB::CFString str(tags->composer, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kComposerKey, str);
	}

	// Comment
	if(tags->comments) {
		SFB::CFString str(tags->comments, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kCommentKey, str);
	}

	// Track title
	if(tags->name) {
		SFB::CFString str(tags->name, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kTitleKey, str);
	}

	// Track number
	if(tags->track) {
		if(tags->track->index) {
			SFB::CFNumber num(kCFNumberSInt16Type, &tags->track->index);
			CFDictionarySetValue(mMetadata, kTrackNumberKey, num);
		}

		if(tags->track->total) {
			SFB::CFNumber num(kCFNumberSInt16Type, &tags->track->total);
			CFDictionarySetValue(mMetadata, kTrackTotalKey, num);
		}
	}

	// Disc number
	if(tags->disk) {
		if(tags->disk->index) {
			SFB::CFNumber num(kCFNumberSInt16Type, &tags->disk->index);
			CFDictionarySetValue(mMetadata, kDiscNumberKey, num);
		}

		if(tags->disk->total) {
			SFB::CFNumber num(kCFNumberSInt16Type, &tags->disk->total);
			CFDictionarySetValue(mMetadata, kDiscTotalKey, num);
		}
	}

	// Compilation
	if(tags->compilation)
		CFDictionarySetValue(mMetadata, kCompilationKey, *(tags->compilation) ? kCFBooleanTrue : kCFBooleanFalse);

	// BPM
	if(tags->tempo) {
		SFB::CFNumber num(kCFNumberSInt16Type, &tags->tempo);
		CFDictionarySetValue(mMetadata, kBPMKey, num);
	}

	// Lyrics
	if(tags->lyrics) {
		SFB::CFString str(tags->lyrics, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kLyricsKey, str);
	}

	if(tags->sortName) {
		SFB::CFString str(tags->sortName, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kTitleSortOrderKey, str);
	}

	if(tags->sortAlbum) {
		SFB::CFString str(tags->sortAlbum, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kAlbumTitleSortOrderKey, str);
	}

	if(tags->sortArtist) {
		SFB::CFString str(tags->sortArtist, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kArtistSortOrderKey, str);
	}

	if(tags->sortAlbumArtist) {
		SFB::CFString str(tags->sortAlbumArtist, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kAlbumArtistSortOrderKey, str);
	}

	if(tags->sortComposer) {
		SFB::CFString str(tags->sortComposer, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kComposerSortOrderKey, str);
	}

	if(tags->grouping) {
		SFB::CFString str(tags->grouping, kCFStringEncodingUTF8);
		if(str)
			CFDictionarySetValue(mMetadata, kGroupingKey, str);
	}

	// Album art
	if(tags->artworkCount) {
		for(uint32_t i = 0; i < tags->artworkCount; ++i) {
			SFB::CFData data((const UInt8 *)tags->artwork[i].data, tags->artwork[i].size);

			mPictures.push_back(std::make_shared<AttachedPicture>(data));
		}
	}

	// MusicBrainz
	MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "MusicBrainz Album Id");
	if(nullptr != items) {
		if(1 <= items->size && 1 <= items->elements[0].dataList.size) {
			SFB::CFString releaseID((const char *)items->elements[0].dataList.elements[0].value, kCFStringEncodingUTF8);
			CFDictionaryAddValue(mMetadata, kMusicBrainzReleaseIDKey, releaseID);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "MusicBrainz Track Id");
	if(nullptr != items) {
		if(1 <= items->size && 1 <= items->elements[0].dataList.size) {
			SFB::CFString recordingID((const char *)items->elements[0].dataList.elements[0].value, kCFStringEncodingUTF8);
			CFDictionaryAddValue(mMetadata, kMusicBrainzRecordingIDKey, recordingID);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}


	// ReplayGain
	// Reference loudness
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
	if(nullptr != items) {
		float referenceLoudnessValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf((const char *)items->elements[0].dataList.elements[0].value, "%f", &referenceLoudnessValue)) {
			SFB::CFNumber referenceLoudness(kCFNumberFloatType, &referenceLoudnessValue);
			CFDictionaryAddValue(mMetadata, kReferenceLoudnessKey, referenceLoudness);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Track gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_gain");
	if(nullptr != items) {
		float trackGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf((const char *)items->elements[0].dataList.elements[0].value, "%f", &trackGainValue)) {
			SFB::CFNumber trackGain(kCFNumberFloatType, &trackGainValue);
			CFDictionaryAddValue(mMetadata, kTrackGainKey, trackGain);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Track peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_track_peak");
	if(nullptr != items) {
		float trackPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf((const char *)items->elements[0].dataList.elements[0].value, "%f", &trackPeakValue)) {
			SFB::CFNumber trackPeak(kCFNumberFloatType, &trackPeakValue);
			CFDictionaryAddValue(mMetadata, kTrackPeakKey, trackPeak);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Album gain
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_gain");
	if(nullptr != items) {
		float albumGainValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf((const char *)items->elements[0].dataList.elements[0].value, "%f", &albumGainValue)) {
			SFB::CFNumber albumGain(kCFNumberFloatType, &albumGainValue);
			CFDictionaryAddValue(mMetadata, kAlbumGainKey, albumGain);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Album peak
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_album_peak");
	if(nullptr != items) {
		float albumPeakValue;
		if(1 <= items->size && 1 <= items->elements[0].dataList.size && sscanf((const char *)items->elements[0].dataList.elements[0].value, "%f", &albumPeakValue)) {
			SFB::CFNumber albumPeak(kCFNumberFloatType, &albumPeakValue);
			CFDictionaryAddValue(mMetadata, kAlbumPeakKey, albumPeak);
		}

		MP4ItmfItemListFree(items), items = nullptr;
	}

	// Clean up
	MP4TagsFree(tags), tags = nullptr;
	MP4Close(file), file = nullptr;

	return true;
}

bool SFB::Audio::MP4Metadata::_WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	// Open the file for modification
	MP4FileHandle file = MP4Modify((const char *)buf);
	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MPEG-4 file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not an MPEG-4 file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
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
			artwork.data = (void *)CFDataGetBytePtr(data);
			artwork.size = (uint32_t)CFDataGetLength(data);
			artwork.type = MP4_ART_UNDEFINED;

			MP4TagsAddArtwork(tags, &artwork);
		}
	}

	// Save our changes
	MP4TagsStore(tags, file);
	MP4TagsFree(tags), tags = nullptr;

	// MusicBrainz
	MP4ItmfItemList *items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "MusicBrainz Album Id");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	CFStringRef musicBrainzReleaseID = GetMusicBrainzReleaseID();
	if(musicBrainzReleaseID) {
		CFIndex valueSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(musicBrainzReleaseID), kCFStringEncodingUTF8);
		char value [valueSize + 1];

		if(!CFStringGetCString(musicBrainzReleaseID, value, valueSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;
		}

		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("MusicBrainz Album Id");

			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "MusicBrainz Track Id");
	if(items) {
		for(uint32_t i = 0; i < items->size; ++i)
			MP4ItmfRemoveItem(file, items->elements + i);
		MP4ItmfItemListFree(items), items = nullptr;
	}

	CFStringRef musicBrainzRecordingID = GetMusicBrainzRecordingID();
	if(musicBrainzRecordingID) {
		CFIndex valueSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(musicBrainzRecordingID), kCFStringEncodingUTF8);
		char value [valueSize + 1];

		if(!CFStringGetCString(musicBrainzRecordingID, value, valueSize + 1, kCFStringEncodingUTF8)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "CFStringGetCString() failed");
			return false;
		}

		MP4ItmfItem *item = MP4ItmfItemAlloc("----", 1);
		if(nullptr != item) {
			item->mean = strdup("com.apple.iTunes");
			item->name = strdup("MusicBrainz Track Id");

			item->dataList.elements[0].typeCode = MP4_ITMF_BT_UTF8;
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Replay Gain
	// Reference loudness
	items = MP4ItmfGetItemsByMeaning(file, "com.apple.iTunes", "replaygain_reference_loudness");
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
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

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
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

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
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

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
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

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
			item->dataList.elements[0].value = (uint8_t *)strdup(value);
			item->dataList.elements[0].valueSize = (uint32_t)strlen(value);

			if(!MP4ItmfAddItem(file, item)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.MP4", "MP4ItmfAddItem() failed");
				return false;
			}
		}
	}

	// Clean up
	MP4Close(file), file = nullptr;

	return true;
}
