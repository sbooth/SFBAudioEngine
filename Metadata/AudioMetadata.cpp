/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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
#include <CoreServices/CoreServices.h>

#include "AudioEngineDefines.h"
#include "AudioMetadata.h"
#include "FLACMetadata.h"
#include "WavPackMetadata.h"
#include "MP3Metadata.h"
#include "MP4Metadata.h"
#include "WAVEMetadata.h"
#include "AIFFMetadata.h"
#include "MusepackMetadata.h"
#include "OggVorbisMetadata.h"


// ========================================
// Error Codes
// ========================================
const CFStringRef	AudioMetadataErrorDomain				= CFSTR("org.sbooth.SFBAudioEngine.ErrorDomain.AudioMetadata");

// ========================================
// Key names for the metadata dictionary
// ========================================
static const CFStringRef	kMetadataTitleKey						= CFSTR("Title");
static const CFStringRef	kMetadataAlbumTitleKey					= CFSTR("AlbumTitle");
static const CFStringRef	kMetadataArtistKey						= CFSTR("Artist");
static const CFStringRef	kMetadataAlbumArtistKey					= CFSTR("AlbumArtist");
static const CFStringRef	kMetadataGenreKey						= CFSTR("Genre");
static const CFStringRef	kMetadataComposerKey					= CFSTR("Composer");
static const CFStringRef	kMetadataReleaseDateKey					= CFSTR("Date");
static const CFStringRef	kMetadataCompilationKey					= CFSTR("Compilation");
static const CFStringRef	kMetadataTrackNumberKey					= CFSTR("TrackNumber");
static const CFStringRef	kMetadataTrackTotalKey					= CFSTR("TrackTotal");
static const CFStringRef	kMetadataDiscNumberKey					= CFSTR("DiscNumber");
static const CFStringRef	kMetadataDiscTotalKey					= CFSTR("DiscTotal");
static const CFStringRef	kMetadataLyricsKey						= CFSTR("Lyrics");
static const CFStringRef	kMetadataCommentKey						= CFSTR("Comment");
static const CFStringRef	kMetadataISRCKey						= CFSTR("ISRC");
static const CFStringRef	kMetadataMCNKey							= CFSTR("MCN");
static const CFStringRef	kMetadataMusicBrainzAlbumIDKey			= CFSTR("MusicBrainzAlbumID");
static const CFStringRef	kMetadataMusicBrainzTrackIDKey			= CFSTR("MusicBrainzTrackID");
static const CFStringRef	kMetadataAdditionalMetadataKey			= CFSTR("AdditionalMetadata");
static const CFStringRef	kReplayGainReferenceLoudnessKey			= CFSTR("ReplayGainReferenceLoudess");
static const CFStringRef	kReplayGainTrackGainKey					= CFSTR("ReplayGainTrackGain");
static const CFStringRef	kReplayGainTrackPeakKey					= CFSTR("ReplayGainTrackPeak");
static const CFStringRef	kReplayGainAlbumGainKey					= CFSTR("ReplayGainAlbumGain");
static const CFStringRef	kReplayGainAlbumPeakKey					= CFSTR("ReplayGainAlbumPeak");
static const CFStringRef	kAlbumArtFrontCoverKey					= CFSTR("AlbumArtFrontCover");


#pragma mark Static Methods


CFArrayRef AudioMetadata::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 32, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderExtensions = FLACMetadata::CreateSupportedFileExtensions();
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
	
	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedExtensions);
	
	CFRelease(supportedExtensions), supportedExtensions = NULL;
	
	return result;
}

CFArrayRef AudioMetadata::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 32, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderMIMETypes = FLACMetadata::CreateSupportedMIMETypes();
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
	
	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return result;
}

bool AudioMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
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
	assert(NULL != mimeType);
	
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

