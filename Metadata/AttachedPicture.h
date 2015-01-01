/*
 *  Copyright (C) 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
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
#include <memory>

#include "CFWrapper.h"

/*! @file AttachedPicture.h @brief Support for attached pictures */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief A class encapsulating a single attached picture.
		 *
		 * Most file formats may have more than one attached picture of each type.
		 */
		class AttachedPicture
		{

			friend class Metadata;

		public:

			/*! @name Metadata Dictionary Keys */
			//@{
			static const CFStringRef kTypeKey;			/*!< @brief Picture type (\c CFNumber) */
			static const CFStringRef kDescriptionKey;	/*!< @brief Picture description (\c CFString) */
			static const CFStringRef kDataKey;			/*!< @brief Picture data (\c CFData) */
			//@}

			// ========================================
			/*! @brief The function or content of a picture */
			enum class Type : unsigned int {
				Other				= 0x00,		/*!< A type not otherwise enumerated */
				FileIcon			= 0x01,		/*!< 32x32 PNG image that should be used as the file icon */
				OtherFileIcon		= 0x02,		/*!< File icon of a different size or format */
				FrontCover			= 0x03,		/*!< Front cover image of the album */
				BackCover			= 0x04,		/*!< Back cover image of the album */
				LeafletPage			= 0x05,		/*!< Inside leaflet page of the album */
				Media				= 0x06,		/*!< Image from the album itself */
				LeadArtist			= 0x07,		/*!< Picture of the lead artist or soloist */
				Artist				= 0x08,		/*!< Picture of the artist or performer */
				Conductor			= 0x09,		/*!< Picture of the conductor */
				Band				= 0x0A,		/*!< Picture of the band or orchestra */
				Composer			= 0x0B,		/*!< Picture of the composer */
				Lyricist			= 0x0C,		/*!< Picture of the lyricist or text writer */
				RecordingLocation	= 0x0D,		/*!< Picture of the recording location or studio */
				DuringRecording		= 0x0E,		/*!< Picture of the artists during recording */
				DuringPerformance	= 0x0F,		/*!< Picture of the artists during performance */
				MovieScreenCapture	= 0x10,		/*!< Picture from a movie or video related to the track */
				ColouredFish		= 0x11,		/*!< Picture of a large, coloured fish */
				Illustration		= 0x12,		/*!< Illustration related to the track */
				BandLogo			= 0x13,		/*!< Logo of the band or performer */
				PublisherLogo		= 0x14		/*!< Logo of the publisher (record company) */
			};

			/*! @brief A \c std::shared_ptr for \c AttachedPicture objects */
			using shared_ptr = std::shared_ptr<AttachedPicture>;


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*!
			 * @brief Create a new \c AttachedPicture
			 * @param data The raw image data
			 * @param type An optional artwork type
			 * @param description An optional image description
			 */
			AttachedPicture(CFDataRef data = nullptr, AttachedPicture::Type type = Type::Other, CFStringRef description = nullptr);

			/*! @cond */

			/*! @internal This class is non-copyable */
			AttachedPicture(const AttachedPicture& rhs) = delete;

			/*! @internal This class is non-assignable */
			AttachedPicture& operator=(const AttachedPicture& rhs) = delete;

			/*! @endcond */
			//@}


			// ========================================
			/*!
			 * @name Picture information
			 * To remove an existing value call the appropriate \c Set() function with \c nullptr
			 */
			//@{

			/*! @brief Get the artwork type */
			Type GetType() const;

			/*! @brief Set the artwork type */
			void SetType(Type type);


			/*! @brief Get the image description */
			CFStringRef GetDescription() const;

			/*! @brief Set the image description */
			void SetDescription(CFStringRef description);


			/*! @brief Get the image data */
			CFDataRef GetData() const;

			/*! @brief Set the image data */
			void SetData(CFDataRef data);

			//@}


			// ========================================
			/*! @name Change management */
			//@{

			/*! @brief Query the object for unsaved changes */
			inline bool HasUnsavedChanges() const					{ return (0 != CFDictionaryGetCount(mChangedMetadata));}

			/*! @brief Revert unsaved changes */
			inline void RevertUnsavedChanges()						{ CFDictionaryRemoveAllValues(mChangedMetadata); }

			/*! @brief Query a particular key for unsaved changes */
			inline bool HasUnsavedChangesForKey(CFStringRef key) const { return CFDictionaryContainsKey(mChangedMetadata, key); }

			//@}


		protected:

			/*! @brief For AudioMetadata change tracking */
			enum class ChangeState {
				Saved,		/*!< The picure is saved */
				Added,		/*!< The picture is added but not yet saved */
				Removed		/*!< The picture has been removed but not yet saved*/
			};

			SFB::CFMutableDictionary		mMetadata;			/*!< @brief The metadata information */
			SFB::CFMutableDictionary		mChangedMetadata;	/*!< @brief The metadata information that has been changed but not saved */
			ChangeState						mState;				/*!< @brief The state of the picture relative to the saved file */

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

			/*!
			 * @brief Retrieve data from the metadata dictionary
			 * @param key The key to retrieve
			 * @return The value associated with \c key if present and data, \c nullptr otherwise
			 */
			CFDataRef GetDataValue(CFStringRef key) const;

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
		};
		
	}
}
