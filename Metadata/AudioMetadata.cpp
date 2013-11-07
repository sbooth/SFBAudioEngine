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
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef	SFB::Audio::AudioMetadataErrorDomain				= CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioMetadata");

// ========================================
// Key names for the metadata dictionary
// ========================================
const CFStringRef	SFB::Audio::kPropertiesFormatNameKey				= CFSTR("Format Name");
const CFStringRef	SFB::Audio::kPropertiesTotalFramesKey				= CFSTR("Total Frames");
const CFStringRef	SFB::Audio::kPropertiesChannelsPerFrameKey			= CFSTR("Channels Per Frame");
const CFStringRef	SFB::Audio::kPropertiesBitsPerChannelKey			= CFSTR("Bits per Channel");
const CFStringRef	SFB::Audio::kPropertiesSampleRateKey				= CFSTR("Sample Rate");
const CFStringRef	SFB::Audio::kPropertiesDurationKey					= CFSTR("Duration");
const CFStringRef	SFB::Audio::kPropertiesBitrateKey					= CFSTR("Bitrate");

const CFStringRef	SFB::Audio::kMetadataTitleKey						= CFSTR("Title");
const CFStringRef	SFB::Audio::kMetadataAlbumTitleKey					= CFSTR("Album Title");
const CFStringRef	SFB::Audio::kMetadataArtistKey						= CFSTR("Artist");
const CFStringRef	SFB::Audio::kMetadataAlbumArtistKey					= CFSTR("Album Artist");
const CFStringRef	SFB::Audio::kMetadataGenreKey						= CFSTR("Genre");
const CFStringRef	SFB::Audio::kMetadataComposerKey					= CFSTR("Composer");
const CFStringRef	SFB::Audio::kMetadataReleaseDateKey					= CFSTR("Date");
const CFStringRef	SFB::Audio::kMetadataCompilationKey					= CFSTR("Compilation");
const CFStringRef	SFB::Audio::kMetadataTrackNumberKey					= CFSTR("Track Number");
const CFStringRef	SFB::Audio::kMetadataTrackTotalKey					= CFSTR("Track Total");
const CFStringRef	SFB::Audio::kMetadataDiscNumberKey					= CFSTR("Disc Number");
const CFStringRef	SFB::Audio::kMetadataDiscTotalKey					= CFSTR("Disc Total");
const CFStringRef	SFB::Audio::kMetadataLyricsKey						= CFSTR("Lyrics");
const CFStringRef	SFB::Audio::kMetadataBPMKey							= CFSTR("BPM");
const CFStringRef	SFB::Audio::kMetadataRatingKey						= CFSTR("Rating");
const CFStringRef	SFB::Audio::kMetadataCommentKey						= CFSTR("Comment");
const CFStringRef	SFB::Audio::kMetadataISRCKey						= CFSTR("ISRC");
const CFStringRef	SFB::Audio::kMetadataMCNKey							= CFSTR("MCN");
const CFStringRef	SFB::Audio::kMetadataMusicBrainzReleaseIDKey		= CFSTR("MusicBrainz Release ID");
const CFStringRef	SFB::Audio::kMetadataMusicBrainzRecordingIDKey		= CFSTR("MusicBrainz Recording ID");

const CFStringRef	SFB::Audio::kMetadataTitleSortOrderKey				= CFSTR("Title Sort Order");
const CFStringRef	SFB::Audio::kMetadataAlbumTitleSortOrderKey			= CFSTR("Album Title Sort Order");
const CFStringRef	SFB::Audio::kMetadataArtistSortOrderKey				= CFSTR("Artist Sort Order");
const CFStringRef	SFB::Audio::kMetadataAlbumArtistSortOrderKey		= CFSTR("Album Artist Sort Order");
const CFStringRef	SFB::Audio::kMetadataComposerSortOrderKey			= CFSTR("Composer Sort Order");

