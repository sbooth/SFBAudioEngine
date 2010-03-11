/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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

#include "AudioEngineDefines.h"
#include "MP4Metadata.h"
#include "CreateDisplayNameForURL.h"


// ========================================
// WavPack comment utilities
// ========================================
//static bool
//SetWavPackTag(WavpackContext	*wpc,
//			  const char		*key,
//			  CFStringRef		value)
//{
//	assert(NULL != wpc);
//	assert(NULL != key);
//	
//	// Remove the existing comment with this name
//	WavpackDeleteTagItem(wpc, key);
//
//	// Nothing left to do if value is NULL
//	if(NULL == value)
//		return true;
//	
//	CFIndex valueCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)  + 1;
//	char valueCString [valueCStringSize];
//	
//	if(false == CFStringGetCString(value, valueCString, valueCStringSize, kCFStringEncodingUTF8)) {
//		ERR("CFStringGetCString failed");
//		return false;
//	}
//	
//	if(false == WavpackAppendTagItem(wpc, key, valueCString, static_cast<int>(strlen(valueCString)))) {
//		ERR("WavpackAppendTagItem failed");
//		return false;
//	}
//	
//	return true;
//}
//
//static bool
//SetWavPackTagNumber(WavpackContext		*wpc,
//					const char			*key,
//					CFNumberRef			value)
//{
//	assert(NULL != wpc);
//	assert(NULL != key);
//	
//	CFStringRef numberString = NULL;
//	
//	if(NULL != value)
//		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
//												NULL, 
//												CFSTR("%@"), 
//												value);
//	
//	bool result = SetWavPackTag(wpc, key, numberString);
//	
//	if(numberString)
//		CFRelease(numberString), numberString = NULL;
//	
//	return result;
//}
//
//static bool
//SetWavPackTagBoolean(WavpackContext		*wpc,
//					 const char			*key,
//					 CFBooleanRef		value)
//{
//	assert(NULL != wpc);
//	assert(NULL != key);
//	
//	if(CFBooleanGetValue(value))
//		return SetWavPackTag(wpc, key, CFSTR("1"));
//	else
//		return SetWavPackTag(wpc, key, CFSTR("0"));
//}
//
//static bool
//SetWavPackTagDouble(WavpackContext		*wpc,
//					const char			*key,
//					CFNumberRef			value,
//					CFStringRef			format = NULL)
//{
//	assert(NULL != wpc);
//	assert(NULL != key);
//	
//	CFStringRef numberString = NULL;
//	
//	if(NULL != value) {
//		double f;
//		if(false == CFNumberGetValue(value, kCFNumberDoubleType, &f)) {
//			ERR("CFNumberGetValue failed");
//			return false;
//		}
//		
//		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
//												NULL, 
//												NULL == format ? CFSTR("%f") : format, 
//												f);
//	}
//	
//	bool result = SetWavPackTag(wpc, key, numberString);
//	
//	if(numberString)
//		CFRelease(numberString), numberString = NULL;
//	
//	return result;
//}


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
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("m4a"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp4"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool MP4Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
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
	
	UInt8 buf [PATH_MAX];
	if(false == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		return false;
	
	// Open the file for reading
	MP4FileHandle file = MP4Read(reinterpret_cast<const char *>(buf), 0);
	
	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
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
	
	// Album title
	if(tags->album) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->album, kCFStringEncodingUTF8);
		SetAlbumTitle(str);
		CFRelease(str), str = NULL;
	}
	
	// Artist
	if(tags->artist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->artist, kCFStringEncodingUTF8);
		SetArtist(str);
		CFRelease(str), str = NULL;
	}
	
	// Album Artist
	if(tags->albumArtist) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->albumArtist, kCFStringEncodingUTF8);
		SetAlbumArtist(str);
		CFRelease(str), str = NULL;
	}
	
	// Genre
	if(tags->genre) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->genre, kCFStringEncodingUTF8);
		SetGenre(str);
		CFRelease(str), str = NULL;
	}
	
	// Release date
	if(tags->releaseDate) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->releaseDate, kCFStringEncodingUTF8);
		SetReleaseDate(str);
		CFRelease(str), str = NULL;
	}
	
	// Composer
	if(tags->composer) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->composer, kCFStringEncodingUTF8);
		SetComposer(str);
		CFRelease(str), str = NULL;
	}
	
	// Comment
	if(tags->comments) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->comments, kCFStringEncodingUTF8);
		SetComment(str);
		CFRelease(str), str = NULL;
	}
	
	// Track title
	if(tags->name) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->name, kCFStringEncodingUTF8);
		SetTitle(str);
		CFRelease(str), str = NULL;
	}
	
	// Track number
	if(tags->track) {
		if(tags->track->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->index);
			SetTrackNumber(num);
			CFRelease(num), num = NULL;
		}
		
		if(tags->track->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->track->total);
			SetTrackTotal(num);
			CFRelease(num), num = NULL;
		}
	}
	
	// Disc number
	if(tags->disk) {
		if(tags->disk->index) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->index);
			SetDiscNumber(num);
			CFRelease(num), num = NULL;
		}
		
		if(tags->disk->total) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &tags->disk->total);
			SetDiscTotal(num);
			CFRelease(num), num = NULL;
		}
	}
	
	// Compilation
	if(tags->compilation)
		SetCompilation(*(tags->compilation) ? kCFBooleanTrue : kCFBooleanFalse);
	
	// BPM
	if(tags->tempo) {
		
	}
	
	// Lyrics
	if(tags->lyrics) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tags->lyrics, kCFStringEncodingUTF8);
		SetLyrics(str);
		CFRelease(str), str = NULL;
	}
	
	// Album art
	if(tags->artworkCount) {
		for(uint32_t i = 0; i < tags->artworkCount; ++i) {
			CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(tags->artwork[i].data), tags->artwork[i].size);
			SetFrontCoverArt(data);
			CFRelease(data), data = NULL;
		}
	}
	
	// ReplayGain
	MP4ItmfItemList *itemList = MP4ItmfGetItemsByMeaning(file, "replaygain_reference_loudness", NULL);
	

	MP4ItmfItemListFree(itemList), itemList = NULL;
	
