/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
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
const CFStringRef SFB::Audio::Metadata::ErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioMetadata");

// ========================================
// Key names for the metadata dictionary
// ========================================
const CFStringRef SFB::Audio::Metadata::kFormatNameKey					= CFSTR("Format Name");
const CFStringRef SFB::Audio::Metadata::kTotalFramesKey					= CFSTR("Total Frames");
const CFStringRef SFB::Audio::Metadata::kChannelsPerFrameKey			= CFSTR("Channels Per Frame");
const CFStringRef SFB::Audio::Metadata::kBitsPerChannelKey				= CFSTR("Bits Per Channel");
const CFStringRef SFB::Audio::Metadata::kSampleRateKey					= CFSTR("Sample Rate");
const CFStringRef SFB::Audio::Metadata::kDurationKey					= CFSTR("Duration");
const CFStringRef SFB::Audio::Metadata::kBitrateKey						= CFSTR("Bitrate");

const CFStringRef SFB::Audio::Metadata::kTitleKey						= CFSTR("Title");
const CFStringRef SFB::Audio::Metadata::kAlbumTitleKey					= CFSTR("Album Title");
const CFStringRef SFB::Audio::Metadata::kArtistKey						= CFSTR("Artist");
const CFStringRef SFB::Audio::Metadata::kAlbumArtistKey					= CFSTR("Album Artist");
const CFStringRef SFB::Audio::Metadata::kGenreKey						= CFSTR("Genre");
const CFStringRef SFB::Audio::Metadata::kComposerKey					= CFSTR("Composer");
const CFStringRef SFB::Audio::Metadata::kReleaseDateKey					= CFSTR("Date");
const CFStringRef SFB::Audio::Metadata::kCompilationKey					= CFSTR("Compilation");
const CFStringRef SFB::Audio::Metadata::kTrackNumberKey					= CFSTR("Track Number");
const CFStringRef SFB::Audio::Metadata::kTrackTotalKey					= CFSTR("Track Total");
const CFStringRef SFB::Audio::Metadata::kDiscNumberKey					= CFSTR("Disc Number");
const CFStringRef SFB::Audio::Metadata::kDiscTotalKey					= CFSTR("Disc Total");
const CFStringRef SFB::Audio::Metadata::kLyricsKey						= CFSTR("Lyrics");
const CFStringRef SFB::Audio::Metadata::kBPMKey							= CFSTR("BPM");
const CFStringRef SFB::Audio::Metadata::kRatingKey						= CFSTR("Rating");
const CFStringRef SFB::Audio::Metadata::kCommentKey						= CFSTR("Comment");
const CFStringRef SFB::Audio::Metadata::kISRCKey						= CFSTR("ISRC");
const CFStringRef SFB::Audio::Metadata::kMCNKey							= CFSTR("MCN");
const CFStringRef SFB::Audio::Metadata::kMusicBrainzReleaseIDKey		= CFSTR("MusicBrainz Release ID");
const CFStringRef SFB::Audio::Metadata::kMusicBrainzRecordingIDKey		= CFSTR("MusicBrainz Recording ID");

const CFStringRef SFB::Audio::Metadata::kTitleSortOrderKey				= CFSTR("Title Sort Order");
const CFStringRef SFB::Audio::Metadata::kAlbumTitleSortOrderKey			= CFSTR("Album Title Sort Order");
const CFStringRef SFB::Audio::Metadata::kArtistSortOrderKey				= CFSTR("Artist Sort Order");
const CFStringRef SFB::Audio::Metadata::kAlbumArtistSortOrderKey		= CFSTR("Album Artist Sort Order");
const CFStringRef SFB::Audio::Metadata::kComposerSortOrderKey			= CFSTR("Composer Sort Order");

const CFStringRef SFB::Audio::Metadata::kGroupingKey					= CFSTR("Grouping");

const CFStringRef SFB::Audio::Metadata::kAdditionalMetadataKey			= CFSTR("Additional Metadata");

