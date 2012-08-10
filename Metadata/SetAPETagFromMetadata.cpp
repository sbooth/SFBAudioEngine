/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <taglib/flacpicture.h>
#include <ApplicationServices/ApplicationServices.h>

#include "AudioMetadata.h"
#include "SetAPETagFromMetadata.h"
#include "TagLibStringUtilities.h"
#include "Base64Utilities.h"
#include "Logger.h"

// ========================================
// APE tag utilities
// ========================================
static bool
SetAPETag(TagLib::APE::Tag *tag, const char *key, CFStringRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	// Remove the existing comment with this name
	tag->removeItem(key);
	
	// Nothing left to do if value is nullptr
	if(nullptr == value)
		return true;
	
	tag->addValue(key, TagLib::StringFromCFString(value));
	
	return true;
}

static bool
SetAPETagNumber(TagLib::APE::Tag *tag, const char *key, CFNumberRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	CFStringRef numberString = nullptr;
	
	if(nullptr != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), value);
	
	bool result = SetAPETag(tag, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = nullptr;
	
	return result;
}

static bool
SetAPETagBoolean(TagLib::APE::Tag *tag, const char *key, CFBooleanRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	if(nullptr == value)
		return SetAPETag(tag, key, nullptr);
	else if(CFBooleanGetValue(value))
		return SetAPETag(tag, key, CFSTR("1"));
	else
		return SetAPETag(tag, key, CFSTR("0"));
}

static bool
SetAPETagDouble(TagLib::APE::Tag *tag, const char *key, CFNumberRef value, CFStringRef format = nullptr)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	CFStringRef numberString = nullptr;
	
	if(nullptr != value) {
		double f;
		if(!CFNumberGetValue(value, kCFNumberDoubleType, &f))
			LOGGER_INFO("org.sbooth.AudioEngine", "CFNumberGetValue returned an approximation");

		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, nullptr == format ? CFSTR("%f") : format, f);
	}
	
	bool result = SetAPETag(tag, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = nullptr;
	
	return result;
}

