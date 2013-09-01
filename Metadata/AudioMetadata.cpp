/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif

#include "AudioMetadata.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef	AudioMetadataErrorDomain				= CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioMetadata");

// ========================================
// Key names for the metadata dictionary
// ========================================
const CFStringRef	kPropertiesFormatNameKey				= CFSTR("Format Name");
const CFStringRef	kPropertiesTotalFramesKey				= CFSTR("Total Frames");
const CFStringRef	kPropertiesChannelsPerFrameKey			= CFSTR("Channels Per Frame");
const CFStringRef	kPropertiesBitsPerChannelKey			= CFSTR("Bits per Channel");
const CFStringRef	kPropertiesSampleRateKey				= CFSTR("Sample Rate");
const CFStringRef	kPropertiesDurationKey					= CFSTR("Duration");
const CFStringRef	kPropertiesBitrateKey					= CFSTR("Bitrate");

const CFStringRef	kMetadataTitleKey						= CFSTR("Title");
const CFStringRef	kMetadataAlbumTitleKey					= CFSTR("Album Title");
const CFStringRef	kMetadataArtistKey						= CFSTR("Artist");
const CFStringRef	kMetadataAlbumArtistKey					= CFSTR("Album Artist");
const CFStringRef	kMetadataGenreKey						= CFSTR("Genre");
const CFStringRef	kMetadataComposerKey					= CFSTR("Composer");
const CFStringRef	kMetadataReleaseDateKey					= CFSTR("Date");
const CFStringRef	kMetadataCompilationKey					= CFSTR("Compilation");
const CFStringRef	kMetadataTrackNumberKey					= CFSTR("Track Number");
const CFStringRef	kMetadataTrackTotalKey					= CFSTR("Track Total");
const CFStringRef	kMetadataDiscNumberKey					= CFSTR("Disc Number");
const CFStringRef	kMetadataDiscTotalKey					= CFSTR("Disc Total");
const CFStringRef	kMetadataLyricsKey						= CFSTR("Lyrics");
const CFStringRef	kMetadataBPMKey							= CFSTR("BPM");
const CFStringRef	kMetadataRatingKey						= CFSTR("Rating");
const CFStringRef	kMetadataCommentKey						= CFSTR("Comment");
const CFStringRef	kMetadataISRCKey						= CFSTR("ISRC");
const CFStringRef	kMetadataMCNKey							= CFSTR("MCN");
const CFStringRef	kMetadataMusicBrainzReleaseIDKey		= CFSTR("MusicBrainz Release ID");
const CFStringRef	kMetadataMusicBrainzRecordingIDKey		= CFSTR("MusicBrainz Recording ID");

const CFStringRef	kMetadataTitleSortOrderKey				= CFSTR("Title Sort Order");
const CFStringRef	kMetadataAlbumTitleSortOrderKey			= CFSTR("Album Title Sort Order");
const CFStringRef	kMetadataArtistSortOrderKey				= CFSTR("Artist Sort Order");
const CFStringRef	kMetadataAlbumArtistSortOrderKey		= CFSTR("Album Artist Sort Order");
const CFStringRef	kMetadataComposerSortOrderKey			= CFSTR("Composer Sort Order");

const CFStringRef	kMetadataGroupingKey					= CFSTR("Grouping");

const CFStringRef	kMetadataAdditionalMetadataKey			= CFSTR("Additional Metadata");

const CFStringRef	kReplayGainReferenceLoudnessKey			= CFSTR("Replay Gain Reference Loudness");
const CFStringRef	kReplayGainTrackGainKey					= CFSTR("Replay Gain Track Gain");
const CFStringRef	kReplayGainTrackPeakKey					= CFSTR("Replay Gain Track Peak");
const CFStringRef	kReplayGainAlbumGainKey					= CFSTR("Replay Gain Album Gain");
const CFStringRef	kReplayGainAlbumPeakKey					= CFSTR("Replay Gain Album Peak");

#pragma mark Static Methods

std::vector<AudioMetadata::SubclassInfo> AudioMetadata::sRegisteredSubclasses;