const CFStringRef	SFB::Audio::kMetadataGroupingKey					= CFSTR("Grouping");

const CFStringRef	SFB::Audio::kMetadataAdditionalMetadataKey			= CFSTR("Additional Metadata");

const CFStringRef	SFB::Audio::kReplayGainReferenceLoudnessKey			= CFSTR("Replay Gain Reference Loudness");
const CFStringRef	SFB::Audio::kReplayGainTrackGainKey					= CFSTR("Replay Gain Track Gain");
const CFStringRef	SFB::Audio::kReplayGainTrackPeakKey					= CFSTR("Replay Gain Track Peak");
const CFStringRef	SFB::Audio::kReplayGainAlbumGainKey					= CFSTR("Replay Gain Album Gain");
const CFStringRef	SFB::Audio::kReplayGainAlbumPeakKey					= CFSTR("Replay Gain Album Peak");

#pragma mark Static Methods

std::vector<SFB::Audio::Metadata::SubclassInfo> SFB::Audio::Metadata::sRegisteredSubclasses;

CFArrayRef SFB::Audio::Metadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedFileExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderFileExtensions = subclassInfo.mCreateSupportedFileExtensions();
		CFArrayAppendArray(supportedFileExtensions, decoderFileExtensions, CFRangeMake(0, CFArrayGetCount(decoderFileExtensions)));
	}

	return supportedFileExtensions;
}

CFArrayRef SFB::Audio::Metadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderMIMETypes = subclassInfo.mCreateSupportedMIMETypes();
		CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	}

	return supportedMIMETypes;
}

bool SFB::Audio::Metadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesFilesWithExtension(extension))
			return true;
	}

	return false;
}

bool SFB::Audio::Metadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesMIMEType(mimeType))
			return true;
	}

	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::Metadata::CreateMetadataForURL(CFURLRef url, CFErrorRef *error)
{
	if(nullptr == url)
		return nullptr;

	// If this is a file URL, use the extension-based resolvers
	SFB::CFString scheme = CFURLCopyScheme(url);

	// If there is no scheme the URL is invalid
	if(!scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, nullptr);
		return nullptr;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		SFB::CFBoolean fileExists = (CFBooleanRef)CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode);
		
		if(fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				SFB::CFString pathExtension = CFURLCopyPathExtension(url);
				if(pathExtension) {
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

					for(auto subclassInfo : sRegisteredSubclasses) {
						if(subclassInfo.mHandlesFilesWithExtension(pathExtension)) {
							unique_ptr metadata(subclassInfo.mCreateMetadata(url));
							if(metadata->ReadMetadata(error))
								return metadata;
						}
					}
				}
			}
			else {
				LOGGER_WARNING("org.sbooth.AudioEngine.Metadata", "The requested URL doesn't exist");
				
				if(error) {
					SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” does not exist."), "");
					SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File not found"), "");
					SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may exist on removable media or may have been deleted."), "");
					
					*error = CreateErrorForURL(AudioMetadataErrorDomain, AudioMetadataInputOutputError, description, url, failureReason, recoverySuggestion);
				}
			}
		}
		else
			LOGGER_WARNING("org.sbooth.AudioEngine.Metadata", "CFURLCreatePropertyFromResource failed: " << errorCode);		
	}

	return nullptr;
}

#pragma mark Creation and Destruction

SFB::Audio::Metadata::Metadata()
	: mURL(nullptr)
{	
	mMetadata			= CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	mChangedMetadata	= CFDictionaryCreateMutable(kCFAllocatorDefault,  0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

SFB::Audio::Metadata::Metadata(CFURLRef url)
	: Metadata()
{
	mURL = (CFURLRef)CFRetain(url);
}

SFB::Audio::Metadata::~Metadata()
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = nullptr;

	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = nullptr;
}

void SFB::Audio::Metadata::SetURL(CFURLRef URL)
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(URL)
		mURL = (CFURLRef)CFRetain(URL);
}