//	if(MP4GetMetadataFreeForm(file, "replaygain_reference_loudness", &rawValue, &rawValueSize, NULL)) {
//		NSString	*value			= [[NSString alloc] initWithBytes:rawValue length:rawValueSize encoding:NSUTF8StringEncoding];
//		NSScanner	*scanner		= [NSScanner scannerWithString:value];
//		double		doubleValue		= 0.0;
//		
//		if([scanner scanDouble:&doubleValue])
//			[metadataDictionary setValue:[NSNumber numberWithDouble:doubleValue] forKey:ReplayGainReferenceLoudnessKey];
//		
//		[value release];
//	}
//	
//	if(MP4GetMetadataFreeForm(file, "replaygain_track_gain", &rawValue, &rawValueSize, NULL)) {
//		NSString	*value			= [[NSString alloc] initWithBytes:rawValue length:rawValueSize encoding:NSUTF8StringEncoding];
//		NSScanner	*scanner		= [NSScanner scannerWithString:value];
//		double		doubleValue		= 0.0;
//		
//		if([scanner scanDouble:&doubleValue])
//			[metadataDictionary setValue:[NSNumber numberWithDouble:doubleValue] forKey:ReplayGainTrackGainKey];
//		
//		[value release];
//	}
//	
//	if(MP4GetMetadataFreeForm(file, "replaygain_track_peak", &rawValue, &rawValueSize, NULL)) {
//		NSString *value = [[NSString alloc] initWithBytes:rawValue length:rawValueSize encoding:NSUTF8StringEncoding];
//		[metadataDictionary setValue:[NSNumber numberWithDouble:[value doubleValue]] forKey:ReplayGainTrackPeakKey];
//		[value release];
//	}
//	
//	if(MP4GetMetadataFreeForm(file, "replaygain_album_gain", &rawValue, &rawValueSize, NULL)) {
//		NSString	*value			= [[NSString alloc] initWithBytes:rawValue length:rawValueSize encoding:NSUTF8StringEncoding];
//		NSScanner	*scanner		= [NSScanner scannerWithString:value];
//		double		doubleValue		= 0.0;
//		
//		if([scanner scanDouble:&doubleValue])
//			[metadataDictionary setValue:[NSNumber numberWithDouble:doubleValue] forKey:ReplayGainAlbumGainKey];
//		
//		[value release];
//	}
//	
//	if(MP4GetMetadataFreeForm(file, "replaygain_album_peak", &rawValue, &rawValueSize, NULL)) {
//		NSString *value = [[NSString alloc] initWithBytes:rawValue length:rawValueSize encoding:NSUTF8StringEncoding];
//		[metadataDictionary setValue:[NSNumber numberWithDouble:[value doubleValue]] forKey:ReplayGainAlbumPeakKey];
//		[value release];
//	}
	
	// Clean up
	MP4TagsFree(tags), tags = NULL;
	MP4Close(file), file = NULL;
	
	return true;
}

bool MP4Metadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(false == CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	// Open the file for modification
	MP4FileHandle file = MP4Modify(reinterpret_cast<const char *>(buf), MP4_DETAILS_ERROR, 0);
	if(MP4_INVALID_FILE_HANDLE == file) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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

	// Lyrics
	str = GetLyrics();
	
	if(str) {
		CFIndex cStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
		char cString [cStringSize + 1];
		
		if(false == CFStringGetCString(str, cString, cStringSize + 1, kCFStringEncodingUTF8)) {
			ERR("CFStringGetCString failed");
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
	
	// Application version
//	NSString *appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
//	NSString *shortVersionNumber = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
//	NSString *versionNumber = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];	
//	
//	MP4TagsSetEncodingTool(tags, [[NSString stringWithFormat:@"%@ %@ (%@)", appName, shortVersionNumber, versionNumber] UTF8String]);
	
	// Save our changes
	MP4TagsStore(tags, file);
	
	// Clean up
	MP4TagsFree(tags), tags = NULL;
	MP4Close(file), file = NULL;

	return true;
}