const CFStringRef SFB::Audio::Metadata::kReferenceLoudnessKey			= CFSTR("Replay Gain Reference Loudness");
const CFStringRef SFB::Audio::Metadata::kTrackGainKey					= CFSTR("Replay Gain Track Gain");
const CFStringRef SFB::Audio::Metadata::kTrackPeakKey					= CFSTR("Replay Gain Track Peak");
const CFStringRef SFB::Audio::Metadata::kAlbumGainKey					= CFSTR("Replay Gain Album Gain");
const CFStringRef SFB::Audio::Metadata::kAlbumPeakKey					= CFSTR("Replay Gain Album Peak");

const CFStringRef SFB::Audio::Metadata::kAttachedPicturesKey			= CFSTR("Attached Pictures");

#pragma mark Static Methods

std::vector<SFB::Audio::Metadata::SubclassInfo> SFB::Audio::Metadata::sRegisteredSubclasses;

CFArrayRef SFB::Audio::Metadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedFileExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderFileExtensions(subclassInfo.mCreateSupportedFileExtensions());
		CFArrayAppendArray(supportedFileExtensions, decoderFileExtensions, CFRangeMake(0, CFArrayGetCount(decoderFileExtensions)));
	}

	return supportedFileExtensions;
}

CFArrayRef SFB::Audio::Metadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderMIMETypes(subclassInfo.mCreateSupportedMIMETypes());
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
	SFB::CFString scheme(CFURLCopyScheme(url));

	// If there is no scheme the URL is invalid
	if(!scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, nullptr);
		return nullptr;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		SFB::CFBoolean fileExists((CFBooleanRef)CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));

		if(fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				SFB::CFString pathExtension(CFURLCopyPathExtension(url));
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
					SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” does not exist."), ""));
					SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File not found"), ""));
					SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file may exist on removable media or may have been deleted."), ""));

					*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, url, failureReason, recoverySuggestion);
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
	: mURL(nullptr), mMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), mChangedMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)
{}

SFB::Audio::Metadata::Metadata(CFURLRef url)
	: Metadata()
{
	mURL = (CFURLRef)CFRetain(url);
}

void SFB::Audio::Metadata::SetURL(CFURLRef URL)
{
	mURL = URL ? (CFURLRef)CFRetain(URL) : nullptr;
}

bool SFB::Audio::Metadata::ReadMetadata(CFErrorRef *error)
{
	ClearAllMetadata();
	return _ReadMetadata(error);
}

bool SFB::Audio::Metadata::WriteMetadata(CFErrorRef *error)
{
	bool result = _WriteMetadata(error);
	if(result)
		MergeChangedMetadataIntoMetadata();
	return result;
}

#pragma mark External Representations

CFDictionaryRef SFB::Audio::Metadata::CreateDictionaryRepresentation() const
{
	CFMutableDictionaryRef dictionaryRepresentation = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, mMetadata);

	CFIndex count = CFDictionaryGetCount(mChangedMetadata);

	CFTypeRef *keys = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);
	CFTypeRef *values = (CFTypeRef *)malloc(sizeof(CFTypeRef) * (size_t)count);

	CFDictionaryGetKeysAndValues(mChangedMetadata, keys, values);

	for(CFIndex i = 0; i < count; ++i) {
		if(kCFNull == values[i])
			CFDictionaryRemoveValue(dictionaryRepresentation, keys[i]);
		else
			CFDictionarySetValue(dictionaryRepresentation, keys[i], values[i]);
	}

	free(keys), keys = nullptr;
	free(values), values = nullptr;

	CFMutableArray pictureArray(0, &kCFTypeArrayCallBacks);

	for(auto picture : GetAttachedPictures()) {
		CFDictionary pictureRepresentation(picture->CreateDictionaryRepresentation());
		CFArrayAppendValue(pictureArray, pictureRepresentation);
	}

	if(0 < CFArrayGetCount(pictureArray)) {
		CFDictionarySetValue(dictionaryRepresentation, kAttachedPicturesKey, pictureArray);
	}

	return dictionaryRepresentation;
}