CFArrayRef AudioMetadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedFileExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		CFArrayRef decoderFileExtensions = subclassInfo.mCreateSupportedFileExtensions();
		CFArrayAppendArray(supportedFileExtensions, decoderFileExtensions, CFRangeMake(0, CFArrayGetCount(decoderFileExtensions)));
		CFRelease(decoderFileExtensions);
	}

	return supportedFileExtensions;
}

CFArrayRef AudioMetadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		CFArrayRef decoderMIMETypes = subclassInfo.mCreateSupportedMIMETypes();
		CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
		CFRelease(decoderMIMETypes);
	}

	return supportedMIMETypes;
}

bool AudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesFilesWithExtension(extension))
			return true;
	}

	return false;
}

bool AudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesMIMEType(mimeType))
			return true;
	}

	return false;
}

AudioMetadata * AudioMetadata::CreateMetadataForURL(CFURLRef url, CFErrorRef *error)
{
	if(nullptr == url)
		return nullptr;
	
	AudioMetadata *metadata = nullptr;
	
	// If this is a file URL, use the extension-based resolvers
	CFStringRef scheme = CFURLCopyScheme(url);

	// If there is no scheme the URL is invalid
	if(nullptr == scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, nullptr);
		return nullptr;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		CFBooleanRef fileExists = (CFBooleanRef)CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode);
		
		if(fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				CFStringRef pathExtension = CFURLCopyPathExtension(url);
				if(pathExtension) {
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

					for(auto subclassInfo : sRegisteredSubclasses) {
						if(subclassInfo.mHandlesFilesWithExtension(pathExtension)) {
							metadata = subclassInfo.mCreateMetadata(url);
							if(!metadata->ReadMetadata(error))
								delete metadata, metadata = nullptr;
						}

						if(metadata)
							break;
					}

					CFRelease(pathExtension), pathExtension = nullptr;
				}				
			}
			else {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "The requested URL doesn't exist");
				
				if(error) {
					CFStringRef description = CFCopyLocalizedString(CFSTR("The file “%@” does not exist."), "");
					CFStringRef failureReason = CFCopyLocalizedString(CFSTR("File not found"), "");
					CFStringRef recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may exist on removable media or may have been deleted."), "");
					
					*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, url, failureReason, recoverySuggestion);
					
					CFRelease(description), description = nullptr;
					CFRelease(failureReason), failureReason = nullptr;
					CFRelease(recoverySuggestion), recoverySuggestion = nullptr;
				}				
			}

			CFRelease(fileExists), fileExists = nullptr;
		}
		else
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "CFURLCreatePropertyFromResource failed: " << errorCode);		
	}

	CFRelease(scheme), scheme = nullptr;
	
	return metadata;
}

#pragma mark Creation and Destruction

AudioMetadata::AudioMetadata()
	: mURL(nullptr)
{	
	mMetadata			= CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	mChangedMetadata	= CFDictionaryCreateMutable(kCFAllocatorDefault,  0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

AudioMetadata::AudioMetadata(CFURLRef url)
	: AudioMetadata()
{
	mURL = (CFURLRef)CFRetain(url);
}

AudioMetadata::~AudioMetadata()
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = nullptr;

	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = nullptr;
}

void AudioMetadata::SetURL(CFURLRef URL)
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(URL)
		mURL = (CFURLRef)CFRetain(URL);
}

#pragma mark Change management

bool AudioMetadata::HasUnsavedChanges() const
{
	if(CFDictionaryGetCount(mChangedMetadata))
		return true;

	for(auto picture : mPictures) {
		if(AttachedPicture::ChangeState::Saved != picture->mState || picture->HasUnsavedChanges())
			return true;
	}

	return false;
}

void AudioMetadata::RevertUnsavedChanges()
{
	CFDictionaryRemoveAllValues(mChangedMetadata);

	auto iter = mPictures.begin();
	while(iter != mPictures.end()) {
		auto picture = *iter;
		if(AttachedPicture::ChangeState::Removed == picture->mState) {
			picture->mState = AttachedPicture::ChangeState::Saved;
			picture->RevertUnsavedChanges();
		}
		else
			picture->RevertUnsavedChanges();
	}
}

