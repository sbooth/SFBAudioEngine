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

#include <stdexcept>

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif

#include "AudioMetadata.h"
#include "CreateDisplayNameForURL.h"
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
const CFStringRef	kMetadataMusicBrainzAlbumIDKey			= CFSTR("MusicBrainz Album ID");
const CFStringRef	kMetadataMusicBrainzTrackIDKey			= CFSTR("MusicBrainz Track ID");
const CFStringRef	kMetadataAdditionalMetadataKey			= CFSTR("Additional Metadata");
const CFStringRef	kReplayGainReferenceLoudnessKey			= CFSTR("Replay Gain Reference Loudness");
const CFStringRef	kReplayGainTrackGainKey					= CFSTR("Replay Gain Track Gain");
const CFStringRef	kReplayGainTrackPeakKey					= CFSTR("Replay Gain Track Peak");
const CFStringRef	kReplayGainAlbumGainKey					= CFSTR("Replay Gain Album Gain");
const CFStringRef	kReplayGainAlbumPeakKey					= CFSTR("Replay Gain Album Peak");
const CFStringRef	kAlbumArtFrontCoverKey					= CFSTR("Album Art (Front Cover)");


#pragma mark Static Methods


CFArrayRef AudioMetadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderExtensions = NULL;

#if !TARGET_OS_IPHONE
	decoderExtensions = FLACMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = WavPackMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MP3Metadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MP4Metadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
	
	decoderExtensions = WAVEMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = AIFFMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MusepackMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = OggVorbisMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = OggFLACMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MonkeysAudioMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
	
	decoderExtensions = OggSpeexMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MODMetadata::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
#endif

	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedExtensions);
	
	CFRelease(supportedExtensions), supportedExtensions = NULL;
	
	return result;
}

CFArrayRef AudioMetadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderMIMETypes = NULL;

#if !TARGET_OS_IPHONE
	decoderMIMETypes = FLACMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = WavPackMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = MP3Metadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = MP4Metadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = WAVEMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = AIFFMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = MusepackMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = OggVorbisMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = OggFLACMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = MonkeysAudioMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = OggSpeexMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

	decoderMIMETypes = MODMetadata::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
#endif

	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return result;
}

bool AudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(NULL == extension)
		return false;
	
	CFArrayRef supportedExtensions = CreateSupportedFileExtensions();
	if(NULL == supportedExtensions)
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
	
	CFRelease(supportedExtensions), supportedExtensions = NULL;
	
	return extensionIsSupported;
}

bool AudioMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
	CFArrayRef supportedMIMETypes = CreateSupportedMIMETypes();
	if(NULL == supportedMIMETypes)
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
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return mimeTypeIsSupported;
}

AudioMetadata * AudioMetadata::CreateMetadataForURL(CFURLRef url, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;
	
	AudioMetadata *metadata = NULL;
	
	// If this is a file URL, use the extension-based resolvers
	CFStringRef scheme = CFURLCopyScheme(url);

	// If there is no scheme the URL is invalid
	if(NULL == scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, NULL);
		return NULL;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));
		
		if(NULL != fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				CFStringRef fileSystemPath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
				CFStringRef pathExtension = NULL;

				CFRange range;
				if(CFStringFindWithOptionsAndLocale(fileSystemPath, CFSTR("."), CFRangeMake(0, CFStringGetLength(fileSystemPath)), kCFCompareBackwards, CFLocaleGetSystem(), &range)) {
					pathExtension = CFStringCreateWithSubstring(kCFAllocatorDefault, fileSystemPath, CFRangeMake(range.location + 1, CFStringGetLength(fileSystemPath) - range.location - 1));
				}

				CFRelease(fileSystemPath), fileSystemPath = NULL;

				if(NULL != pathExtension) {
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

					// As a factory this class has knowledge of its subclasses
					// It would be possible (and perhaps preferable) to switch to a generic
					// plugin interface at a later date
#if !TARGET_OS_IPHONE
					if(FLACMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new FLACMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && WavPackMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new WavPackMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && MP3Metadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MP3Metadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && MP4Metadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MP4Metadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && WAVEMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new WAVEMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && AIFFMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new AIFFMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && MusepackMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MusepackMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && OggVorbisMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggVorbisMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && OggFLACMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggFLACMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && MonkeysAudioMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MonkeysAudioMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && OggSpeexMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new OggSpeexMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
					if(!metadata && MODMetadata::HandlesFilesWithExtension(pathExtension)) {
						metadata = new MODMetadata(url);
						if(!metadata->ReadMetadata(error))
							delete metadata, metadata = NULL;
					}
#endif

					CFRelease(pathExtension), pathExtension = NULL;
				}				
			}
			else {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "The requested URL doesn't exist");
				
				if(error) {
					CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																					   0,
																					   &kCFTypeDictionaryKeyCallBacks,
																					   &kCFTypeDictionaryValueCallBacks);
					
					CFStringRef displayName = CreateDisplayNameForURL(url);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” does not exist."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("File not found"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file may exist on removable media or may have been deleted."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					*error = CFErrorCreate(kCFAllocatorDefault, 
										   AudioMetadataErrorDomain, 
										   AudioMetadataInputOutputError, 
										   errorDictionary);
					
					CFRelease(errorDictionary), errorDictionary = NULL;				
				}				
			}
		}
		else
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "CFURLCreatePropertyFromResource failed: " << errorCode);
		
		CFRelease(fileExists), fileExists = NULL;
	}