bool SFB::Audio::Metadata::SetFromDictionaryRepresentation(CFDictionaryRef dictionary)
{
	if(nullptr == dictionary)
		return false;

	SetValue(kTitleKey, CFDictionaryGetValue(dictionary, kTitleKey));
	SetValue(kAlbumTitleKey, CFDictionaryGetValue(dictionary, kAlbumTitleKey));
	SetValue(kArtistKey, CFDictionaryGetValue(dictionary, kArtistKey));
	SetValue(kAlbumArtistKey, CFDictionaryGetValue(dictionary, kAlbumArtistKey));
	SetValue(kGenreKey, CFDictionaryGetValue(dictionary, kGenreKey));
	SetValue(kComposerKey, CFDictionaryGetValue(dictionary, kComposerKey));
	SetValue(kReleaseDateKey, CFDictionaryGetValue(dictionary, kReleaseDateKey));
	SetValue(kCompilationKey, CFDictionaryGetValue(dictionary, kCompilationKey));
	SetValue(kTrackNumberKey, CFDictionaryGetValue(dictionary, kTrackNumberKey));
	SetValue(kTrackTotalKey, CFDictionaryGetValue(dictionary, kTrackTotalKey));
	SetValue(kDiscNumberKey, CFDictionaryGetValue(dictionary, kDiscNumberKey));
	SetValue(kDiscTotalKey, CFDictionaryGetValue(dictionary, kDiscTotalKey));
	SetValue(kLyricsKey, CFDictionaryGetValue(dictionary, kLyricsKey));
	SetValue(kBPMKey, CFDictionaryGetValue(dictionary, kBPMKey));
	SetValue(kRatingKey, CFDictionaryGetValue(dictionary, kRatingKey));
	SetValue(kCommentKey, CFDictionaryGetValue(dictionary, kCommentKey));
	SetValue(kISRCKey, CFDictionaryGetValue(dictionary, kISRCKey));
	SetValue(kMCNKey, CFDictionaryGetValue(dictionary, kMCNKey));
	SetValue(kMusicBrainzReleaseIDKey, CFDictionaryGetValue(dictionary, kMusicBrainzReleaseIDKey));
	SetValue(kMusicBrainzRecordingIDKey, CFDictionaryGetValue(dictionary, kMusicBrainzRecordingIDKey));

	SetValue(kTitleSortOrderKey, CFDictionaryGetValue(dictionary, kTitleSortOrderKey));
	SetValue(kAlbumTitleSortOrderKey, CFDictionaryGetValue(dictionary, kAlbumTitleSortOrderKey));
	SetValue(kArtistSortOrderKey, CFDictionaryGetValue(dictionary, kArtistSortOrderKey));
	SetValue(kAlbumArtistSortOrderKey, CFDictionaryGetValue(dictionary, kAlbumArtistSortOrderKey));
	SetValue(kComposerSortOrderKey, CFDictionaryGetValue(dictionary, kComposerSortOrderKey));

	SetValue(kGroupingKey, CFDictionaryGetValue(dictionary, kGroupingKey));

	SetValue(kAdditionalMetadataKey, CFDictionaryGetValue(dictionary, kAdditionalMetadataKey));

	SetValue(kReferenceLoudnessKey, CFDictionaryGetValue(dictionary, kReferenceLoudnessKey));
	SetValue(kTrackGainKey, CFDictionaryGetValue(dictionary, kTrackGainKey));
	SetValue(kTrackPeakKey, CFDictionaryGetValue(dictionary, kTrackPeakKey));
	SetValue(kAlbumGainKey, CFDictionaryGetValue(dictionary, kAlbumGainKey));
	SetValue(kAlbumPeakKey, CFDictionaryGetValue(dictionary, kAlbumPeakKey));

	RemoveAllAttachedPictures();

	CFArrayRef attachedPictures = (CFArrayRef)CFDictionaryGetValue(dictionary, kAttachedPicturesKey);
	for(CFIndex i = 0; i < CFArrayGetCount(attachedPictures); ++i) {
		CFDictionaryRef pictureDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(attachedPictures, i);

		CFDataRef pictureData = (CFDataRef)CFDictionaryGetValue(pictureDictionary, AttachedPicture::kDataKey);

		AttachedPicture::Type pictureType = AttachedPicture::Type::Other;
		CFNumberRef typeWrapper = (CFNumberRef)CFDictionaryGetValue(pictureDictionary, AttachedPicture::kTypeKey);
		if(nullptr != typeWrapper)
			CFNumberGetValue(typeWrapper, kCFNumberIntType, &pictureType);

		CFStringRef pictureDescription = (CFStringRef)CFDictionaryGetValue(pictureDictionary, AttachedPicture::kDescriptionKey);

		AttachPicture(std::make_shared<AttachedPicture>(pictureData, pictureType, pictureDescription));
	}

	return true;
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

	for(auto picture : mPictures) {
		if(AttachedPicture::ChangeState::Removed == picture->mState)
			picture->mState = AttachedPicture::ChangeState::Saved;
		picture->RevertUnsavedChanges();
	}
}