#pragma mark Properties Access

CFStringRef AudioMetadata::GetFormatName() const
{
	return GetStringValue(kPropertiesFormatNameKey);
}

CFNumberRef AudioMetadata::GetTotalFrames() const
{
	return GetNumberValue(kPropertiesTotalFramesKey);
}

CFNumberRef AudioMetadata::GetChannelsPerFrame() const
{
	return GetNumberValue(kPropertiesChannelsPerFrameKey);
}

CFNumberRef AudioMetadata::GetBitsPerChannel() const
{
	return GetNumberValue(kPropertiesBitsPerChannelKey);
}

CFNumberRef AudioMetadata::GetSampleRate() const
{
	return GetNumberValue(kPropertiesSampleRateKey);
}

CFNumberRef AudioMetadata::GetDuration() const
{
	return GetNumberValue(kPropertiesDurationKey);
}

CFNumberRef AudioMetadata::GetBitrate() const
{
	return GetNumberValue(kPropertiesBitrateKey);
}

#pragma mark Metadata Access

CFStringRef AudioMetadata::GetTitle() const
{
	return GetStringValue(kMetadataTitleKey);
}

void AudioMetadata::SetTitle(CFStringRef title)
{
	SetValue(kMetadataTitleKey, title);
}

CFStringRef AudioMetadata::GetAlbumTitle() const
{
	return GetStringValue(kMetadataAlbumTitleKey);
}

void AudioMetadata::SetAlbumTitle(CFStringRef albumTitle)
{
	SetValue(kMetadataAlbumTitleKey, albumTitle);
}

CFStringRef AudioMetadata::GetArtist() const
{
	return GetStringValue(kMetadataArtistKey);
}

void AudioMetadata::SetArtist(CFStringRef artist)
{
	SetValue(kMetadataArtistKey, artist);
}

CFStringRef AudioMetadata::GetAlbumArtist() const
{
	return GetStringValue(kMetadataAlbumArtistKey);
}

void AudioMetadata::SetAlbumArtist(CFStringRef albumArtist)
{
	SetValue(kMetadataAlbumArtistKey, albumArtist);
}

CFStringRef AudioMetadata::GetGenre() const
{
	return GetStringValue(kMetadataGenreKey);
}

void AudioMetadata::SetGenre(CFStringRef genre)
{
	SetValue(kMetadataGenreKey, genre);
}

CFStringRef AudioMetadata::GetComposer() const
{
	return GetStringValue(kMetadataComposerKey);
}

void AudioMetadata::SetComposer(CFStringRef composer)
{
	SetValue(kMetadataComposerKey, composer);
}

CFStringRef AudioMetadata::GetReleaseDate() const
{
	return GetStringValue(kMetadataReleaseDateKey);
}

void AudioMetadata::SetReleaseDate(CFStringRef releaseDate)
{
	SetValue(kMetadataReleaseDateKey, releaseDate);
}

CFBooleanRef AudioMetadata::GetCompilation() const
{
	CFTypeRef value = GetValue(kMetadataCompilationKey);
	
	if(nullptr == value)
		return nullptr;

	if(CFBooleanGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFBooleanRef)value;
}

void AudioMetadata::SetCompilation(CFBooleanRef compilation)
{
	SetValue(kMetadataCompilationKey, compilation);
}

CFNumberRef AudioMetadata::GetTrackNumber() const
{
	return GetNumberValue(kMetadataTrackNumberKey);
}

void AudioMetadata::SetTrackNumber(CFNumberRef trackNumber)
{
	SetValue(kMetadataTrackNumberKey, trackNumber);
}

CFNumberRef AudioMetadata::GetTrackTotal() const
{
	return GetNumberValue(kMetadataTrackTotalKey);
}

void AudioMetadata::SetTrackTotal(CFNumberRef trackTotal)
{
	SetValue(kMetadataTrackTotalKey, trackTotal);
}

CFNumberRef AudioMetadata::GetDiscNumber() const
{
	return GetNumberValue(kMetadataDiscNumberKey);
}

