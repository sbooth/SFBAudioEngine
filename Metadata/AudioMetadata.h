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

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <vector>

#include "AttachedPicture.h"

/*! @file AudioMetadata.h @brief Support for metadata reading and writing */

extern const CFStringRef		AudioMetadataErrorDomain;	/*!< @brief The \c CFErrorRef error domain used by \c AudioMetadata */

/*! @brief Possible \c CFErrorRef error codes used by \c AudioMetadata */
enum {
	AudioMetadataFileFormatNotRecognizedError		= 0,	/*!< File format not recognized */
	AudioMetadataFileFormatNotSupportedError		= 1,	/*!< File format not supported */
	AudioMetadataInputOutputError					= 2		/*!< Input/output error */
};

/*! @name Audio property dictionary keys */
//@{
extern const CFStringRef		kPropertiesFormatNameKey;			/*!< @brief The name of the audio format */
extern const CFStringRef		kPropertiesTotalFramesKey;			/*!< @brief The total number of audio frames (\c CFNumber) */
extern const CFStringRef		kPropertiesChannelsPerFrameKey;		/*!< @brief The number of channels (\c CFNumber) */
extern const CFStringRef		kPropertiesBitsPerChannelKey;		/*!< @brief The number of bits per channel (\c CFNumber) */
extern const CFStringRef		kPropertiesSampleRateKey;			/*!< @brief The sample rate (\c CFNumber) */
extern const CFStringRef		kPropertiesDurationKey;				/*!< @brief The duration (\c CFNumber) */
extern const CFStringRef		kPropertiesBitrateKey;				/*!< @brief The audio bitrate (\c CFNumber) */
//@}

/*! @name Metadata dictionary keys */
//@{
extern const CFStringRef		kMetadataTitleKey;					/*!< @brief Title (\c CFString) */
extern const CFStringRef		kMetadataAlbumTitleKey;				/*!< @brief Album title (\c CFString) */
extern const CFStringRef		kMetadataArtistKey;					/*!< @brief Artist (\c CFString) */
extern const CFStringRef		kMetadataAlbumArtistKey;			/*!< @brief Album artist (\c CFString) */
extern const CFStringRef		kMetadataGenreKey;					/*!< @brief Genre (\c CFString) */
extern const CFStringRef		kMetadataComposerKey;				/*!< @brief Composer (\c CFString) */
extern const CFStringRef		kMetadataReleaseDateKey;			/*!< @brief Release date (\c CFString) */
extern const CFStringRef		kMetadataCompilationKey;			/*!< @brief Compilation flag (\c CFBoolean) */
extern const CFStringRef		kMetadataTrackNumberKey;			/*!< @brief Track number (\c CFNumber) */
extern const CFStringRef		kMetadataTrackTotalKey;				/*!< @brief Track total (\c CFNumber) */
extern const CFStringRef		kMetadataDiscNumberKey;				/*!< @brief Disc number (\c CFNumber) */
extern const CFStringRef		kMetadataDiscTotalKey;				/*!< @brief Disc total (\c CFNumber) */
extern const CFStringRef		kMetadataLyricsKey;					/*!< @brief Lyrics (\c CFString) */
extern const CFStringRef		kMetadataBPMKey;					/*!< @brief Beats per minute (BPM) (\c CFNumber) */
extern const CFStringRef		kMetadataRatingKey;					/*!< @brief Rating (\c CFNumber) */
extern const CFStringRef		kMetadataCommentKey;				/*!< @brief Comment (\c CFString) */
extern const CFStringRef		kMetadataISRCKey;					/*!< @brief International Standard Recording Code (ISRC) (\c CFString) */
extern const CFStringRef		kMetadataMCNKey;					/*!< @brief Media Catalog Number (MCN) (\c CFString) */
extern const CFStringRef		kMetadataMusicBrainzReleaseIDKey;	/*!< @brief MusicBrainz release ID (\c CFString) */
extern const CFStringRef		kMetadataMusicBrainzRecordingIDKey;	/*!< @brief MusicBrainz recording ID (\c CFString) */
//@}