#pragma mark Metadata manipulation

void SFB::Audio::Metadata::CopyMetadata(const Metadata& metadata, unsigned kind)
{
	if(Basic & kind) {
		SetValue(kTitleKey, metadata.GetTitle());
		SetValue(kAlbumTitleKey, metadata.GetAlbumTitle());
		SetValue(kArtistKey, metadata.GetArtist());
		SetValue(kAlbumArtistKey, metadata.GetAlbumArtist());
		SetValue(kGenreKey, metadata.GetGenre());
		SetValue(kComposerKey, metadata.GetComposer());
		SetValue(kReleaseDateKey, metadata.GetReleaseDate());
		SetValue(kCompilationKey, metadata.GetCompilation());
		SetValue(kTrackNumberKey, metadata.GetTrackNumber());
		SetValue(kTrackTotalKey, metadata.GetTrackTotal());
		SetValue(kDiscNumberKey, metadata.GetDiscNumber());
		SetValue(kDiscTotalKey, metadata.GetDiscTotal());
		SetValue(kLyricsKey, metadata.GetLyrics());
		SetValue(kBPMKey, metadata.GetBPM());
		SetValue(kRatingKey, metadata.GetRating());
		SetValue(kCommentKey, metadata.GetComment());
		SetValue(kISRCKey, metadata.GetISRC());
		SetValue(kMCNKey, metadata.GetMCN());
		SetValue(kMusicBrainzReleaseIDKey, metadata.GetMusicBrainzReleaseID());
		SetValue(kMusicBrainzRecordingIDKey, metadata.GetMusicBrainzRecordingID());
	}

	if(Sorting & kind) {
		SetValue(kTitleSortOrderKey, metadata.GetTitleSortOrder());
		SetValue(kAlbumTitleSortOrderKey, metadata.GetAlbumTitleSortOrder());
		SetValue(kArtistSortOrderKey, metadata.GetArtistSortOrder());
		SetValue(kAlbumArtistSortOrderKey, metadata.GetAlbumArtistSortOrder());
		SetValue(kComposerSortOrderKey, metadata.GetComposer());
	}

	if(Grouping & kind)
		SetValue(kGroupingKey, metadata.GetGrouping());

	if(Additional & kind)
		SetValue(kAdditionalMetadataKey, metadata.GetAdditionalMetadata());

	if(ReplayGain & kind) {
		SetValue(kReferenceLoudnessKey, metadata.GetReplayGainReferenceLoudness());
		SetValue(kTrackGainKey, metadata.GetReplayGainTrackGain());
		SetValue(kTrackPeakKey, metadata.GetReplayGainTrackPeak());
		SetValue(kAlbumGainKey, metadata.GetReplayGainAlbumGain());
		SetValue(kAlbumPeakKey, metadata.GetReplayGainAlbumPeak());
	}
}

void SFB::Audio::Metadata::CopyAllMetadata(const Metadata& metadata)
{
	CopyMetadata(metadata, Basic | Sorting | Grouping | Additional | ReplayGain);
}