void AudioMetadata::SetDiscNumber(CFNumberRef discNumber)
{
	SetValue(kMetadataDiscNumberKey, discNumber);
}

CFNumberRef AudioMetadata::GetDiscTotal() const
{
	return GetNumberValue(kMetadataDiscTotalKey);
}

void AudioMetadata::SetDiscTotal(CFNumberRef discTotal)
{
	SetValue(kMetadataDiscTotalKey, discTotal);
}

CFStringRef AudioMetadata::GetLyrics() const
{
	return GetStringValue(kMetadataLyricsKey);
}

void AudioMetadata::SetLyrics(CFStringRef lyrics)
{
	SetValue(kMetadataLyricsKey, lyrics);
}

CFNumberRef AudioMetadata::GetBPM() const
{
	return GetNumberValue(kMetadataBPMKey);
}

void AudioMetadata::SetBPM(CFNumberRef BPM)
{
	SetValue(kMetadataBPMKey, BPM);
}

CFNumberRef AudioMetadata::GetRating() const
{
	return GetNumberValue(kMetadataRatingKey);
}

void AudioMetadata::SetRating(CFNumberRef rating)
{
	SetValue(kMetadataRatingKey, rating);
}

CFStringRef AudioMetadata::GetComment() const
{
	return GetStringValue(kMetadataCommentKey);
}

void AudioMetadata::SetComment(CFStringRef comment)
{
	SetValue(kMetadataCommentKey, comment);
}

CFStringRef AudioMetadata::GetMCN() const
{
	return GetStringValue(kMetadataMCNKey);
}

void AudioMetadata::SetMCN(CFStringRef mcn)
{
	SetValue(kMetadataMCNKey, mcn);
}

CFStringRef AudioMetadata::GetISRC() const
{
	return GetStringValue(kMetadataISRCKey);
}

void AudioMetadata::SetISRC(CFStringRef isrc)
{
	SetValue(kMetadataISRCKey, isrc);
}

CFStringRef AudioMetadata::GetMusicBrainzReleaseID() const
{
	return GetStringValue(kMetadataMusicBrainzReleaseIDKey);
}

void AudioMetadata::SetMusicBrainzReleaseID(CFStringRef releaseID)
{
	SetValue(kMetadataMusicBrainzReleaseIDKey, releaseID);
}

CFStringRef AudioMetadata::GetMusicBrainzRecordingID() const
{
	return GetStringValue(kMetadataMusicBrainzRecordingIDKey);
}

void AudioMetadata::SetMusicBrainzRecordingID(CFStringRef recordingID)
{
	SetValue(kMetadataMusicBrainzRecordingIDKey, recordingID);
}

CFStringRef AudioMetadata::GetTitleSortOrder() const
{
	return GetStringValue(kMetadataTitleSortOrderKey);
}

void AudioMetadata::SetTitleSortOrder(CFStringRef titleSortOrder)
{
	SetValue(kMetadataTitleSortOrderKey, titleSortOrder);
}

CFStringRef AudioMetadata::GetAlbumTitleSortOrder() const
{
	return GetStringValue(kMetadataAlbumTitleSortOrderKey);
}

void AudioMetadata::SetAlbumTitleSortOrder(CFStringRef albumTitleSortOrder)
{
	SetValue(kMetadataAlbumTitleSortOrderKey, albumTitleSortOrder);
}

CFStringRef AudioMetadata::GetArtistSortOrder() const
{
	return GetStringValue(kMetadataArtistSortOrderKey);
}

void AudioMetadata::SetArtistSortOrder(CFStringRef artistSortOrder)
{
	SetValue(kMetadataArtistSortOrderKey, artistSortOrder);
}

CFStringRef AudioMetadata::GetAlbumArtistSortOrder() const
{
	return GetStringValue(kMetadataAlbumArtistSortOrderKey);
}

void AudioMetadata::SetAlbumArtistSortOrder(CFStringRef albumArtistSortOrder)
{
	SetValue(kMetadataAlbumArtistSortOrderKey, albumArtistSortOrder);
}

