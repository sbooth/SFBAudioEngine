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

#include <stdexcept>

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif

#include "AudioMetadata.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

#if !TARGET_OS_IPHONE
# include "FLACMetadata.h"
# include "WavPackMetadata.h"
# include "MP3Metadata.h"
# include "MP4Metadata.h"
# include "WAVEMetadata.h"
# include "AIFFMetadata.h"
# include "MusepackMetadata.h"
# include "OggVorbisMetadata.h"
# include "OggFLACMetadata.h"
# include "MonkeysAudioMetadata.h"
# include "OggSpeexMetadata.h"
# include "MODMetadata.h"
# include "TrueAudioMetadata.h"
#endif

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

#pragma mark Helper Classes

// ============================================================
// Class PointerIdentityComparator
template <class T>
class PointerIdentityComparator : public std::unary_function<T *, bool>
{
public:
	inline explicit PointerIdentityComparator(T *value)
		: mValue(value) 
	{}
	
	inline bool operator() (const T *value) const
	{
		return mValue == value;
	}
	
private:
	T *mValue;
};

// ============================================================
// Class AttachedPictureTypeComparator
class AttachedPictureTypeComparator : public std::unary_function<AttachedPicture *, bool>
{
public:
	inline explicit AttachedPictureTypeComparator(AttachedPicture::Type type)
		: mType(type) 
	{}
	
	inline bool operator() (const AttachedPicture *picture) const
	{
		return mType == picture->GetType();
	}
	
private:
	AttachedPicture::Type mType;
};

#pragma mark Static Methods

CFArrayRef AudioMetadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderExtensions = nullptr;

#if !TARGET_OS_IPHONE
	decoderExtensions = FLACMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = WavPackMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = MP3Metadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = MP4Metadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;
	
	decoderExtensions = WAVEMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = AIFFMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = MusepackMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = OggVorbisMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = OggFLACMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = MonkeysAudioMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;
	
	decoderExtensions = OggSpeexMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = MODMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;

	decoderExtensions = TrueAudioMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = nullptr;
#endif

	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedExtensions);
	
	CFRelease(supportedExtensions), supportedExtensions = nullptr;
	
	return result;
}

CFArrayRef AudioMetadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderMIMETypes = nullptr;

#if !TARGET_OS_IPHONE
	decoderMIMETypes = FLACMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = WavPackMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = MP3Metadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = MP4Metadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;
	
	decoderMIMETypes = WAVEMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = AIFFMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = MusepackMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = OggVorbisMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = OggFLACMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;
	
	decoderMIMETypes = MonkeysAudioMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = OggSpeexMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = MODMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;

	decoderMIMETypes = TrueAudioMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = nullptr;
#endif

	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = nullptr;
	
	return result;
}

bool AudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	CFArrayRef supportedExtensions = CreateSupportedFileExtensions();
	if(nullptr == supportedExtensions)
		return false;
	
	bool extensionIsSupported = false;
	
	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedExtensions, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive)) {
			extensionIsSupported = true;
			break;
		}
	}
	
	CFRelease(supportedExtensions), supportedExtensions = nullptr;
	
	return extensionIsSupported;
}