/*! @name Sorting dictionary keys */
//@{
extern const CFStringRef		kMetadataTitleSortOrderKey;			/*!< @brief Title sort order (\c CFString) */
extern const CFStringRef		kMetadataAlbumTitleSortOrderKey;	/*!< @brief Album title sort order (\c CFString) */
extern const CFStringRef		kMetadataArtistSortOrderKey;		/*!< @brief Artist sort order (\c CFString) */
extern const CFStringRef		kMetadataAlbumArtistSortOrderKey;	/*!< @brief Album artist sort order (\c CFString) */
extern const CFStringRef		kMetadataComposerSortOrderKey;		/*!< @brief Composer sort order (\c CFString) */
//@}

/*! @name Grouping dictionary keys */
//@{
extern const CFStringRef		kMetadataGroupingKey;				/*!< @brief Grouping (\c CFString) */
//@}

/*! @name Additional metadata dictionary keys */
//@{
extern const CFStringRef		kMetadataAdditionalMetadataKey;		/*!< @brief Additional metadata (\c CFDictionary) */
//@}

/*! @name Replay gain dictionary keys */
//@{
extern const CFStringRef		kReplayGainReferenceLoudnessKey;	/*!< @brief Replay gain reference loudness (\c CFNumber) */
extern const CFStringRef		kReplayGainTrackGainKey;			/*!< @brief Replay gain track gain (\c CFNumber) */
extern const CFStringRef		kReplayGainTrackPeakKey;			/*!< @brief Replay gain track peak (\c CFNumber) */
extern const CFStringRef		kReplayGainAlbumGainKey;			/*!< @brief Replay gain album gain (\c CFNumber) */
extern const CFStringRef		kReplayGainAlbumPeakKey;			/*!< @brief Replay gain album peak (\c CFNumber) */
//@}


/*! @brief Base class for all audio metadata reader/writer classes */
class AudioMetadata
{
public:

	// ========================================
	/*! @name Supported file formats */
	//@{

	/*!
	 * @brief Create an array containing the supported file extensions
	 * @note The returned array must be released by the caller
	 * @return An array containing the supported file extensions
	 */
	static CFArrayRef CreateSupportedFileExtensions();

	/*!
	 * @brief Create an array containing the supported MIME types
	 * @note The returned array must be released by the caller
	 * @return An array containing the supported MIME types
	 */
	static CFArrayRef CreateSupportedMIMETypes();


	/*!
	 * @brief Test whether a file extension is supported
	 * @param extension The file extension to test
	 * @return \c true if the file extension is supported, \c false otherwise
	 */
	static bool HandlesFilesWithExtension(CFStringRef extension);

	/*!
	 * @brief Test whether a MIME type is supported
	 * @param mimeType The MIME type to test
	 * @return \c true if the MIME type is supported, \c false otherwise
	 */
	static bool HandlesMIMEType(CFStringRef mimeType);

	//@}


	// ========================================
	/*! @name Factory Methods */
	//@{

	/*!
	 * @brief Create an \c AudioMetadata object for the specified URL
	 * @param url The URL
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioMetadata object, or \c nullptr on failure
	 */
	static AudioMetadata * CreateMetadataForURL(CFURLRef url, CFErrorRef *error = nullptr);

	//@}


	// ========================================
	/*! @name Creation and Destruction */
	//@{
	
	/*! @brief Destroy this \c AudioMetadata */
	virtual ~AudioMetadata();
	
	/*! @cond */

	/*! @internal This class is non-copyable */
	AudioMetadata(const AudioMetadata& rhs) = delete;

	/*! @internal This class is non-assignable */
	AudioMetadata& operator=(const AudioMetadata& rhs) = delete;

	/*! @endcond */
	//@}


	// ========================================
	/*! @name URL access */
	//@{

	/*! @brief Get the URL containing this metadata */
	inline CFURLRef GetURL() const							{ return mURL; }

	/*! @brief Set the URL containing this metadata */
	void SetURL(CFURLRef URL);

	//@}


	// ========================================
	/*! @name File access */
	//@{

	/*!
	 * @brief Read the metadata
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return \c true on success, \c false otherwise
	 */
	virtual bool ReadMetadata(CFErrorRef *error = nullptr) = 0;

	/*!
	 * @brief Write the metadata
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return \c true on success, \c false otherwise
	 */
	virtual bool WriteMetadata(CFErrorRef *error = nullptr) = 0;

	//@}


	// ========================================
	/*! @name Change management */
	//@{