CFStringRef AudioMetadata::GetComposerSortOrder() const
{
	return GetStringValue(kMetadataComposerSortOrderKey);
}

void AudioMetadata::SetComposerSortOrder(CFStringRef composerSortOrder)
{
	SetValue(kMetadataComposerSortOrderKey, composerSortOrder);
}

CFStringRef AudioMetadata::GetGrouping() const
{
	return GetStringValue(kMetadataGroupingKey);
}

void AudioMetadata::SetGrouping(CFStringRef grouping)
{
	SetValue(kMetadataGroupingKey, grouping);
}

#pragma mark Additional Metadata

CFDictionaryRef AudioMetadata::GetAdditionalMetadata() const
{
	CFTypeRef value = GetValue(kMetadataAdditionalMetadataKey);
	
	if(nullptr == value)
		return nullptr;

	if(CFDictionaryGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFDictionaryRef)value;
}

void AudioMetadata::SetAdditionalMetadata(CFDictionaryRef additionalMetadata)
{
	SetValue(kMetadataAdditionalMetadataKey, additionalMetadata);
}

#pragma mark Replay Gain Information

CFNumberRef AudioMetadata::GetReplayGainReferenceLoudness() const
{
	return GetNumberValue(kReplayGainReferenceLoudnessKey);
}

void AudioMetadata::SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness)
{
	SetValue(kReplayGainReferenceLoudnessKey, referenceLoudness);
}

CFNumberRef AudioMetadata::GetReplayGainTrackGain() const
{
	return GetNumberValue(kReplayGainTrackGainKey);
}

void AudioMetadata::SetReplayGainTrackGain(CFNumberRef trackGain)
{
	SetValue(kReplayGainTrackGainKey, trackGain);
}

CFNumberRef AudioMetadata::GetReplayGainTrackPeak() const
{
	return GetNumberValue(kReplayGainTrackPeakKey);
}

void AudioMetadata::SetReplayGainTrackPeak(CFNumberRef trackPeak)
{
	SetValue(kReplayGainTrackPeakKey, trackPeak);
}

CFNumberRef AudioMetadata::GetReplayGainAlbumGain() const
{
	return GetNumberValue(kReplayGainAlbumGainKey);
}

void AudioMetadata::SetReplayGainAlbumGain(CFNumberRef albumGain)
{
	SetValue(kReplayGainAlbumGainKey, albumGain);
}

CFNumberRef AudioMetadata::GetReplayGainAlbumPeak() const
{
	return GetNumberValue(kReplayGainAlbumPeakKey);
}

void AudioMetadata::SetReplayGainAlbumPeak(CFNumberRef albumPeak)
{
	SetValue(kReplayGainAlbumPeakKey, albumPeak);
}

#pragma mark Album Artwork

const std::vector<std::shared_ptr<AttachedPicture>> AudioMetadata::GetAttachedPictures() const
{
	std::vector<std::shared_ptr<AttachedPicture>> result;

	std::copy_if(mPictures.begin(), mPictures.end(), std::back_inserter(result), [](const std::shared_ptr<AttachedPicture>& picture) {
		return AttachedPicture::ChangeState::Removed != picture->mState;
	});

	return result;
}

const std::vector<std::shared_ptr<AttachedPicture>> AudioMetadata::GetAttachedPicturesOfType(AttachedPicture::Type type) const
{
	std::vector<std::shared_ptr<AttachedPicture>> result;

	std::copy_if(mPictures.begin(), mPictures.end(), std::back_inserter(result), [type](const std::shared_ptr<AttachedPicture>& picture) {
		return AttachedPicture::ChangeState::Removed != picture->mState && type == picture->GetType();
	});

	return result;
}

void AudioMetadata::AttachPicture(std::shared_ptr<AttachedPicture> picture)
{
	if(picture) {
		auto match = std::find(mPictures.begin(), mPictures.end(), picture);
		if(match != mPictures.end()) {
			if(AttachedPicture::ChangeState::Removed == picture->mState)
				picture->mState = AttachedPicture::ChangeState::Saved;
		}
		// By default a picture is created with mState == ChangeState::Saved
		else {
			picture->mState = AttachedPicture::ChangeState::Added;
			mPictures.push_back(std::shared_ptr<AttachedPicture>(picture));
		}
	}
}