bool AudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	CFArrayRef supportedMIMETypes = CreateSupportedMIMETypes();
	if(nullptr == supportedMIMETypes)
		return false;
	
	bool mimeTypeIsSupported = false;
	
	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		CFStringRef currentMIMEType = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(mimeType, currentMIMEType, kCFCompareCaseInsensitive)) {
			mimeTypeIsSupported = true;
			break;
		}
	}
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = nullptr;
	
	return mimeTypeIsSupported;
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
		CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));
		
		if(fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				CFStringRef fileSystemPath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
				CFStringRef pathExtension = nullptr;

				CFRange range;
				if(CFStringFindWithOptionsAndLocale(fileSystemPath, CFSTR("."), CFRangeMake(0, CFStringGetLength(fileSystemPath)), kCFCompareBackwards, CFLocaleGetSystem(), &range)) {
					pathExtension = CFStringCreateWithSubstring(kCFAllocatorDefault, fileSystemPath, CFRangeMake(range.location + 1, CFStringGetLength(fileSystemPath) - range.location - 1));
				}

				CFRelease(fileSystemPath), fileSystemPath = nullptr;

				if(pathExtension) {
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

					// As a factory this class has knowledge of its subclasses
					// It would be possible (and perhaps preferable) to switch to a generic
					// plugin interface at a later date
#if !TARGET_OS_IPHONE
					if(FLACMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new FLACMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && WavPackMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new WavPackMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && MP3Metadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MP3Metadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && MP4Metadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MP4Metadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && WAVEMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new WAVEMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && AIFFMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new AIFFMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && MusepackMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MusepackMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && OggVorbisMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggVorbisMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && OggFLACMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggFLACMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && MonkeysAudioMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MonkeysAudioMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && OggSpeexMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggSpeexMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && MODMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MODMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
					if(!metadata && TrueAudioMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new TrueAudioMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = nullptr;
					}
#endif

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
	mURL = static_cast<CFURLRef>(CFRetain(url));
}

AudioMetadata::~AudioMetadata()
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = nullptr;

	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = nullptr;

	auto iter = mPictures.begin();
	while(iter != mPictures.end()) {
		AttachedPicture *picture = *iter;
		iter = mPictures.erase(iter);
		delete picture;
	}
//	for(auto picture : mPictures)
//		delete picture;
	mPictures.clear();
}

void AudioMetadata::SetURL(CFURLRef URL)
{
	if(mURL)
		CFRelease(mURL), mURL = nullptr;

	if(URL)
		mURL = static_cast<CFURLRef>(CFRetain(URL));
}

#pragma mark Change management

bool AudioMetadata::HasUnsavedChanges() const
{
	if(CFDictionaryGetCount(mChangedMetadata))
		return true;

	for(auto picture : mPictures) {
		// TODO: Remove this check once std::shared_ptr is used (because no pictures with this state should be present in mPictures)
		if(AttachedPicture::ChangeState::Added & picture->mState && AttachedPicture::ChangeState::Removed & picture->mState)
			continue;

		if(AttachedPicture::ChangeState::Added & picture->mState || AttachedPicture::ChangeState::Removed & picture->mState || picture->HasUnsavedChanges())
			return true;
	}

	return false;
}

void AudioMetadata::RevertUnsavedChanges()
{
	CFDictionaryRemoveAllValues(mChangedMetadata);

	auto iter = mPictures.begin();
	while(iter != mPictures.end()) {
		AttachedPicture *picture = *iter;
		if(AttachedPicture::ChangeState::Removed & picture->mState) {
			if(AttachedPicture::ChangeState::Added & picture->mState) {
				iter = mPictures.erase(iter);
				delete picture;
			}
			else {
				picture->mState &= ~AttachedPicture::ChangeState::Removed;
				picture->RevertUnsavedChanges();
			}
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
		return reinterpret_cast<CFBooleanRef>(value);
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
		return reinterpret_cast<CFDictionaryRef>(value);
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

const std::vector<AttachedPicture *> AudioMetadata::GetAttachedPictures() const
{
	std::vector<AttachedPicture *> result;

	for(auto picture : mPictures) {
		if(!(AttachedPicture::ChangeState::Removed & picture->mState))
			result.push_back(picture);
	}

	return result;
}

const std::vector<AttachedPicture *> AudioMetadata::GetAttachedPicturesOfType(AttachedPicture::Type type) const
{
	std::vector<AttachedPicture *> result;

	for(auto picture : mPictures) {
		if(!(AttachedPicture::ChangeState::Removed & picture->mState) && type == picture->GetType())
			result.push_back(picture);
	}

	return result;
}

void AudioMetadata::AttachPicture(AttachedPicture *picture)
{
	if(picture) {
		auto match = std::find_if(mPictures.begin(), mPictures.end(), PointerIdentityComparator<AttachedPicture>(picture));
		if(match != mPictures.end()) {
			if(AttachedPicture::ChangeState::Removed & picture->mState)
				picture->mState &= ~AttachedPicture::ChangeState::Removed;
		}
		else {
			picture->mState = AttachedPicture::Added;
			mPictures.push_back(picture);
		}
	}
}

void AudioMetadata::RemoveAttachedPicture(AttachedPicture *picture)
{
	if(picture) {
//		auto match = std::find_if(mPictures.begin(), mPictures.end(), [] (AttachedPicture *p) -> bool { p == picture; });
		auto match = std::find_if(mPictures.begin(), mPictures.end(), PointerIdentityComparator<AttachedPicture>(picture));
		if(match != mPictures.end())
			(*match)->mState |= AttachedPicture::ChangeState::Removed;

#if 0
		// TODO: It would be more correct to remove picture from mPictures if the state is Added 
		// but that necessitates std::shared_ptr since picture can't be immediately deleted (in use by the caller)
		if(match != mPictures.end()) {
			if((*match)->mState & AttachedPicture::ChangeState::Added)
				mPictures.erase(match);
			else
				(*match)->mState |= AttachedPicture::ChangeState::Removed;
		}
#endif
	}
}

void AudioMetadata::RemoveAttachedPicturesOfType(AttachedPicture::Type type)
{
	for(auto picture : mPictures) {
		if(type == picture->GetType())
			picture->mState |= AttachedPicture::ChangeState::Removed;
	}
}

void AudioMetadata::RemoveAllAttachedPictures()
{
//	std::for_each(mPictures.begin(), mPictures.end(), [] (AttachedPicture *picture){ picture->mState |= AttachedPicture::ChangeState::Removed; });
	for(auto picture : mPictures)
		picture->mState |= AttachedPicture::ChangeState::Removed;
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
		return reinterpret_cast<CFStringRef>(value);
}

CFNumberRef AudioMetadata::GetNumberValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(nullptr == value)
		return nullptr;

	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return nullptr;
	else
		return reinterpret_cast<CFNumberRef>(value);
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

void AudioMetadata::AddSavedPicture(AttachedPicture *picture)
{
	if(picture) {
		auto match = std::find_if(mPictures.begin(), mPictures.end(), PointerIdentityComparator<AttachedPicture>(picture));
		if(match == mPictures.end()) {
			picture->mState = AttachedPicture::Saved;
			mPictures.push_back(picture);
		}
	}
}

void AudioMetadata::MergeChangedMetadataIntoMetadata()
{
	CFIndex count = CFDictionaryGetCount(mChangedMetadata);
	
	CFTypeRef *keys = static_cast<CFTypeRef *>(malloc(sizeof(CFTypeRef) * count));
	CFTypeRef *values = static_cast<CFTypeRef *>(malloc(sizeof(CFTypeRef) * count));
	
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
		AttachedPicture *picture = *iter;
		if(AttachedPicture::ChangeState::Removed & picture->mState) {
			iter = mPictures.erase(iter);
			delete picture;
		}
		else {
			picture->MergeChangedMetadataIntoMetadata();
			picture->mState = AttachedPicture::ChangeState::Saved;
			++iter;
		}
	}
}