	/*! @brief Query the object for unsaved changes */
	bool HasUnsavedChanges() const;

	/*! @brief Revert unsaved changes */
	void RevertUnsavedChanges();

	/*! @brief Query a particular key for unsaved changes */
	inline bool HasUnsavedChangesForKey(CFStringRef key) const { return CFDictionaryContainsKey(mChangedMetadata, key); }

	//@}


	// ========================================
	/*! @name Properties access */
	//@{

	/*! @brief Get the name of the audio format */
	CFStringRef GetFormatName() const;

	/*! @brief Get the total number of audio frames */
	CFNumberRef GetTotalFrames() const;

	/*! @brief Get the number of channels */
	CFNumberRef GetChannelsPerFrame() const;

	/*! @brief Get the number of bits per channel */
	CFNumberRef GetBitsPerChannel() const;

	/*! @brief Get the sample rate in Hz */
	CFNumberRef GetSampleRate() const;

	/*! @brief Get the duration in seconds */
	CFNumberRef GetDuration() const;

	/*! @brief Get the audio bitrate in KiB/sec */
	CFNumberRef GetBitrate() const;

	//@}


	// ========================================
	/*!
	 * @name Metadata
	 * To remove an existing value call the appropriate \c Set() function with \c nullptr
	 */
	//@{


	/*! @brief Get the title */
	CFStringRef GetTitle() const;

	/*! @brief Set the title */
	void SetTitle(CFStringRef title);


	/*! @brief Get the album title */
	CFStringRef GetAlbumTitle() const;

	/*! @brief Set the album title */
	void SetAlbumTitle(CFStringRef albumTitle);


	/*! @brief Get the artist */
	CFStringRef GetArtist() const;

	/*! @brief Set the artist */
	void SetArtist(CFStringRef artist);


	/*! @brief Get the album artist */
	CFStringRef GetAlbumArtist() const;

	/*! @brief Set the album artist */
	void SetAlbumArtist(CFStringRef albumArtist);


	/*! @brief Get the genre */
	CFStringRef GetGenre() const;

	/*! @brief Set the genre */
	void SetGenre(CFStringRef genre);


	/*! @brief Get the composer */
	CFStringRef GetComposer() const;

	/*! @brief Set the composer */
	void SetComposer(CFStringRef composer);


	/*! @brief Get the release date */
	CFStringRef GetReleaseDate() const;

	/*! @brief Set the release date */
	void SetReleaseDate(CFStringRef releaseDate);


	/*! @brief Get the compilation flag */
	CFBooleanRef GetCompilation() const;

	/*! @brief Set the compilation flag */
	void SetCompilation(CFBooleanRef releaseDate);


	/*! @brief Get the track number */
	CFNumberRef GetTrackNumber() const;

	/*! @brief Set the track number */
	void SetTrackNumber(CFNumberRef trackNumber);


	/*! @brief Get the track total */
	CFNumberRef GetTrackTotal() const;

	/*! @brief Set the track total */
	void SetTrackTotal(CFNumberRef trackTotal);


	/*! @brief Get the disc number */
	CFNumberRef GetDiscNumber() const;

	/*! @brief Set the disc number */
	void SetDiscNumber(CFNumberRef discNumber);


	/*! @brief Get the disc total */
	CFNumberRef GetDiscTotal() const;

	/*! @brief Set the disc total */
	void SetDiscTotal(CFNumberRef discTotal);


	/*! @brief Get the lyrics */
	CFStringRef GetLyrics() const;

	/*! @brief Set the lyrics */
	void SetLyrics(CFStringRef lyrics);


	/*! @brief Get the Beats per minute (BPM) */
	CFNumberRef GetBPM() const;

	/*! @brief Set the Beats per minute (BPM) */
	void SetBPM(CFNumberRef BPM);


	/*! @brief Get the rating */
	CFNumberRef GetRating() const;

	/*! @brief Set the rating */
	void SetRating(CFNumberRef rating);


	/*! @brief Get the comment */
	CFStringRef GetComment() const;

	/*! @brief Set the comment */
	void SetComment(CFStringRef comment);


	/*! @brief Get the Media Catalog Number (MCN) */
	CFStringRef GetMCN() const;