void SFB::Audio::Metadata::RemoveMetadata(unsigned kind)
{
	if(Basic & kind) {
		SetValue(kTitleKey, nullptr);
		SetValue(kAlbumTitleKey, nullptr);
		SetValue(kArtistKey, nullptr);
		SetValue(kAlbumArtistKey, nullptr);
		SetValue(kGenreKey, nullptr);
		SetValue(kComposerKey, nullptr);
		SetValue(kReleaseDateKey, nullptr);
		SetValue(kCompilationKey, nullptr);
		SetValue(kTrackNumberKey, nullptr);
		SetValue(kTrackTotalKey, nullptr);
		SetValue(kDiscNumberKey, nullptr);
		SetValue(kDiscTotalKey, nullptr);
		SetValue(kLyricsKey, nullptr);
		SetValue(kBPMKey, nullptr);
		SetValue(kRatingKey, nullptr);
		SetValue(kCommentKey, nullptr);
		SetValue(kISRCKey, nullptr);
		SetValue(kMCNKey, nullptr);
		SetValue(kMusicBrainzReleaseIDKey, nullptr);
		SetValue(kMusicBrainzRecordingIDKey, nullptr);
	}

	if(Sorting & kind) {
		SetValue(kTitleSortOrderKey, nullptr);
		SetValue(kAlbumTitleSortOrderKey, nullptr);
		SetValue(kArtistSortOrderKey, nullptr);
		SetValue(kAlbumArtistSortOrderKey, nullptr);
		SetValue(kComposerSortOrderKey, nullptr);
	}

	if(Grouping & kind)
		SetValue(kGroupingKey, nullptr);

	if(Additional & kind)
		SetValue(kAdditionalMetadataKey, nullptr);

	if(ReplayGain & kind) {
		SetValue(kReferenceLoudnessKey, nullptr);
		SetValue(kTrackGainKey, nullptr);
		SetValue(kTrackPeakKey, nullptr);
		SetValue(kAlbumGainKey, nullptr);
		SetValue(kAlbumPeakKey, nullptr);
	}
}

void SFB::Audio::Metadata::RemoveAllMetadata()
{
	RemoveMetadata(Basic | Sorting | Grouping | Additional | ReplayGain);
}

#pragma mark Properties Access

CFStringRef SFB::Audio::Metadata::GetFormatName() const
{
	return GetStringValue(kFormatNameKey);
}

CFNumberRef SFB::Audio::Metadata::GetTotalFrames() const
{
	return GetNumberValue(kTotalFramesKey);
}

CFNumberRef SFB::Audio::Metadata::GetChannelsPerFrame() const
{
	return GetNumberValue(kChannelsPerFrameKey);
}

CFNumberRef SFB::Audio::Metadata::GetBitsPerChannel() const
{
	return GetNumberValue(kBitsPerChannelKey);
}

CFNumberRef SFB::Audio::Metadata::GetSampleRate() const
{
	return GetNumberValue(kSampleRateKey);
}

CFNumberRef SFB::Audio::Metadata::GetDuration() const
{
	return GetNumberValue(kDurationKey);
}

CFNumberRef SFB::Audio::Metadata::GetBitrate() const
{
	return GetNumberValue(kBitrateKey);
}

#pragma mark Metadata Access

CFStringRef SFB::Audio::Metadata::GetTitle() const
{
	return GetStringValue(kTitleKey);
}

void SFB::Audio::Metadata::SetTitle(CFStringRef title)
{
	SetValue(kTitleKey, title);
}

CFStringRef SFB::Audio::Metadata::GetAlbumTitle() const
{
	return GetStringValue(kAlbumTitleKey);
}

void SFB::Audio::Metadata::SetAlbumTitle(CFStringRef albumTitle)
{
	SetValue(kAlbumTitleKey, albumTitle);
}

CFStringRef SFB::Audio::Metadata::GetArtist() const
{
	return GetStringValue(kArtistKey);
}

void SFB::Audio::Metadata::SetArtist(CFStringRef artist)
{
	SetValue(kArtistKey, artist);
}

CFStringRef SFB::Audio::Metadata::GetAlbumArtist() const
{
	return GetStringValue(kAlbumArtistKey);
}

void SFB::Audio::Metadata::SetAlbumArtist(CFStringRef albumArtist)
{
	SetValue(kAlbumArtistKey, albumArtist);
}

CFStringRef SFB::Audio::Metadata::GetGenre() const
{
	return GetStringValue(kGenreKey);
}

void SFB::Audio::Metadata::SetGenre(CFStringRef genre)
{
	SetValue(kGenreKey, genre);
}

CFStringRef SFB::Audio::Metadata::GetComposer() const
{
	return GetStringValue(kComposerKey);
}

void SFB::Audio::Metadata::SetComposer(CFStringRef composer)
{
	SetValue(kComposerKey, composer);
}

CFStringRef SFB::Audio::Metadata::GetReleaseDate() const
{
	return GetStringValue(kReleaseDateKey);
}

void SFB::Audio::Metadata::SetReleaseDate(CFStringRef releaseDate)
{
	SetValue(kReleaseDateKey, releaseDate);
}