#pragma mark Change management

bool SFB::Audio::Metadata::HasUnsavedChanges() const
{
	if(CFDictionaryGetCount(mChangedMetadata))
		return true;

	for(auto picture : mPictures) {
		if(AttachedPicture::ChangeState::Saved != picture->mState || picture->HasUnsavedChanges())
			return true;
	}

	return false;
}

void SFB::Audio::Metadata::RevertUnsavedChanges()
{
	CFDictionaryRemoveAllValues(mChangedMetadata);

	auto iter = std::begin(mPictures);
	while(iter != std::end(mPictures)) {
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

CFStringRef SFB::Audio::Metadata::GetFormatName() const
{
	return GetStringValue(kPropertiesFormatNameKey);
}

CFNumberRef SFB::Audio::Metadata::GetTotalFrames() const
{
	return GetNumberValue(kPropertiesTotalFramesKey);
}

CFNumberRef SFB::Audio::Metadata::GetChannelsPerFrame() const
{
	return GetNumberValue(kPropertiesChannelsPerFrameKey);
}

CFNumberRef SFB::Audio::Metadata::GetBitsPerChannel() const
{
	return GetNumberValue(kPropertiesBitsPerChannelKey);
}

CFNumberRef SFB::Audio::Metadata::GetSampleRate() const
{
	return GetNumberValue(kPropertiesSampleRateKey);
}

CFNumberRef SFB::Audio::Metadata::GetDuration() const
{
	return GetNumberValue(kPropertiesDurationKey);
}

CFNumberRef SFB::Audio::Metadata::GetBitrate() const
{
	return GetNumberValue(kPropertiesBitrateKey);
}

#pragma mark Metadata Access

CFStringRef SFB::Audio::Metadata::GetTitle() const
{
	return GetStringValue(kMetadataTitleKey);
}

void SFB::Audio::Metadata::SetTitle(CFStringRef title)
{
	SetValue(kMetadataTitleKey, title);
}

CFStringRef SFB::Audio::Metadata::GetAlbumTitle() const
{
	return GetStringValue(kMetadataAlbumTitleKey);
}

void SFB::Audio::Metadata::SetAlbumTitle(CFStringRef albumTitle)
{
	SetValue(kMetadataAlbumTitleKey, albumTitle);
}

CFStringRef SFB::Audio::Metadata::GetArtist() const
{
	return GetStringValue(kMetadataArtistKey);
}

void SFB::Audio::Metadata::SetArtist(CFStringRef artist)
{
	SetValue(kMetadataArtistKey, artist);
}

CFStringRef SFB::Audio::Metadata::GetAlbumArtist() const
{
	return GetStringValue(kMetadataAlbumArtistKey);
}

void SFB::Audio::Metadata::SetAlbumArtist(CFStringRef albumArtist)
{
	SetValue(kMetadataAlbumArtistKey, albumArtist);
}

CFStringRef SFB::Audio::Metadata::GetGenre() const
{
	return GetStringValue(kMetadataGenreKey);
}

void SFB::Audio::Metadata::SetGenre(CFStringRef genre)
{
	SetValue(kMetadataGenreKey, genre);
}

CFStringRef SFB::Audio::Metadata::GetComposer() const
{
	return GetStringValue(kMetadataComposerKey);
}

void SFB::Audio::Metadata::SetComposer(CFStringRef composer)
{
	SetValue(kMetadataComposerKey, composer);
}

CFStringRef SFB::Audio::Metadata::GetReleaseDate() const
{
	return GetStringValue(kMetadataReleaseDateKey);
}

void SFB::Audio::Metadata::SetReleaseDate(CFStringRef releaseDate)
{
	SetValue(kMetadataReleaseDateKey, releaseDate);
}

CFBooleanRef SFB::Audio::Metadata::GetCompilation() const
{
	CFTypeRef value = GetValue(kMetadataCompilationKey);
	
	if(nullptr == value)
		return nullptr;

	if(CFBooleanGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFBooleanRef)value;
}

void SFB::Audio::Metadata::SetCompilation(CFBooleanRef compilation)
{
	SetValue(kMetadataCompilationKey, compilation);
}

CFNumberRef SFB::Audio::Metadata::GetTrackNumber() const
{
	return GetNumberValue(kMetadataTrackNumberKey);
}

void SFB::Audio::Metadata::SetTrackNumber(CFNumberRef trackNumber)
{
	SetValue(kMetadataTrackNumberKey, trackNumber);
}

CFNumberRef SFB::Audio::Metadata::GetTrackTotal() const
{
	return GetNumberValue(kMetadataTrackTotalKey);
}

void SFB::Audio::Metadata::SetTrackTotal(CFNumberRef trackTotal)
{
	SetValue(kMetadataTrackTotalKey, trackTotal);
}

CFNumberRef SFB::Audio::Metadata::GetDiscNumber() const
{
	return GetNumberValue(kMetadataDiscNumberKey);
}

void SFB::Audio::Metadata::SetDiscNumber(CFNumberRef discNumber)
{
	SetValue(kMetadataDiscNumberKey, discNumber);
}

CFNumberRef SFB::Audio::Metadata::GetDiscTotal() const
{
	return GetNumberValue(kMetadataDiscTotalKey);
}

void SFB::Audio::Metadata::SetDiscTotal(CFNumberRef discTotal)
{
	SetValue(kMetadataDiscTotalKey, discTotal);
}

CFStringRef SFB::Audio::Metadata::GetLyrics() const
{
	return GetStringValue(kMetadataLyricsKey);
}

void SFB::Audio::Metadata::SetLyrics(CFStringRef lyrics)
{
	SetValue(kMetadataLyricsKey, lyrics);
}

CFNumberRef SFB::Audio::Metadata::GetBPM() const
{
	return GetNumberValue(kMetadataBPMKey);
}

void SFB::Audio::Metadata::SetBPM(CFNumberRef BPM)
{
	SetValue(kMetadataBPMKey, BPM);
}

CFNumberRef SFB::Audio::Metadata::GetRating() const
{
	return GetNumberValue(kMetadataRatingKey);
}

void SFB::Audio::Metadata::SetRating(CFNumberRef rating)
{
	SetValue(kMetadataRatingKey, rating);
}

CFStringRef SFB::Audio::Metadata::GetComment() const
{
	return GetStringValue(kMetadataCommentKey);
}

void SFB::Audio::Metadata::SetComment(CFStringRef comment)
{
	SetValue(kMetadataCommentKey, comment);
}

CFStringRef SFB::Audio::Metadata::GetMCN() const
{
	return GetStringValue(kMetadataMCNKey);
}

void SFB::Audio::Metadata::SetMCN(CFStringRef mcn)
{
	SetValue(kMetadataMCNKey, mcn);
}

CFStringRef SFB::Audio::Metadata::GetISRC() const
{
	return GetStringValue(kMetadataISRCKey);
}

void SFB::Audio::Metadata::SetISRC(CFStringRef isrc)
{
	SetValue(kMetadataISRCKey, isrc);
}

CFStringRef SFB::Audio::Metadata::GetMusicBrainzReleaseID() const
{
	return GetStringValue(kMetadataMusicBrainzReleaseIDKey);
}

void SFB::Audio::Metadata::SetMusicBrainzReleaseID(CFStringRef releaseID)
{
	SetValue(kMetadataMusicBrainzReleaseIDKey, releaseID);
}

CFStringRef SFB::Audio::Metadata::GetMusicBrainzRecordingID() const
{
	return GetStringValue(kMetadataMusicBrainzRecordingIDKey);
}

void SFB::Audio::Metadata::SetMusicBrainzRecordingID(CFStringRef recordingID)
{
	SetValue(kMetadataMusicBrainzRecordingIDKey, recordingID);
}

CFStringRef SFB::Audio::Metadata::GetTitleSortOrder() const
{
	return GetStringValue(kMetadataTitleSortOrderKey);
}

void SFB::Audio::Metadata::SetTitleSortOrder(CFStringRef titleSortOrder)
{
	SetValue(kMetadataTitleSortOrderKey, titleSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetAlbumTitleSortOrder() const
{
	return GetStringValue(kMetadataAlbumTitleSortOrderKey);
}

void SFB::Audio::Metadata::SetAlbumTitleSortOrder(CFStringRef albumTitleSortOrder)
{
	SetValue(kMetadataAlbumTitleSortOrderKey, albumTitleSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetArtistSortOrder() const
{
	return GetStringValue(kMetadataArtistSortOrderKey);
}

void SFB::Audio::Metadata::SetArtistSortOrder(CFStringRef artistSortOrder)
{
	SetValue(kMetadataArtistSortOrderKey, artistSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetAlbumArtistSortOrder() const
{
	return GetStringValue(kMetadataAlbumArtistSortOrderKey);
}

void SFB::Audio::Metadata::SetAlbumArtistSortOrder(CFStringRef albumArtistSortOrder)
{
	SetValue(kMetadataAlbumArtistSortOrderKey, albumArtistSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetComposerSortOrder() const
{
	return GetStringValue(kMetadataComposerSortOrderKey);
}

void SFB::Audio::Metadata::SetComposerSortOrder(CFStringRef composerSortOrder)
{
	SetValue(kMetadataComposerSortOrderKey, composerSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetGrouping() const
{
	return GetStringValue(kMetadataGroupingKey);
}

void SFB::Audio::Metadata::SetGrouping(CFStringRef grouping)
{
	SetValue(kMetadataGroupingKey, grouping);
}

#pragma mark Additional Metadata

CFDictionaryRef SFB::Audio::Metadata::GetAdditionalMetadata() const
{
	CFTypeRef value = GetValue(kMetadataAdditionalMetadataKey);
	
	if(nullptr == value)
		return nullptr;

	if(CFDictionaryGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFDictionaryRef)value;
}

void SFB::Audio::Metadata::SetAdditionalMetadata(CFDictionaryRef additionalMetadata)
{
	SetValue(kMetadataAdditionalMetadataKey, additionalMetadata);
}

#pragma mark Replay Gain Information

CFNumberRef SFB::Audio::Metadata::GetReplayGainReferenceLoudness() const
{
	return GetNumberValue(kReplayGainReferenceLoudnessKey);
}

void SFB::Audio::Metadata::SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness)
{
	SetValue(kReplayGainReferenceLoudnessKey, referenceLoudness);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainTrackGain() const
{
	return GetNumberValue(kReplayGainTrackGainKey);
}

void SFB::Audio::Metadata::SetReplayGainTrackGain(CFNumberRef trackGain)
{
	SetValue(kReplayGainTrackGainKey, trackGain);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainTrackPeak() const
{
	return GetNumberValue(kReplayGainTrackPeakKey);
}

void SFB::Audio::Metadata::SetReplayGainTrackPeak(CFNumberRef trackPeak)
{
	SetValue(kReplayGainTrackPeakKey, trackPeak);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainAlbumGain() const
{
	return GetNumberValue(kReplayGainAlbumGainKey);
}

void SFB::Audio::Metadata::SetReplayGainAlbumGain(CFNumberRef albumGain)
{
	SetValue(kReplayGainAlbumGainKey, albumGain);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainAlbumPeak() const
{
	return GetNumberValue(kReplayGainAlbumPeakKey);
}

void SFB::Audio::Metadata::SetReplayGainAlbumPeak(CFNumberRef albumPeak)
{
	SetValue(kReplayGainAlbumPeakKey, albumPeak);
}

#pragma mark Album Artwork

const std::vector<std::shared_ptr<SFB::Audio::AttachedPicture>> SFB::Audio::Metadata::GetAttachedPictures() const
{
	picture_vector result;

	std::copy_if(std::begin(mPictures), std::end(mPictures), std::back_inserter(result), [](const AttachedPicture::shared_ptr& picture) {
		return AttachedPicture::ChangeState::Removed != picture->mState;
	});

	return result;
}

const std::vector<std::shared_ptr<SFB::Audio::AttachedPicture>> SFB::Audio::Metadata::GetAttachedPicturesOfType(AttachedPicture::Type type) const
{
	picture_vector result;

	std::copy_if(std::begin(mPictures), std::end(mPictures), std::back_inserter(result), [type](const AttachedPicture::shared_ptr& picture) {
		return AttachedPicture::ChangeState::Removed != picture->mState && type == picture->GetType();
	});

	return result;
}

void SFB::Audio::Metadata::AttachPicture(AttachedPicture::shared_ptr picture)
{
	if(picture) {
		auto match = std::find(std::begin(mPictures), std::end(mPictures), picture);
		if(match != std::end(mPictures)) {
			if(AttachedPicture::ChangeState::Removed == picture->mState)
				picture->mState = AttachedPicture::ChangeState::Saved;
		}
		// By default a picture is created with mState == ChangeState::Saved
		else {
			picture->mState = AttachedPicture::ChangeState::Added;
			mPictures.push_back(AttachedPicture::shared_ptr(picture));
		}
	}
}

void SFB::Audio::Metadata::RemoveAttachedPicture(AttachedPicture::shared_ptr picture)
{
	if(picture) {
		auto match = std::find(std::begin(mPictures), std::end(mPictures), picture);
		if(match != std::end(mPictures)) {
			if((*match)->mState == AttachedPicture::ChangeState::Added)
				mPictures.erase(match);
			else
				(*match)->mState = AttachedPicture::ChangeState::Removed;
		}
	}
}

void SFB::Audio::Metadata::RemoveAttachedPicturesOfType(AttachedPicture::Type type)
{
	for(auto iter = std::begin(mPictures); iter != std::end(mPictures); ++iter) {
		auto picture = *iter;
		if(type == picture->GetType()) {
			if(picture->mState == AttachedPicture::ChangeState::Added)
				iter = mPictures.erase(iter);
			else
				picture->mState = AttachedPicture::ChangeState::Removed;
		}
	}
}

void SFB::Audio::Metadata::RemoveAllAttachedPictures()
{
	std::for_each(std::begin(mPictures), std::end(mPictures), [](const AttachedPicture::shared_ptr& picture){
		picture->mState = AttachedPicture::ChangeState::Removed;
	});
}

#pragma mark Type-Specific Access

CFStringRef SFB::Audio::Metadata::GetStringValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(nullptr == value)
		return nullptr;
	
	if(CFStringGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFStringRef)value;
}

CFNumberRef SFB::Audio::Metadata::GetNumberValue(CFStringRef key) const
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

CFTypeRef SFB::Audio::Metadata::GetValue(CFStringRef key) const
{
	if(nullptr == key)
		return nullptr;
	
	if(CFDictionaryContainsKey(mChangedMetadata, key)) {
		CFTypeRef value = CFDictionaryGetValue(mChangedMetadata, key);
		return (kCFNull == value ? nullptr : value);
	}

	return CFDictionaryGetValue(mMetadata, key);
}

void SFB::Audio::Metadata::SetValue(CFStringRef key, CFTypeRef value)
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

void SFB::Audio::Metadata::ClearAllMetadata()
{
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	mPictures.clear();
}

void SFB::Audio::Metadata::MergeChangedMetadataIntoMetadata()
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

	auto iter = std::begin(mPictures);
	while(iter != std::end(mPictures)) {
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