AudioMetadata * AudioMetadata::CreateMetadataForURL(CFURLRef url)
{
	assert(NULL != url);
	
	AudioMetadata *metadata = NULL;
	
	// If this is a file URL, use the extension-based resolvers
	CFStringRef scheme = CFURLCopyScheme(url);
	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));
		
		if(NULL != fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				CFStringRef pathExtension = CFURLCopyPathExtension(url);
				
				if(NULL != pathExtension) {
					
					// Creating a decoder may throw an exception for any number of reasons
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
					// In lieu of adding a FileIsValid() method to each class that would open
					// and evaluate each file before opening, the try/catch madness here has a
					// similar effect without the file opening overhead
					
					// Additionally, as a factory this class has knowledge of its subclasses
					// It would be possible (and perhaps preferable) to switch to a generic
					// plugin interface at a later date
					try {
						if(FLACMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new FLACMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(WavPackMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new WavPackMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(MP3Metadata::HandlesFilesWithExtension(pathExtension))
							metadata = new MP3Metadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(MP4Metadata::HandlesFilesWithExtension(pathExtension))
							metadata = new MP4Metadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}
					
					try {
						if(WAVEMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new WAVEMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(AIFFMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new AIFFMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(MusepackMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new MusepackMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}

					try {
						if(OggVorbisMetadata::HandlesFilesWithExtension(pathExtension))
							metadata = new OggVorbisMetadata(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating metadata: %s", e.what());
					}
					
					CFRelease(pathExtension), pathExtension = NULL;
				}				
			}
			else
				LOG("The requested URL doesn't exist");
		}
		else
			ERR("CFURLCreatePropertyFromResource failed: %i", errorCode);		
		
		CFRelease(fileExists), fileExists = NULL;
	}
	// Determine the MIME type for the URL
	else {
		// Get the UTI for this URL
		FSRef ref;
		Boolean success = CFURLGetFSRef(url, &ref);
		if(FALSE == success) {
			ERR("Unable to get FSRef for URL");
			
			return NULL;
		}
		
		CFStringRef uti = NULL;
		OSStatus result = LSCopyItemAttribute(&ref, kLSRolesAll, kLSItemContentType, (CFTypeRef *)&uti);
		
		if(noErr != result) {
			ERR("LSCopyItemAttribute (kLSItemContentType) failed: %i", result);
			
			return NULL;
		}
		
		CFRelease(uti), uti = NULL;
	}
	
	CFRelease(scheme), scheme = NULL;
	
	return metadata;
}


#pragma mark Creation and Destruction


AudioMetadata::AudioMetadata()
	: mURL(NULL)
{
	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
										  32,
										  &kCFTypeDictionaryKeyCallBacks,
										  &kCFTypeDictionaryValueCallBacks);
}

AudioMetadata::AudioMetadata(CFURLRef url)
	: mURL(NULL)
{
	assert(NULL != url);
	
	mURL = static_cast<CFURLRef>(CFRetain(url));

	mMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
										  32,
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
}


#pragma mark Operator Overloads


AudioMetadata& AudioMetadata::operator=(const AudioMetadata& rhs)
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;

	if(mMetadata)
		CFRelease(mMetadata), mMetadata = NULL;

	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));

	if(rhs.mMetadata)
		mMetadata = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
												  32, 
												  rhs.mMetadata);

	return *this;
}


#pragma mark Metadata Access


CFStringRef AudioMetadata::GetTitle()
{
	return GetStringValue(kMetadataTitleKey);
}

void AudioMetadata::SetTitle(CFStringRef title)
{
	SetValue(kMetadataTitleKey, title);
}

CFStringRef AudioMetadata::GetAlbumTitle()
{
	return GetStringValue(kMetadataAlbumTitleKey);
}

void AudioMetadata::SetAlbumTitle(CFStringRef albumTitle)
{
	SetValue(kMetadataAlbumTitleKey, albumTitle);
}

CFStringRef AudioMetadata::GetArtist()
{
	return GetStringValue(kMetadataArtistKey);
}

void AudioMetadata::SetArtist(CFStringRef artist)
{
	SetValue(kMetadataArtistKey, artist);
}

CFStringRef AudioMetadata::GetAlbumArtist()
{
	return GetStringValue(kMetadataAlbumArtistKey);
}

void AudioMetadata::SetAlbumArtist(CFStringRef albumArtist)
{
	SetValue(kMetadataAlbumArtistKey, albumArtist);
}

CFStringRef AudioMetadata::GetGenre()
{
	return GetStringValue(kMetadataGenreKey);
}

void AudioMetadata::SetGenre(CFStringRef genre)
{
	SetValue(kMetadataGenreKey, genre);
}

CFStringRef AudioMetadata::GetComposer()
{
	return GetStringValue(kMetadataComposerKey);
}

void AudioMetadata::SetComposer(CFStringRef composer)
{
	SetValue(kMetadataComposerKey, composer);
}

CFStringRef AudioMetadata::GetReleaseDate()
{
	return GetStringValue(kMetadataReleaseDateKey);
}

void AudioMetadata::SetReleaseDate(CFStringRef releaseDate)
{
	SetValue(kMetadataReleaseDateKey, releaseDate);
}