	/*! @brief Set the Media Catalog Number (MCN) */
	void SetMCN(CFStringRef mcn);


	/*! @brief Get the International Standard Recording Code (ISRC) */
	CFStringRef GetISRC() const;

	/*! @brief Set the International Standard Recording Code (ISRC) */
	void SetISRC(CFStringRef isrc);


	/*! @brief Get the MusicBrainz release ID */
	CFStringRef GetMusicBrainzReleaseID() const;

	/*! @brief Set the MusicBrainz release ID */
	void SetMusicBrainzReleaseID(CFStringRef releaseID);


	/*! @brief Get the MusicBrainz recording ID */
	CFStringRef GetMusicBrainzRecordingID() const;

	/*! @brief Set the MusicBrainz recording ID */
	void SetMusicBrainzRecordingID(CFStringRef recordingID);

	//@}


	// ========================================
	/*!
	 * @name Sorting
	 * To remove an existing value call the appropriate \c Set() function with \c nullptr
	 */
	//@{

	/*! @brief Get the title sort order */
	CFStringRef GetTitleSortOrder() const;

	/*! @brief Set the title sort order */
	void SetTitleSortOrder(CFStringRef titleSortOrder);


	/*! @brief Get the album title sort order */
	CFStringRef GetAlbumTitleSortOrder() const;

	/*! @brief Set the album title sort order */
	void SetAlbumTitleSortOrder(CFStringRef albumTitleSortOrder);


	/*! @brief Get the artist sort order */
	CFStringRef GetArtistSortOrder() const;

	/*! @brief Set the artist sort order */
	void SetArtistSortOrder(CFStringRef artistSortOrder);


	/*! @brief Get the album artist sort order */
	CFStringRef GetAlbumArtistSortOrder() const;

	/*! @brief Set the album artist sort order */
	void SetAlbumArtistSortOrder(CFStringRef albumArtistSortOrder);


	/*! @brief Get the composer sort order */
	CFStringRef GetComposerSortOrder() const;

	/*! @brief Set the composer sort order */
	void SetComposerSortOrder(CFStringRef composerSortOrder);

	//@}


	// ========================================
	/*!
	 * @name Grouping
	 * To remove an existing value call the appropriate \c Set() function with \c nullptr
	 */
	//@{

	/*! @brief Get the grouping */
	CFStringRef GetGrouping() const;

	/*! @brief Set the grouping */
	void SetGrouping(CFStringRef grouping);

	//@}


	// ========================================
	/*! 
	 * @name Additional metadata
	 * To remove an existing value call the appropriate \c Set() function with \c nullptr
	 */
	//@{

	/*! @brief Get the additional metadata */
	CFDictionaryRef GetAdditionalMetadata() const;

	/*! @brief Set the additional metadata */
	void SetAdditionalMetadata(CFDictionaryRef additionalMetadata);

	//@}


	// ========================================
	/*!
	 * @name Replay gain
	 * To remove an existing value call the appropriate \c Set() function with \c nullptr
	 */
	//@{

	/*! @brief Get the replay gain reference loudness */
	CFNumberRef GetReplayGainReferenceLoudness() const;

	/*! @brief Set the replay gain reference loudness (should be 89.0 dB) */
	void SetReplayGainReferenceLoudness(CFNumberRef referenceLoudness);


	/*! @brief Get the replay gain track gain */
	CFNumberRef GetReplayGainTrackGain() const;

	/*! @brief Set the replay gain track gain */
	void SetReplayGainTrackGain(CFNumberRef trackGain);


	/*! @brief Get the replay gain track peak */
	CFNumberRef GetReplayGainTrackPeak() const;

	/*! @brief Set the replay gain track peak */
	void SetReplayGainTrackPeak(CFNumberRef trackPeak);


	/*! @brief Get the replay gain album gain */
	CFNumberRef GetReplayGainAlbumGain() const;

	/*! @brief Set the replay gain album gain */
	void SetReplayGainAlbumGain(CFNumberRef albumGain);


	/*! @brief Get the replay gain album peak */
	CFNumberRef GetReplayGainAlbumPeak() const;

	/*! @brief Set the replay gain album peak */
	void SetReplayGainAlbumPeak(CFNumberRef albumPeak);

	//@}


	// ========================================
	/*! @name Album artwork */
	//@{

