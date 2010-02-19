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

#pragma once

#include <CoreFoundation/CoreFoundation.h>


// ========================================
// Error Codes
// ========================================
extern const CFStringRef		AudioMetadataErrorDomain;

enum {
	AudioMetadataFileFormatNotRecognizedError		= 0,
	AudioMetadataFileFormatNotSupportedError		= 1,
	AudioMetadataInputOutputError					= 2
};

// ========================================
// Key names for the metadata dictionary
// ========================================
extern const CFStringRef		kMetadataTitleKey;					// CFStringRef
extern const CFStringRef		kMetadataAlbumTitleKey;				// CFStringRef
extern const CFStringRef		kMetadataArtistKey;					// CFStringRef
extern const CFStringRef		kMetadataAlbumArtistKey;			// CFStringRef
extern const CFStringRef		kMetadataGenreKey;					// CFStringRef
extern const CFStringRef		kMetadataComposerKey;				// CFStringRef
extern const CFStringRef		kMetadataReleaseDateKey;			// CFStringRef
extern const CFStringRef		kMetadataCompilationKey;			// CFBooleanRef
extern const CFStringRef		kMetadataTrackNumberKey;			// CFNumberRef
extern const CFStringRef		kMetadataTrackTotalKey;				// CFNumberRef
extern const CFStringRef		kMetadataDiscNumberKey;				// CFNumberRef
extern const CFStringRef		kMetadataDiscTotalKey;				// CFNumberRef
extern const CFStringRef		kMetadataLyricsKey;					// CFStringRef
extern const CFStringRef		kMetadataCommentKey;				// CFStringRef
extern const CFStringRef		kMetadataISRCKey;					// CFStringRef
extern const CFStringRef		kMetadataMCNKey;					// CFStringRef
extern const CFStringRef		kMetadataMusicBrainzAlbumIDKey;		// CFStringRef
extern const CFStringRef		kMetadataMusicBrainzTrackIDKey;		// CFStringRef
extern const CFStringRef		kMetadataAdditionalMetadataKey;		// CFDictionaryRef

extern const CFStringRef		kReplayGainReferenceLoudnessKey;	// CFNumberRef
extern const CFStringRef		kReplayGainTrackGainKey;			// CFNumberRef
extern const CFStringRef		kReplayGainTrackPeakKey;			// CFNumberRef
extern const CFStringRef		kReplayGainAlbumGainKey;			// CFNumberRef
extern const CFStringRef		kReplayGainAlbumPeakKey;			// CFNumberRef

//extern const CFStringRef		kCueSheetKey;						// CFStringRef
extern const CFStringRef		kAlbumArtFrontCoverKey;				// CFDataRef


// ========================================
//
// ========================================
class AudioMetadata
{
public:

	// ========================================
	// Information on supported file formats
	static CFArrayRef CreateSupportedFileExtensions();
	static CFArrayRef CreateSupportedMIMETypes();
	
	static bool HandlesFilesWithExtension(CFStringRef extension);
	static bool HandlesMIMEType(CFStringRef mimeType);
	
	// ========================================
	// Factory methods that return an AudioMetadata for the specified URL, or NULL on failure
	static AudioMetadata * CreateMetadataForURL(CFURLRef url);
	virtual ~AudioMetadata();
	
	// ========================================
	// The URL containing this metadata
	inline CFURLRef GetURL()								{ return mURL; }
	
	// ========================================
	// The core functionality
	virtual bool ReadMetadata(CFErrorRef *error = NULL) = 0;
	virtual bool WriteMetadata(CFErrorRef *error = NULL) = 0;
	
	// ========================================
	// Metadata access
	CFStringRef GetTitle();
	void SetTitle(CFStringRef title);

	CFStringRef GetAlbumTitle();
	void SetAlbumTitle(CFStringRef albumTitle);

	CFStringRef GetArtist();
	void SetArtist(CFStringRef artist);

	CFStringRef GetAlbumArtist();
	void SetAlbumArtist(CFStringRef albumArtist);

	CFStringRef GetGenre();
	void SetGenre(CFStringRef genre);

	CFStringRef GetComposer();
	void SetComposer(CFStringRef composer);

	CFStringRef GetReleaseDate();
	void SetReleaseDate(CFStringRef releaseDate);

	CFBooleanRef GetCompilation();
	void SetCompilation(CFBooleanRef releaseDate);

	CFNumberRef GetTrackNumber();
	void SetTrackNumber(CFNumberRef trackNumber);

	CFNumberRef GetTrackTotal();
	void SetTrackTotal(CFNumberRef trackTotal);

	CFNumberRef GetDiscNumber();
	void SetDiscNumber(CFNumberRef discNumber);

	CFNumberRef GetDiscTotal();
	void SetDiscTotal(CFNumberRef discTotal);

	CFStringRef GetLyrics();
	void SetLyrics(CFStringRef lyrics);
	
	CFStringRef GetComment();
	void SetComment(CFStringRef comment);

	CFStringRef GetMCN();
	void SetMCN(CFStringRef mcn);

	CFStringRef GetISRC();
	void SetISRC(CFStringRef isrc);

	CFStringRef GetMusicBrainzAlbumID();
	void SetMusicBrainzAlbumID(CFStringRef albumID);

	CFStringRef GetMusicBrainzTrackID();
	void SetMusicBrainzTrackID(CFStringRef trackID);

	CFDictionaryRef GetAdditionalMetadata();
	void SetAdditionalMetadata(CFDictionaryRef additionalMetadata);
	
	CFNumberRef GetReplayGainReferenceLoudness();
	void SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness);

	CFNumberRef GetReplayGainTrackGain();
	void SetReplayGainTrackGain(CFNumberRef trackGain);

	CFNumberRef GetReplayGainTrackPeak();
	void SetReplayGainTrackPeak(CFNumberRef trackPeak);

	CFNumberRef GetReplayGainAlbumGain();
	void SetReplayGainAlbumGain(CFNumberRef albumGain);

	CFNumberRef GetReplayGainAlbumPeak();
	void SetReplayGainAlbumPeak(CFNumberRef albumPeak);

	// Cue Sheet

	CFDataRef GetFrontCoverArt();
	void SetFrontCoverArt(CFDataRef frontCoverArt);

	
protected:
	
	// ========================================
	// Data members
	CFURLRef						mURL;				// The location of the stream to be decoded
	CFMutableDictionaryRef			mMetadata;			// The metadata information
	
	// ========================================
	// For subclass use only
	AudioMetadata();
	AudioMetadata(CFURLRef url);
	AudioMetadata(const AudioMetadata& rhs);
	AudioMetadata& operator=(const AudioMetadata& rhs);

	// Type-specific access
	CFStringRef GetStringValue(CFStringRef key);
	CFNumberRef GetNumberValue(CFStringRef key);

	// Generic access
	CFTypeRef GetValue(CFStringRef key);
	void SetValue(CFStringRef key, CFTypeRef value);
};