CFBooleanRef AudioMetadata::GetCompilation()
{
	CFTypeRef value = GetValue(kMetadataCompilationKey);
	
	if(CFBooleanGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFBooleanRef>(value);
}

void AudioMetadata::SetCompilation(CFBooleanRef compilation)
{
	SetValue(kMetadataCompilationKey, compilation);
}

CFNumberRef AudioMetadata::GetTrackNumber()
{
	return GetNumberValue(kMetadataTrackNumberKey);
}

void AudioMetadata::SetTrackNumber(CFNumberRef trackNumber)
{
	SetValue(kMetadataTrackNumberKey, trackNumber);
}

CFNumberRef AudioMetadata::GetTrackTotal()
{
	return GetNumberValue(kMetadataTrackTotalKey);
}

void AudioMetadata::SetTrackTotal(CFNumberRef trackTotal)
{
	SetValue(kMetadataTrackTotalKey, trackTotal);
}

CFNumberRef AudioMetadata::GetDiscNumber()
{
	return GetNumberValue(kMetadataDiscNumberKey);
}

void AudioMetadata::SetDiscNumber(CFNumberRef discNumber)
{
	SetValue(kMetadataDiscNumberKey, discNumber);
}

CFNumberRef AudioMetadata::GetDiscTotal()
{
	return GetNumberValue(kMetadataDiscTotalKey);
}

void AudioMetadata::SetDiscTotal(CFNumberRef discTotal)
{
	SetValue(kMetadataDiscTotalKey, discTotal);
}

CFStringRef AudioMetadata::GetLyrics()
{
	return GetStringValue(kMetadataLyricsKey);
}

void AudioMetadata::SetLyrics(CFStringRef lyrics)
{
	SetValue(kMetadataLyricsKey, lyrics);
}

CFStringRef AudioMetadata::GetComment()
{
	return GetStringValue(kMetadataCommentKey);
}

void AudioMetadata::SetComment(CFStringRef comment)
{
	SetValue(kMetadataCommentKey, comment);
}

CFStringRef AudioMetadata::GetMCN()
{
	return GetStringValue(kMetadataMCNKey);
}

void AudioMetadata::SetMCN(CFStringRef mcn)
{
	SetValue(kMetadataMCNKey, mcn);
}

CFStringRef AudioMetadata::GetISRC()
{
	return GetStringValue(kMetadataISRCKey);
}

void AudioMetadata::SetISRC(CFStringRef isrc)
{
	SetValue(kMetadataISRCKey, isrc);
}

CFStringRef AudioMetadata::GetMusicBrainzAlbumID()
{
	return GetStringValue(kMetadataMusicBrainzAlbumIDKey);
}

void AudioMetadata::SetMusicBrainzAlbumID(CFStringRef albumID)
{
	SetValue(kMetadataMusicBrainzAlbumIDKey, albumID);
}

CFStringRef AudioMetadata::GetMusicBrainzTrackID()
{
	return GetStringValue(kMetadataMusicBrainzTrackIDKey);
}

void AudioMetadata::SetMusicBrainzTrackID(CFStringRef trackID)
{
	SetValue(kMetadataMusicBrainzTrackIDKey, trackID);
}


#pragma mark Additional Metadata


CFDictionaryRef AudioMetadata::GetAdditionalMetadata()
{
	CFTypeRef value = GetValue(kMetadataAdditionalMetadataKey);
	
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


CFNumberRef AudioMetadata::GetReplayGainReferenceLoudness()
{
	return GetNumberValue(kReplayGainReferenceLoudnessKey);
}

void AudioMetadata::SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness)
{
	SetValue(kReplayGainReferenceLoudnessKey, referenceLoudness);
}

CFNumberRef AudioMetadata::GetReplayGainTrackGain()
{
	return GetNumberValue(kReplayGainTrackGainKey);
}

void AudioMetadata::SetReplayGainTrackGain(CFNumberRef trackGain)
{
	SetValue(kReplayGainTrackGainKey, trackGain);
}

CFNumberRef AudioMetadata::GetReplayGainTrackPeak()
{
	return GetNumberValue(kReplayGainTrackPeakKey);
}

void AudioMetadata::SetReplayGainTrackPeak(CFNumberRef trackPeak)
{
	SetValue(kReplayGainTrackPeakKey, trackPeak);
}

CFNumberRef AudioMetadata::GetReplayGainAlbumGain()
{
	return GetNumberValue(kReplayGainAlbumGainKey);
}

void AudioMetadata::SetReplayGainAlbumGain(CFNumberRef albumGain)
{
	SetValue(kReplayGainAlbumGainKey, albumGain);
}

CFNumberRef AudioMetadata::GetReplayGainAlbumPeak()
{
	return GetNumberValue(kReplayGainAlbumPeakKey);
}

void AudioMetadata::SetReplayGainAlbumPeak(CFNumberRef albumPeak)
{
	SetValue(kReplayGainAlbumPeakKey, albumPeak);
}


#pragma mark Album Artwork


CFDataRef AudioMetadata::GetFrontCoverArt()
{
	CFTypeRef value = GetValue(kAlbumArtFrontCoverKey);
	
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


CFStringRef AudioMetadata::GetStringValue(CFStringRef key)
{
	assert(NULL != key);
	
	CFTypeRef value = GetValue(key);
	
	if(CFStringGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFStringRef>(value);
}

CFNumberRef AudioMetadata::GetNumberValue(CFStringRef key)
{
	assert(NULL != key);
	
	CFTypeRef value = GetValue(key);
	
	if(CFNumberGetTypeID() != CFGetTypeID(value))
		return NULL;
	else
		return reinterpret_cast<CFNumberRef>(value);
}


#pragma mark Generic Access


CFTypeRef AudioMetadata::GetValue(CFStringRef key)
{
	assert(NULL != key);
	
	return CFDictionaryGetValue(mMetadata, key);
}

void AudioMetadata::SetValue(CFStringRef key, CFTypeRef value)
{
	assert(NULL != key);
	
	if(NULL == value)
		CFDictionaryRemoveValue(mMetadata, key);
	else
		CFDictionarySetValue(mMetadata, key, value);
}