void AudioMetadata::RemoveAttachedPicture(std::shared_ptr<AttachedPicture> picture)
{
	if(picture) {
		auto match = std::find(mPictures.begin(), mPictures.end(), picture);
		if(match != mPictures.end()) {
			if((*match)->mState == AttachedPicture::ChangeState::Added)
				mPictures.erase(match);
			else
				(*match)->mState = AttachedPicture::ChangeState::Removed;
		}
	}
}

void AudioMetadata::RemoveAttachedPicturesOfType(AttachedPicture::Type type)
{
	for(auto iter = mPictures.begin(); iter != mPictures.end(); ++iter) {
		auto picture = *iter;
		if(type == picture->GetType()) {
			if(picture->mState == AttachedPicture::ChangeState::Added)
				iter = mPictures.erase(iter);
			else
				picture->mState = AttachedPicture::ChangeState::Removed;
		}
	}
}

void AudioMetadata::RemoveAllAttachedPictures()
{
	std::for_each(mPictures.begin(), mPictures.end(), [](const std::shared_ptr<AttachedPicture>& picture){
		picture->mState = AttachedPicture::ChangeState::Removed;
	});
}

#pragma mark Type-Specific Access

CFStringRef AudioMetadata::GetStringValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(nullptr == value)
		return nullptr;
	
	if(CFStringGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFStringRef)value;
}

CFNumberRef AudioMetadata::GetNumberValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(nullptr == value)
		return nullptr;

	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFNumberRef)value;
}

#pragma mark Generic Access

CFTypeRef AudioMetadata::GetValue(CFStringRef key) const
{
	if(nullptr == key)
		return nullptr;
	
	if(CFDictionaryContainsKey(mChangedMetadata, key)) {
		CFTypeRef value = CFDictionaryGetValue(mChangedMetadata, key);
		return (kCFNull == value ? nullptr : value);
	}

	return CFDictionaryGetValue(mMetadata, key);
}

void AudioMetadata::SetValue(CFStringRef key, CFTypeRef value)
{
	if(nullptr == key)
		return;

	if(nullptr == value) {
		if(CFDictionaryContainsKey(mMetadata, key))
			CFDictionarySetValue(mChangedMetadata, key, kCFNull);
		else
			CFDictionaryRemoveValue(mChangedMetadata, key);
	}
	else {
		if(CFDictionaryContainsKey(mChangedMetadata, key)) {
			CFTypeRef savedValue = CFDictionaryGetValue(mMetadata, key);
			if(savedValue && CFEqual(savedValue, value))
				CFDictionaryRemoveValue(mChangedMetadata, key);
			else
				CFDictionarySetValue(mChangedMetadata, key, value);
		}
		else
			CFDictionarySetValue(mChangedMetadata, key, value);
	}
}

void AudioMetadata::ClearAllMetadata()
{
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	mPictures.clear();
}

void AudioMetadata::MergeChangedMetadataIntoMetadata()
{
	CFIndex count = CFDictionaryGetCount(mChangedMetadata);
	
	CFTypeRef *keys = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);
	CFTypeRef *values = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);
	
	CFDictionaryGetKeysAndValues(mChangedMetadata, keys, values);
	
	for(CFIndex i = 0; i < count; ++i) {
		if(kCFNull == values[i])
			CFDictionaryRemoveValue(mMetadata, keys[i]);
		else
			CFDictionarySetValue(mMetadata, keys[i], values[i]);
	}
	
	free(keys), keys = nullptr;
	free(values), values = nullptr;
	
	CFDictionaryRemoveAllValues(mChangedMetadata);

	auto iter = mPictures.begin();
	while(iter != mPictures.end()) {
		auto picture = *iter;
		if(AttachedPicture::ChangeState::Removed == picture->mState)
			iter = mPictures.erase(iter);
		else {
			picture->MergeChangedMetadataIntoMetadata();
			picture->mState = AttachedPicture::ChangeState::Saved;
			++iter;
		}
	}
}