CFBooleanRef SFB::Audio::Metadata::GetCompilation() const
{
	CFTypeRef value = GetValue(kCompilationKey);

	if(nullptr == value)
		return nullptr;

	if(CFBooleanGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFBooleanRef)value;
}

void SFB::Audio::Metadata::SetCompilation(CFBooleanRef compilation)
{
	SetValue(kCompilationKey, compilation);
}

CFNumberRef SFB::Audio::Metadata::GetTrackNumber() const
{
	return GetNumberValue(kTrackNumberKey);
}

void SFB::Audio::Metadata::SetTrackNumber(CFNumberRef trackNumber)
{
	SetValue(kTrackNumberKey, trackNumber);
}

CFNumberRef SFB::Audio::Metadata::GetTrackTotal() const
{
	return GetNumberValue(kTrackTotalKey);
}

void SFB::Audio::Metadata::SetTrackTotal(CFNumberRef trackTotal)
{
	SetValue(kTrackTotalKey, trackTotal);
}

CFNumberRef SFB::Audio::Metadata::GetDiscNumber() const
{
	return GetNumberValue(kDiscNumberKey);
}

void SFB::Audio::Metadata::SetDiscNumber(CFNumberRef discNumber)
{
	SetValue(kDiscNumberKey, discNumber);
}

CFNumberRef SFB::Audio::Metadata::GetDiscTotal() const
{
	return GetNumberValue(kDiscTotalKey);
}

void SFB::Audio::Metadata::SetDiscTotal(CFNumberRef discTotal)
{
	SetValue(kDiscTotalKey, discTotal);
}

CFStringRef SFB::Audio::Metadata::GetLyrics() const
{
	return GetStringValue(kLyricsKey);
}

void SFB::Audio::Metadata::SetLyrics(CFStringRef lyrics)
{
	SetValue(kLyricsKey, lyrics);
}

CFNumberRef SFB::Audio::Metadata::GetBPM() const
{
	return GetNumberValue(kBPMKey);
}

void SFB::Audio::Metadata::SetBPM(CFNumberRef BPM)
{
	SetValue(kBPMKey, BPM);
}

CFNumberRef SFB::Audio::Metadata::GetRating() const
{
	return GetNumberValue(kRatingKey);
}

void SFB::Audio::Metadata::SetRating(CFNumberRef rating)
{
	SetValue(kRatingKey, rating);
}

CFStringRef SFB::Audio::Metadata::GetComment() const
{
	return GetStringValue(kCommentKey);
}

void SFB::Audio::Metadata::SetComment(CFStringRef comment)
{
	SetValue(kCommentKey, comment);
}

CFStringRef SFB::Audio::Metadata::GetMCN() const
{
	return GetStringValue(kMCNKey);
}

void SFB::Audio::Metadata::SetMCN(CFStringRef mcn)
{
	SetValue(kMCNKey, mcn);
}

CFStringRef SFB::Audio::Metadata::GetISRC() const
{
	return GetStringValue(kISRCKey);
}

void SFB::Audio::Metadata::SetISRC(CFStringRef isrc)
{
	SetValue(kISRCKey, isrc);
}

CFStringRef SFB::Audio::Metadata::GetMusicBrainzReleaseID() const
{
	return GetStringValue(kMusicBrainzReleaseIDKey);
}

void SFB::Audio::Metadata::SetMusicBrainzReleaseID(CFStringRef releaseID)
{
	SetValue(kMusicBrainzReleaseIDKey, releaseID);
}

CFStringRef SFB::Audio::Metadata::GetMusicBrainzRecordingID() const
{
	return GetStringValue(kMusicBrainzRecordingIDKey);
}

void SFB::Audio::Metadata::SetMusicBrainzRecordingID(CFStringRef recordingID)
{
	SetValue(kMusicBrainzRecordingIDKey, recordingID);
}

CFStringRef SFB::Audio::Metadata::GetTitleSortOrder() const
{
	return GetStringValue(kTitleSortOrderKey);
}