bool
SetAPETagFromMetadata(const AudioMetadata& metadata, TagLib::APE::Tag *tag, bool setAlbumArt)
{
	if(nullptr == tag)
		return false;

	// Standard tags
	SetAPETag(tag, "ALBUM", metadata.GetAlbumTitle());
	SetAPETag(tag, "ARTIST", metadata.GetArtist());
	SetAPETag(tag, "ALBUMARTIST", metadata.GetAlbumArtist());
	SetAPETag(tag, "COMPOSER", metadata.GetComposer());
	SetAPETag(tag, "GENRE", metadata.GetGenre());
	SetAPETag(tag, "DATE", metadata.GetReleaseDate());
	SetAPETag(tag, "DESCRIPTION", metadata.GetComment());
	SetAPETag(tag, "TITLE", metadata.GetTitle());
	SetAPETagNumber(tag, "TRACKNUMBER", metadata.GetTrackNumber());
	SetAPETagNumber(tag, "TRACKTOTAL", metadata.GetTrackTotal());
	SetAPETagBoolean(tag, "COMPILATION", metadata.GetCompilation());
	SetAPETagNumber(tag, "DISCNUMBER", metadata.GetDiscNumber());
	SetAPETagNumber(tag, "DISCTOTAL", metadata.GetDiscTotal());
	SetAPETagNumber(tag, "BPM", metadata.GetBPM());
	SetAPETagNumber(tag, "RATING", metadata.GetRating());
	SetAPETag(tag, "ISRC", metadata.GetISRC());
	SetAPETag(tag, "MCN", metadata.GetMCN());
	SetAPETag(tag, "TITLESORT", metadata.GetTitleSortOrder());
	SetAPETag(tag, "ALBUMTITLESORT", metadata.GetAlbumTitleSortOrder());
	SetAPETag(tag, "ARTISTSORT", metadata.GetArtistSortOrder());
	SetAPETag(tag, "ALBUMARTISTSORT", metadata.GetAlbumArtistSortOrder());
	SetAPETag(tag, "COMPOSERSORT", metadata.GetComposerSortOrder());
	SetAPETag(tag, "GROUPING", metadata.GetGrouping());

	// Additional metadata
	CFDictionaryRef additionalMetadata = metadata.GetAdditionalMetadata();
	if(nullptr != additionalMetadata) {
		CFIndex count = CFDictionaryGetCount(additionalMetadata);
		
		const void * keys [count];
		const void * values [count];
		
		CFDictionaryGetKeysAndValues(additionalMetadata, reinterpret_cast<const void **>(keys), reinterpret_cast<const void **>(values));
		
		for(CFIndex i = 0; i < count; ++i) {
			CFIndex keySize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(reinterpret_cast<CFStringRef>(keys[i])), kCFStringEncodingASCII);
			char key [keySize + 1];
			
			if(!CFStringGetCString(reinterpret_cast<CFStringRef>(keys[i]), key, keySize + 1, kCFStringEncodingASCII)) {
				LOGGER_ERR("org.sbooth.AudioEngine", "CFStringGetCString failed");
				continue;
			}
			
			SetAPETag(tag, key, reinterpret_cast<CFStringRef>(values[i]));
		}
	}
	
	// ReplayGain info
	SetAPETagDouble(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_TRACK_GAIN", metadata.GetReplayGainTrackGain(), CFSTR("%+2.2f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_TRACK_PEAK", metadata.GetReplayGainTrackPeak(), CFSTR("%1.8f"));
	SetAPETagDouble(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.GetReplayGainAlbumPeak(), CFSTR("%1.8f"));

	// Album art
	if(setAlbumArt) {
		tag->removeItem("Cover Art (Front)");
		tag->removeItem("Cover Art (Back)");
#if 0
		tag->removeItem("METADATA_BLOCK_PICTURE");
#endif

		for(auto attachedPicture : metadata.GetAttachedPictures()) {
			// APE can handle front and back covers natively
			if(AttachedPicture::Type::FrontCover == attachedPicture->GetType() || AttachedPicture::Type::FrontCover == attachedPicture->GetType()) {
				TagLib::ByteVector data;
				
				if(attachedPicture->GetDescription())
					data.append(TagLib::StringFromCFString(attachedPicture->GetDescription()).data(TagLib::String::UTF8));
				data.append('\0');
				data.append(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (TagLib::uint)CFDataGetLength(attachedPicture->GetData())));

				if(AttachedPicture::Type::FrontCover == attachedPicture->GetType())
					tag->setData("Cover Art (Front)", data);
				else if(AttachedPicture::Type::BackCover == attachedPicture->GetType())
					tag->setData("Cover Art (Back)", data);
			}
#if 0
			else {
				CGImageSourceRef imageSource = CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr);
				if(nullptr == imageSource)
					return false;

				TagLib::FLAC::Picture picture;
				picture.setData(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (TagLib::uint)CFDataGetLength(attachedPicture->GetData())));
				picture.setType(static_cast<TagLib::FLAC::Picture::Type>(attachedPicture->GetType()));
				if(attachedPicture->GetDescription())
					picture.setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));

				// Convert the image's UTI into a MIME type
				CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
				if(mimeType) {
					picture.setMimeType(TagLib::StringFromCFString(mimeType));
					CFRelease(mimeType), mimeType = nullptr;
				}

				// Flesh out the height, width, and depth
				CFDictionaryRef imagePropertiesDictionary = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr);
				if(imagePropertiesDictionary) {
					CFNumberRef imageWidth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelWidth);
					CFNumberRef imageHeight = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelHeight);
					CFNumberRef imageDepth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyDepth);
					
					int height, width, depth;
					
					// Ignore numeric conversion errors
					CFNumberGetValue(imageWidth, kCFNumberIntType, &width);
					CFNumberGetValue(imageHeight, kCFNumberIntType, &height);
					CFNumberGetValue(imageDepth, kCFNumberIntType, &depth);
					
					picture.setHeight(height);
					picture.setWidth(width);
					picture.setColorDepth(depth);
					
					CFRelease(imagePropertiesDictionary), imagePropertiesDictionary = nullptr;
				}

				TagLib::ByteVector encodedBlock = TagLib::EncodeBase64(picture.render());
				tag->addValue("METADATA_BLOCK_PICTURE", TagLib::String(encodedBlock, TagLib::String::UTF8), false);

				CFRelease(imageSource), imageSource = nullptr;
			}
#endif
		}
	}

	return true;
}
