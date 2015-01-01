/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#include "CFWrapper.h"
#include "AttachedPicture.h"

/*! @file AudioMetadata.h @brief Support for metadata reading and writing */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief Base class for all audio metadata reader/writer classes */
		class Metadata
		{
			
		public:

			/*! @brief The \c CFErrorRef error domain used by \c Metadata and subclasses */
			static const CFStringRef ErrorDomain;

			/*! @brief Possible \c CFErrorRef error codes used by \c Metadata */
			enum ErrorCode {
				FileFormatNotRecognizedError		= 0,	/*!< File format not recognized */
				FileFormatNotSupportedError			= 1,	/*!< File format not supported */
				InputOutputError					= 2		/*!< Input/output error */
			};


			/*! @name Audio property dictionary keys */
			//@{
			static const CFStringRef kFormatNameKey;				/*!< @brief The name of the audio format */
			static const CFStringRef kTotalFramesKey;				/*!< @brief The total number of audio frames (\c CFNumber) */
			static const CFStringRef kChannelsPerFrameKey;			/*!< @brief The number of channels (\c CFNumber) */
			static const CFStringRef kBitsPerChannelKey;			/*!< @brief The number of bits per channel (\c CFNumber) */
			static const CFStringRef kSampleRateKey;				/*!< @brief The sample rate (\c CFNumber) */
			static const CFStringRef kDurationKey;					/*!< @brief The duration (\c CFNumber) */
			static const CFStringRef kBitrateKey;					/*!< @brief The audio bitrate (\c CFNumber) */
			//@}

			/*! @name Metadata dictionary keys */
			//@{
			static const CFStringRef kTitleKey;						/*!< @brief Title (\c CFString) */
			static const CFStringRef kAlbumTitleKey;				/*!< @brief Album title (\c CFString) */
			static const CFStringRef kArtistKey;					/*!< @brief Artist (\c CFString) */
			static const CFStringRef kAlbumArtistKey;				/*!< @brief Album artist (\c CFString) */
			static const CFStringRef kGenreKey;						/*!< @brief Genre (\c CFString) */
			static const CFStringRef kComposerKey;					/*!< @brief Composer (\c CFString) */
			static const CFStringRef kReleaseDateKey;				/*!< @brief Release date (\c CFString) */
			static const CFStringRef kCompilationKey;				/*!< @brief Compilation flag (\c CFBoolean) */
			static const CFStringRef kTrackNumberKey;				/*!< @brief Track number (\c CFNumber) */
			static const CFStringRef kTrackTotalKey;				/*!< @brief Track total (\c CFNumber) */
			static const CFStringRef kDiscNumberKey;				/*!< @brief Disc number (\c CFNumber) */
			static const CFStringRef kDiscTotalKey;					/*!< @brief Disc total (\c CFNumber) */
			static const CFStringRef kLyricsKey;					/*!< @brief Lyrics (\c CFString) */
			static const CFStringRef kBPMKey;						/*!< @brief Beats per minute (BPM) (\c CFNumber) */
			static const CFStringRef kRatingKey;					/*!< @brief Rating (\c CFNumber) */
			static const CFStringRef kCommentKey;					/*!< @brief Comment (\c CFString) */
			static const CFStringRef kISRCKey;						/*!< @brief International Standard Recording Code (ISRC) (\c CFString) */
			static const CFStringRef kMCNKey;						/*!< @brief Media Catalog Number (MCN) (\c CFString) */
			static const CFStringRef kMusicBrainzReleaseIDKey;		/*!< @brief MusicBrainz release ID (\c CFString) */
			static const CFStringRef kMusicBrainzRecordingIDKey;	/*!< @brief MusicBrainz recording ID (\c CFString) */
			//@}

			/*! @name Sorting dictionary keys */
			//@{
			static const CFStringRef kTitleSortOrderKey;			/*!< @brief Title sort order (\c CFString) */
			static const CFStringRef kAlbumTitleSortOrderKey;		/*!< @brief Album title sort order (\c CFString) */
			static const CFStringRef kArtistSortOrderKey;			/*!< @brief Artist sort order (\c CFString) */
			static const CFStringRef kAlbumArtistSortOrderKey;		/*!< @brief Album artist sort order (\c CFString) */
			static const CFStringRef kComposerSortOrderKey;			/*!< @brief Composer sort order (\c CFString) */
			//@}

			/*! @name Grouping dictionary keys */
			//@{
			static const CFStringRef kGroupingKey;					/*!< @brief Grouping (\c CFString) */
			//@}

			/*! @name Additional metadata dictionary keys */
			//@{
			static const CFStringRef kAdditionalMetadataKey;		/*!< @brief Additional metadata (\c CFDictionary) */
			//@}

			/*! @name Replay gain dictionary keys */
			//@{
			static const CFStringRef kReferenceLoudnessKey;			/*!< @brief Replay gain reference loudness (\c CFNumber) */
			static const CFStringRef kTrackGainKey;					/*!< @brief Replay gain track gain (\c CFNumber) */
			static const CFStringRef kTrackPeakKey;					/*!< @brief Replay gain track peak (\c CFNumber) */
			static const CFStringRef kAlbumGainKey;					/*!< @brief Replay gain album gain (\c CFNumber) */
			static const CFStringRef kAlbumPeakKey;					/*!< @brief Replay gain album peak (\c CFNumber) */
			//@}


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


			/*! @brief Test whether a file extension is supported */
			static bool HandlesFilesWithExtension(CFStringRef extension);

			/*! @brief Test whether a MIME type is supported */
			static bool HandlesMIMEType(CFStringRef mimeType);

			//@}


			// ========================================
			/*! @name Factory Methods */
			//@{

			/*! @brief A \c std::vector of \c AttachedPicture::shared_ptr objects */
			using picture_vector = std::vector<AttachedPicture::shared_ptr>;

			/*! @brief A \c std::unique_ptr for \c Metadata objects */
			using unique_ptr = std::unique_ptr<Metadata>;

			/*!
			 * @brief Create a \c Metadata object for the specified URL
			 * @param url The URL
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Metadata object, or \c nullptr on failure
			 */
			static unique_ptr CreateMetadataForURL(CFURLRef url, CFErrorRef *error = nullptr);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Destroy this \c Metadata */
			inline virtual ~Metadata() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			Metadata(const Metadata& rhs) = delete;

			/*! @internal This class is non-assignable */
			Metadata& operator=(const Metadata& rhs) = delete;

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
			bool ReadMetadata(CFErrorRef *error = nullptr);

			/*!
			 * @brief Write the metadata
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, \c false otherwise
			 */
			bool WriteMetadata(CFErrorRef *error = nullptr);

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
			/*! @name Metadata manipulation */
			//@{

			/*! @brief Metadata kind bitmask values used in CopyMetadata() and RemoveMetadata() */
			enum MetadataKind : unsigned {
				Basic			= (1u << 0),	/*!< Basic metadata */
				Sorting			= (1u << 1),	/*!< Sorting metadata */
				Grouping		= (1u << 2),	/*!< Grouping metadata */
				Additional		= (1u << 3),	/*!< Additional metadata */
				ReplayGain		= (1u << 4)		/*!< Replay gain metadata */
			};

			/*!
			 * @brief Copies the values contained in the specified metadata kinds from \c metadata
			 * @note Does not copy album artwork
			 * @param metadata A \c Metadata object containing the metadata to copy
			 * @see CopyAllMetadata
			 * @see CopyAttachedPictures
			 */
			void CopyMetadata(const Metadata& metadata, unsigned kind);

			/*!
			 * @brief Copies all metadata from \c metadata
			 * @note Does not copy album artwork
			 * @param metadata A \c Metadata object containing the metadata to copy
			 * @see CopyMetadata
			 * @see CopyAttachedPictures
			 */
			void CopyAllMetadata(const Metadata& metadata);


			/*!
			 * @brief Sets the values contained in specified metadata kinds to \c nullptr
			 * @param kind A bitmask specifying the kinds of metadata to remove
			 * @see RemoveAllMetadata
			 * @see RemoveAllAttachedPictures
			 */
			void RemoveMetadata(unsigned kind);

			/*!
			 * @brief Sets all metadata to \c nullptr
			 * @note Leaves album art intact
			 * @see RemoveMetadata
			 * @see RemoveAllAttachedPictures
			 */
			void RemoveAllMetadata();

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
			 * @name Basic metadata
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
			 * @name Sorting metadata
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
			 * @name Grouping metadata
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
			 * @name Replay gain metadata
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

			/*!
			 * @brief Copies album artwork from \c metadata
			 * @note This clears existing album artwork
			 * @note Does not copy metadata
			 * @param metadata A \c Metadata object containing the artwork to copy
			 * @see CopyMetadata
			 */
			void CopyAttachedPictures(const Metadata& metadata);

			
			/*! @brief Get all attached pictures */
			const picture_vector GetAttachedPictures() const;

			/*! @brief Get all attached pictures of the specified type */
			const picture_vector GetAttachedPicturesOfType(AttachedPicture::Type type) const;


			/*! @brief Attach a picture */
			void AttachPicture(AttachedPicture::shared_ptr picture);

			/*! @brief Remove an attached picture */
			void RemoveAttachedPicture(AttachedPicture::shared_ptr picture);


			/*! @brief Remove all attached pictures of the specified type */
			void RemoveAttachedPicturesOfType(AttachedPicture::Type type);

			/*! @brief Remove all attached pictures */
			void RemoveAllAttachedPictures();

			//@}

		protected:

			SFB::CFURL						mURL;				/*!< @brief The location of the stream to be read/written */

			SFB::CFMutableDictionary		mMetadata;			/*!< @brief The metadata information */
			SFB::CFMutableDictionary		mChangedMetadata;	/*!< @brief The metadata information that has been changed but not saved */

			picture_vector					mPictures;			/*!< @brief The attached picture information */


			/*! @brief Create a new \c Metadata and initialize \c Metadata::mURL to \c nullptr */
			Metadata();

			/*! @brief Create a new \c Metadata and initialize \c Metadata::mURL to \c url */
			Metadata(CFURLRef url);


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
			// Subclasses must implement the following methods
			virtual bool _ReadMetadata(CFErrorRef *error) = 0;
			virtual bool _WriteMetadata(CFErrorRef *error) = 0;

			// ========================================
			// 
			void ClearAllMetadata();
			void MergeChangedMetadataIntoMetadata();


			// ========================================
			// Subclass registration support
			struct SubclassInfo
			{
				CFArrayRef (*mCreateSupportedFileExtensions)();
				CFArrayRef (*mCreateSupportedMIMETypes)();
				
				bool (*mHandlesFilesWithExtension)(CFStringRef);
				bool (*mHandlesMIMEType)(CFStringRef);
				
				unique_ptr (*mCreateMetadata)(CFURLRef);
				
				int mPriority;
			};
			
			static std::vector <SubclassInfo> sRegisteredSubclasses;
			
		public:
			
			/*!
			 * @brief Register a \c Metadata subclass
			 * @tparam T The subclass name
			 * @param priority The priority of the subclass
			 */
			template <typename T> static void RegisterSubclass(int priority = 0);
			
		};
		
		// ========================================
		// Template implementation
		template <typename T> void Metadata::RegisterSubclass(int priority)
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
		
	}
}