	/*! @brief Get all attached pictures */
	const std::vector<std::shared_ptr<AttachedPicture>> GetAttachedPictures() const;

	/*! @brief Get all attached pictures of the specified type */
	const std::vector<std::shared_ptr<AttachedPicture>> GetAttachedPicturesOfType(AttachedPicture::Type type) const;


	/*! @brief Attach a picture */
	void AttachPicture(std::shared_ptr<AttachedPicture> picture);

	/*! @brief Remove an attached picture */
	void RemoveAttachedPicture(std::shared_ptr<AttachedPicture> picture);


	/*! @brief Remove all attached pictures of the specified type */
	void RemoveAttachedPicturesOfType(AttachedPicture::Type type);

	/*! @brief Remove all attached pictures */
	void RemoveAllAttachedPictures();

	//@}

protected:

	CFURLRef						mURL;				/*!< @brief The location of the stream to be read/written */

	CFMutableDictionaryRef			mMetadata;			/*!< @brief The metadata information */
	CFMutableDictionaryRef			mChangedMetadata;	/*!< @brief The metadata information that has been changed but not saved */

	std::vector<std::shared_ptr<AttachedPicture>>	mPictures; /*!< @brief The attached picture information */


	/*! @brief Create a new \c AudioMetadata and initialize \c AudioMetadata::mURL to \c nullptr */
	AudioMetadata();

	/*! @brief Create a new \c AudioMetadata and initialize \c AudioMetadata::mURL to \c url */
	AudioMetadata(CFURLRef url);

	
	/*! @brief Subclasses should call this from ReadMetadata() to clear AudioMetadata::mMetadata, AudioMetadata::mChangedMetadata, and AudioMetadata::mPictures */
	void ClearAllMetadata();

	/*! @brief Subclasses should call this after a successful save operation */
	void MergeChangedMetadataIntoMetadata();


	/*! @name Type-specific access */
	//@{

	/*!
	 * @brief Retrieve a string from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key if present and a string, \c nullptr otherwise
	 */
	CFStringRef GetStringValue(CFStringRef key) const;

	/*!
	 * @brief Retrieve a number from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key if present and a number, \c nullptr otherwise
	 */
	CFNumberRef GetNumberValue(CFStringRef key) const;

	//@}
	

	/*! @name Generic access */
	//@{

	/*!
	 * @brief Retrieve an object from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key
	 */
	CFTypeRef GetValue(CFStringRef key) const;

	/*!
	 * @brief Set a value in the metadata dictionary
	 * @param key The key to associate with \c value
	 * @param value The value to set
	 */
	void SetValue(CFStringRef key, CFTypeRef value);

	//@}


private:

	// ========================================
	// Subclass registration support
	struct SubclassInfo
	{
		CFArrayRef (*mCreateSupportedFileExtensions)();
		CFArrayRef (*mCreateSupportedMIMETypes)();

		bool (*mHandlesFilesWithExtension)(CFStringRef);
		bool (*mHandlesMIMEType)(CFStringRef);

		AudioMetadata * (*mCreateMetadata)(CFURLRef);

		int mPriority;
	};

	static std::vector <SubclassInfo> sRegisteredSubclasses;

public:

	/*!
	 * @brief Register an \c AudioMetadata subclass
	 * @tparam The subclass name
	 * @param priority The priority of the subclass
	 */
	template <typename T> static void RegisterSubclass(int priority = 0);

};

// ========================================
// Template implementation
template <typename T> void AudioMetadata::RegisterSubclass(int priority)
{
	SubclassInfo subclassInfo = {
		.mCreateSupportedFileExtensions = T::CreateSupportedFileExtensions,
		.mCreateSupportedMIMETypes = T::CreateSupportedMIMETypes,

		.mHandlesFilesWithExtension = T::HandlesFilesWithExtension,
		.mHandlesMIMEType = T::HandlesMIMEType,

		.mCreateMetadata = T::CreateMetadata,

		.mPriority = priority
	};

	sRegisteredSubclasses.push_back(subclassInfo);

	// Sort subclasses by priority
	std::sort(sRegisteredSubclasses.begin(), sRegisteredSubclasses.end(), [](const SubclassInfo& a, const SubclassInfo& b) {
		return a.mPriority > b.mPriority;
	});
}
