/*
 *  Copyright (C) 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

/*! @file */

/*! @name Metadata Dictionary Keys */
//@{

/*! Picture type (\c CFNumber) */
extern const CFStringRef		kAttachedPictureTypeKey;
/*! Picture description (\c CFString) */
extern const CFStringRef		kAttachedPictureDescriptionKey;
/*! Picture data (\c CFData) */
extern const CFStringRef		kAttachedPictureDataKey;

//@}


/*! 
 * @brief A class encapsulating a single attached picture.  
 * Most file formats may have more than one attached picture of each type.
 */
class AttachedPicture
{

	friend class AudioMetadata;

public:
	// ========================================
	/*! The function or content of a picture */
	enum class Type : unsigned int {
		/*! A type not otherwise enumerated */
		Other				= 0x00,
		/*! 32x32 PNG image that should be used as the file icon */
		FileIcon			= 0x01,
		/*! File icon of a different size or format */
		OtherFileIcon		= 0x02,
		/*! Front cover image of the album */
		FrontCover			= 0x03,
		/*! Back cover image of the album */
		BackCover			= 0x04,
		/*! Inside leaflet page of the album */
		LeafletPage			= 0x05,
		/*! Image from the album itself */
		Media				= 0x06,
		/*! Picture of the lead artist or soloist */
		LeadArtist			= 0x07,
		/*! Picture of the artist or performer */
		Artist				= 0x08,
		/*! Picture of the conductor */
		Conductor			= 0x09,
		/*! Picture of the band or orchestra */
		Band				= 0x0A,
		/*! Picture of the composer */
		Composer			= 0x0B,
		/*! Picture of the lyricist or text writer */
		Lyricist			= 0x0C,
		/*! Picture of the recording location or studio */
		RecordingLocation	= 0x0D,
		/*! Picture of the artists during recording */
		DuringRecording		= 0x0E,
		/*! Picture of the artists during performance */
		DuringPerformance	= 0x0F,
		/*! Picture from a movie or video related to the track */
		MovieScreenCapture	= 0x10,
		/*! Picture of a large, coloured fish */
		ColouredFish		= 0x11,
		/*! Illustration related to the track */
		Illustration		= 0x12,
		/*! Logo of the band or performer */
		BandLogo			= 0x13,
		/*! Logo of the publisher (record company) */
		PublisherLogo		= 0x14
	};


	// ========================================
	/*! @name Creation and Destruction */
	//@{

	/*!
	 * Create a new \c AttachedPicture
	 * @param data The raw image data
	 * @param type An optional artwork type
	 * @param description An optional image description
	 */
	AttachedPicture(CFDataRef data = nullptr, AttachedPicture::Type type = Type::Other, CFStringRef description = nullptr);

	/*! Destroy this \c AttachedPicture */
	~AttachedPicture();

	/*! @cond */

	/*! @internal This class is non-copyable */
	AttachedPicture(const AttachedPicture& rhs) = delete;

	/*! @internal This class is non-assignable */
	AttachedPicture& operator=(const AttachedPicture& rhs) = delete;

	/*! @endcond */
	//@}


	// ========================================
	/*! @name Picture information */
	//@{

	/*!
	 * Get the artwork type
	 * @return The artwork type
	 */
	Type GetType() const;

	/*!
	 * Set the artwork type
	 * @param type The artwork type
	 */
	void SetType(Type type);


	/*!
	 * Get the image description
	 * @return The image description
	 */
	CFStringRef GetDescription() const;

	/*!
	 * Set the image description
	 * @param description The image description
	 */
	void SetDescription(CFStringRef description);


	/*!
	 * Get the image data
	 * @return The image data
	 */
	CFDataRef GetData() const;

	/*!
	 * Set the image data
	 * @param data The image data
	 */
	void SetData(CFDataRef data);

	//@}


	// ========================================
	/*! @name Change management */
	//@{

	/*!
	 * Query the object for unsaved changes
	 * @return \c true if there are unsaved changes, false otherwise
	 */
	inline bool HasUnsavedChanges() const					{ return (0 != CFDictionaryGetCount(mChangedMetadata));}

	/*! Revert unsaved changes */
	inline void RevertUnsavedChanges()						{ CFDictionaryRemoveAllValues(mChangedMetadata); }
	
	/*!
	 * Query a particular key for unsaved changes
	 * @param key The key to query
	 * @return \true if this object has unsaved changes for \c key, false otherwise
	 */
	inline bool HasUnsavedChangesForKey(CFStringRef key) const { return CFDictionaryContainsKey(mChangedMetadata, key); }

	//@}


protected:

	/*! For AudioMetadata change tracking */
	enum class ChangeState {
		/*! The picure is saved */
		Saved,
		/*! The picture is added but not yet saved */
		Added,
		/*! The picture has been removed but not yet saved*/
		Removed
	};


	/*! The metadata information */
	CFMutableDictionaryRef			mMetadata;

	/*! The metadata information that has been changed but not saved */
	CFMutableDictionaryRef			mChangedMetadata;

	/*! The state of the picture relative to the saved file */
	ChangeState						mState;


	/*! Subclasses should call this after a successful save operation */
	void MergeChangedMetadataIntoMetadata();


	/*! @name Type-specific access */
	//@{

	/*!
	 * Retrieve a string from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key if present and a string, \c nullptr otherwise
	 */
	CFStringRef GetStringValue(CFStringRef key) const;

	/*!
	 * Retrieve a number from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key if present and a number, \c nullptr otherwise
	 */
	CFNumberRef GetNumberValue(CFStringRef key) const;

	/*!
	 * Retrieve data from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key if present and data, \c nullptr otherwise
	 */
	CFDataRef GetDataValue(CFStringRef key) const;

	//@}


	/*! @name Generic access */
	//@{

	/*!
	 * Retrieve an object from the metadata dictionary
	 * @param key The key to retrieve
	 * @return The value associated with \c key
	 */
	CFTypeRef GetValue(CFStringRef key) const;

	/*!
	 * Set a value in the metadata dictionary
	 * @param key The key to associate with \c value
	 * @param value The value to set
	 */
	void SetValue(CFStringRef key, CFTypeRef value);

	//@}
};
