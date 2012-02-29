/*
 *  Copyright (C) 2012 Stephen F. Booth <me@sbooth.org>
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

// ========================================
// Key names for the metadata dictionary (for use with HasUnsavedChangesForKey())
// ========================================
extern const CFStringRef		kAttachedPictureTypeKey;
extern const CFStringRef		kAttachedPictureDescriptionKey;
extern const CFStringRef		kAttachedPictureDataKey;

// ========================================
// This class encapsulates a single attached picture.  Most file formats may have
// more than one attached picture of each type.
// ========================================
class AttachedPicture
{

	friend class AudioMetadata;

public:
	// ========================================
	// This describes the function or content of the picture
	enum class Type : unsigned int {
		Other				= 0x00,		// A type not enumerated below
		FileIcon			= 0x01,		// 32x32 PNG image that should be used as the file icon
		OtherFileIcon		= 0x02,		// File icon of a different size or format
		FrontCover			= 0x03,		// Front cover image of the album
		BackCover			= 0x04,		// Back cover image of the album
		LeafletPage			= 0x05,		// Inside leaflet page of the album
		Media				= 0x06,		// Image from the album itself
		LeadArtist			= 0x07,		// Picture of the lead artist or soloist
		Artist				= 0x08,		// Picture of the artist or performer
		Conductor			= 0x09,		// Picture of the conductor
		Band				= 0x0A,		// Picture of the band or orchestra
		Composer			= 0x0B,		// Picture of the composer
		Lyricist			= 0x0C,		// Picture of the lyricist or text writer
		RecordingLocation	= 0x0D,		// Picture of the recording location or studio
		DuringRecording		= 0x0E,		// Picture of the artists during recording
		DuringPerformance	= 0x0F,		// Picture of the artists during performance
		MovieScreenCapture	= 0x10,		// Picture from a movie or video related to the track
		ColouredFish		= 0x11,		// Picture of a large, coloured fish
		Illustration		= 0x12,		// Illustration related to the track
		BandLogo			= 0x13,		// Logo of the band or performer
		PublisherLogo		= 0x14		// Logo of the publisher (record company)
	};

	AttachedPicture(CFDataRef data = nullptr, AttachedPicture::Type type = Type::Other, CFStringRef description = nullptr);
	~AttachedPicture();

	// ========================================
	// Picture information
	Type GetType() const;
	void SetType(Type type);

	CFStringRef GetDescription() const;
	void SetDescription(CFStringRef description);

	CFDataRef GetData() const;
	void SetData(CFDataRef data);

	// ========================================
	// Change management
	inline bool HasUnsavedChanges() const					{ return (0 != CFDictionaryGetCount(mChangedMetadata));}
	inline void RevertUnsavedChanges()						{ CFDictionaryRemoveAllValues(mChangedMetadata); }
	
	inline bool HasUnsavedChangesForKey(CFStringRef key) const { return CFDictionaryContainsKey(mChangedMetadata, key); }

protected:
	// This class is non-copyable
	AttachedPicture(const AttachedPicture& rhs) = delete;
	AttachedPicture& operator=(const AttachedPicture& rhs) = delete;

	// For AudioMetadata change tracking
	// Valid states are { Saved, Added, Saved | Removed, Added | Removed }
	enum ChangeState {
		Saved		= 1 << 0,
		Added		= 1 << 1,
		Removed		= 1 << 2
	};

	// ========================================
	// Data members
	CFMutableDictionaryRef			mMetadata;			// The metadata information
	CFMutableDictionaryRef			mChangedMetadata;	// The metadata information that has been changed but not saved
	unsigned int					mState;				// The state of the picture

	// Subclasses should call this after a successful save operation
	void MergeChangedMetadataIntoMetadata();

	// Type-specific access
	CFStringRef GetStringValue(CFStringRef key) const;
	CFNumberRef GetNumberValue(CFStringRef key) const;
	CFDataRef GetDataValue(CFStringRef key) const;

	// Generic access
	CFTypeRef GetValue(CFStringRef key) const;
	void SetValue(CFStringRef key, CFTypeRef value);
};