void SFB::Audio::Metadata::SetTitleSortOrder(CFStringRef titleSortOrder)
{
	SetValue(kTitleSortOrderKey, titleSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetAlbumTitleSortOrder() const
{
	return GetStringValue(kAlbumTitleSortOrderKey);
}

void SFB::Audio::Metadata::SetAlbumTitleSortOrder(CFStringRef albumTitleSortOrder)
{
	SetValue(kAlbumTitleSortOrderKey, albumTitleSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetArtistSortOrder() const
{
	return GetStringValue(kArtistSortOrderKey);
}

void SFB::Audio::Metadata::SetArtistSortOrder(CFStringRef artistSortOrder)
{
	SetValue(kArtistSortOrderKey, artistSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetAlbumArtistSortOrder() const
{
	return GetStringValue(kAlbumArtistSortOrderKey);
}

void SFB::Audio::Metadata::SetAlbumArtistSortOrder(CFStringRef albumArtistSortOrder)
{
	SetValue(kAlbumArtistSortOrderKey, albumArtistSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetComposerSortOrder() const
{
	return GetStringValue(kComposerSortOrderKey);
}

void SFB::Audio::Metadata::SetComposerSortOrder(CFStringRef composerSortOrder)
{
	SetValue(kComposerSortOrderKey, composerSortOrder);
}

CFStringRef SFB::Audio::Metadata::GetGrouping() const
{
	return GetStringValue(kGroupingKey);
}

void SFB::Audio::Metadata::SetGrouping(CFStringRef grouping)
{
	SetValue(kGroupingKey, grouping);
}

#pragma mark Additional Metadata

CFDictionaryRef SFB::Audio::Metadata::GetAdditionalMetadata() const
{
	CFTypeRef value = GetValue(kAdditionalMetadataKey);

	if(nullptr == value)
		return nullptr;

	if(CFDictionaryGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return (CFDictionaryRef)value;
}

void SFB::Audio::Metadata::SetAdditionalMetadata(CFDictionaryRef additionalMetadata)
{
	SetValue(kAdditionalMetadataKey, additionalMetadata);
}

#pragma mark Replay Gain Information

CFNumberRef SFB::Audio::Metadata::GetReplayGainReferenceLoudness() const
{
	return GetNumberValue(kReferenceLoudnessKey);
}

void SFB::Audio::Metadata::SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness)
{
	SetValue(kReferenceLoudnessKey, referenceLoudness);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainTrackGain() const
{
	return GetNumberValue(kTrackGainKey);
}

void SFB::Audio::Metadata::SetReplayGainTrackGain(CFNumberRef trackGain)
{
	SetValue(kTrackGainKey, trackGain);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainTrackPeak() const
{
	return GetNumberValue(kTrackPeakKey);
}

void SFB::Audio::Metadata::SetReplayGainTrackPeak(CFNumberRef trackPeak)
{
	SetValue(kTrackPeakKey, trackPeak);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainAlbumGain() const
{
	return GetNumberValue(kAlbumGainKey);
}

void SFB::Audio::Metadata::SetReplayGainAlbumGain(CFNumberRef albumGain)
{
	SetValue(kAlbumGainKey, albumGain);
}

CFNumberRef SFB::Audio::Metadata::GetReplayGainAlbumPeak() const
{
	return GetNumberValue(kAlbumPeakKey);
}

void SFB::Audio::Metadata::SetReplayGainAlbumPeak(CFNumberRef albumPeak)
{
	SetValue(kAlbumPeakKey, albumPeak);
}

#pragma mark Album Artwork

void SFB::Audio::Metadata::CopyAttachedPictures(const Metadata& metadata)
{
	RemoveAllAttachedPictures();
	for(auto picture : metadata.GetAttachedPictures())
		AttachPicture(std::make_shared<AttachedPicture>(picture->GetData(), picture->GetType(), picture->GetDescription()));
}

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
		CFTypeRef savedValue = CFDictionaryGetValue(mMetadata, key);
		// Revert the change if the new value is the save as the saved value
		if(CFDictionaryContainsKey(mChangedMetadata, key)) {
			if(savedValue && CFEqual(savedValue, value))
				CFDictionaryRemoveValue(mChangedMetadata, key);
			else
				CFDictionarySetValue(mChangedMetadata, key, value);
		}
		// If a saved value exists only register the change if the new value is different
		else if(savedValue && !CFEqual(savedValue, value))
			CFDictionarySetValue(mChangedMetadata, key, value);
		// If no saved value exists for the key register the change
		else if(nullptr == savedValue)
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