#if !TARGET_OS_IPHONE
	// Determine the MIME type for the URL
	else {
		// Get the UTI for this URL
		FSRef ref;
		Boolean success = CFURLGetFSRef(url, &ref);
		if(!success) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "Unable to get FSRef for URL");
			return NULL;
		}
		
		CFStringRef uti = NULL;
		OSStatus result = LSCopyItemAttribute(&ref, kLSRolesAll, kLSItemContentType, (CFTypeRef *)&uti);
		
		if(noErr != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata", "LSCopyItemAttribute (kLSItemContentType) failed: " << result);
			return NULL;
		}
		
		CFRelease(uti), uti = NULL;
	}
#endif

	CFRelease(scheme), scheme = NULL;
	
	return metadata;
}

#pragma mark Creation and Destruction

AudioMetadata::AudioMetadata()
	: mURL(NULL)
{
	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
										  0,
										  &kCFTypeDictionaryKeyCallBacks,
										  &kCFTypeDictionaryValueCallBacks);

	mChangedMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
												 0,
												 &kCFTypeDictionaryKeyCallBacks,
												 &kCFTypeDictionaryValueCallBacks);
}

AudioMetadata::AudioMetadata(CFURLRef url)
	: mURL(NULL)
{
	mURL = static_cast<CFURLRef>(CFRetain(url));

	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
										  0,
										  &kCFTypeDictionaryKeyCallBacks,
										  &kCFTypeDictionaryValueCallBacks);

	mChangedMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
												 0,
												 &kCFTypeDictionaryKeyCallBacks,
												 &kCFTypeDictionaryValueCallBacks);
}

AudioMetadata::AudioMetadata(const AudioMetadata& rhs)
	: mURL(NULL)
{
	*this = rhs;
}

AudioMetadata::~AudioMetadata()
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = NULL;

	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = NULL;
}

#pragma mark Operator Overloads

AudioMetadata& AudioMetadata::operator=(const AudioMetadata& rhs)
{
	if(this == &rhs)
		return *this;

	if(mURL)
		CFRelease(mURL), mURL = NULL;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = NULL;

	if(mChangedMetadata)
		CFRelease(mChangedMetadata), mChangedMetadata = NULL;

	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));

	if(rhs.mMetadata)
		mMetadata = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
												  0, 
												  rhs.mMetadata);

	if(rhs.mChangedMetadata)
		mChangedMetadata = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
														 0, 
														 rhs.mMetadata);
	
	return *this;
}

void AudioMetadata::SetURL(CFURLRef URL)
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;

	if(URL)
		mURL = static_cast<CFURLRef>(CFRetain(URL));
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
	
	if(NULL == value)
		return NULL;

	if(CFBooleanGetTypeID() != CFGetTypeID(value))
		return NULL;
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

CFStringRef AudioMetadata::GetMusicBrainzAlbumID() const
{
	return GetStringValue(kMetadataMusicBrainzAlbumIDKey);
}

void AudioMetadata::SetMusicBrainzAlbumID(CFStringRef albumID)
{
	SetValue(kMetadataMusicBrainzAlbumIDKey, albumID);
}

CFStringRef AudioMetadata::GetMusicBrainzTrackID() const
{
	return GetStringValue(kMetadataMusicBrainzTrackIDKey);
}

void AudioMetadata::SetMusicBrainzTrackID(CFStringRef trackID)
{
	SetValue(kMetadataMusicBrainzTrackIDKey, trackID);
}

#pragma mark Additional Metadata

CFDictionaryRef AudioMetadata::GetAdditionalMetadata() const
{
	CFTypeRef value = GetValue(kMetadataAdditionalMetadataKey);
	
	if(NULL == value)
		return NULL;

	if(CFDictionaryGetTypeID() != CFGetTypeID(value))
		return NULL;
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

CFDataRef AudioMetadata::GetFrontCoverArt() const
{
	CFTypeRef value = GetValue(kAlbumArtFrontCoverKey);
	
	if(NULL == value)
		return NULL;

	if(CFDataGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFDataRef>(value);
}

void AudioMetadata::SetFrontCoverArt(CFDataRef frontCoverArt)
{
	SetValue(kAlbumArtFrontCoverKey, frontCoverArt);
}

#pragma mark Type-Specific Access

CFStringRef AudioMetadata::GetStringValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(NULL == value)
		return NULL;
	
	if(CFStringGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFStringRef>(value);
}

CFNumberRef AudioMetadata::GetNumberValue(CFStringRef key) const
{
	CFTypeRef value = GetValue(key);
	
	if(NULL == value)
		return NULL;

	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFNumberRef>(value);
}

#pragma mark Generic Access

CFTypeRef AudioMetadata::GetValue(CFStringRef key) const
{
	if(NULL == key)
		return NULL;
	
	if(CFDictionaryContainsKey(mChangedMetadata, key)) {
		CFTypeRef value = CFDictionaryGetValue(mChangedMetadata, key);
		return (kCFNull == value ? NULL : value);
	}

	return CFDictionaryGetValue(mMetadata, key);
}

void AudioMetadata::SetValue(CFStringRef key, CFTypeRef value)
{
	if(NULL == key)
		return;

	if(NULL == value) {
		if(CFDictionaryContainsKey(mMetadata, key))
			CFDictionarySetValue(mChangedMetadata, key, kCFNull);
		else
			CFDictionaryRemoveValue(mChangedMetadata, key);
	}
	else {
		if(CFDictionaryContainsKey(mChangedMetadata, key)) {
			CFTypeRef savedValue = CFDictionaryGetValue(mMetadata, key);
			if(NULL != savedValue && CFEqual(savedValue, value))
				CFDictionaryRemoveValue(mChangedMetadata, key);
			else
				CFDictionarySetValue(mChangedMetadata, key, value);
		}
		else
			CFDictionarySetValue(mChangedMetadata, key, value);
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
	
	free(keys), keys = NULL;
	free(values), values = NULL;
	
	CFDictionaryRemoveAllValues(mChangedMetadata);
}
