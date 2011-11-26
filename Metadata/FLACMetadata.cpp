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

#include <FLAC/metadata.h>
#include <ApplicationServices/ApplicationServices.h>

#include "FLACMetadata.h"
#include "CreateDisplayNameForURL.h"
#include "Logger.h"

// ========================================
// Vorbis comment utilities
// ========================================
static bool
SetVorbisComment(FLAC__StreamMetadata *block, const char *key, CFStringRef value)
{
	assert(NULL != block);
	assert(NULL != key);

	// Remove the existing comment with this name
	if(-1 == FLAC__metadata_object_vorbiscomment_remove_entry_matching(block, key)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_vorbiscomment_remove_entry_matching() failed");
		return false;
	}
	
	// Nothing left to do if value is NULL
	if(NULL == value)
		return true;
	
	CFIndex valueCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)  + 1;
	char valueCString [valueCStringSize];
	
	if(!CFStringGetCString(value, valueCString, valueCStringSize, kCFStringEncodingUTF8)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "CFStringGetCString() failed");
		return false;
	}
	
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	
	if(!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, key, valueCString)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair() failed");
		return false;
	}
	
	if(!FLAC__metadata_object_vorbiscomment_replace_comment(block, entry, false, false)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_vorbiscomment_replace_comment() failed");
		return false;
	}
	
	return true;
}

static bool
SetVorbisCommentNumber(FLAC__StreamMetadata *block, const char *key, CFNumberRef value)
{
	assert(NULL != block);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), value);
	
	bool result = SetVorbisComment(block, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = NULL;
	
	return result;
}

static bool
SetVorbisCommentBoolean(FLAC__StreamMetadata *block, const char *key, CFBooleanRef value)
{
	assert(NULL != block);
	assert(NULL != key);
	
	if(NULL == value)
		return SetVorbisComment(block, key, NULL);
	else if(CFBooleanGetValue(value))
		return SetVorbisComment(block, key, CFSTR("1"));
	else
		return SetVorbisComment(block, key, CFSTR("0"));
}

static bool
SetVorbisCommentDouble(FLAC__StreamMetadata *block, const char *key, CFNumberRef value, CFStringRef format = NULL)
{
	assert(NULL != block);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value) {
		double f;
		if(!CFNumberGetValue(value, kCFNumberDoubleType, &f)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "CFNumberGetValue() failed");
			return false;
		}

		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, NULL == format ? CFSTR("%f") : format, f);
	}
	
	bool result = SetVorbisComment(block, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = NULL;
	
	return result;
}

#pragma mark Static Methods

CFArrayRef FLACMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("flac") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef FLACMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/flac") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool FLACMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(NULL == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(NULL == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/flac"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

#pragma mark Creation and Destruction

FLACMetadata::FLACMetadata(CFURLRef url)
	: AudioMetadata(url)
{}

FLACMetadata::~FLACMetadata()
{}

#pragma mark Functionality

bool FLACMetadata::ReadMetadata(CFErrorRef *error)
{
	// Start from scratch
	CFDictionaryRemoveAllValues(mMetadata);
	CFDictionaryRemoveAllValues(mChangedMetadata);
	
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	// ENOMEM sux
	if(NULL == chain)
		return false;
	
	if(!FLAC__metadata_chain_read(chain, reinterpret_cast<const char *>(buf))) {

		// Attempt to provide a descriptive error message
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			switch(FLAC__metadata_chain_status(chain)) {
				case FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);

					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
					
					
				case FLAC__METADATA_CHAIN_STATUS_BAD_METADATA:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file contains bad metadata."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
					
				default:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
			}
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataFileFormatNotRecognizedError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;
		}

		FLAC__metadata_chain_delete(chain), chain = NULL;
		
		return false;
	}
	
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

	if(NULL == iterator) {
		FLAC__metadata_chain_delete(chain), chain = NULL;		
		return false;
	}
	
	CFDictionarySetValue(mMetadata, kPropertiesFormatNameKey, CFSTR("FLAC"));

	FLAC__metadata_iterator_init(iterator, chain);
	
	FLAC__StreamMetadata *block = NULL;
	
	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																		  0,
																		  &kCFTypeDictionaryKeyCallBacks,
																		  &kCFTypeDictionaryValueCallBacks);
	
	do {
		block = FLAC__metadata_iterator_get_block(iterator);
		
		if(NULL == block)
			break;
		
		switch(block->type) {					
			case FLAC__METADATA_TYPE_VORBIS_COMMENT:				
				for(FLAC__uint32 i = 0; i < block->data.vorbis_comment.num_comments; ++i) {
					
					char *fieldName = NULL;
					char *fieldValue = NULL;
					
					// Let FLAC parse the comment for us
					if(!FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(block->data.vorbis_comment.comments[i], &fieldName, &fieldValue)) {
						// Ignore malformed comments
						continue;
					}
					
					CFStringRef key = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
																	reinterpret_cast<const UInt8 *>(fieldName),
																	strlen(fieldName), 
																	kCFStringEncodingASCII,
																	false,
																	kCFAllocatorMalloc);

					CFStringRef value = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
																	  reinterpret_cast<const UInt8 *>(fieldValue),
																	  strlen(fieldValue), 
																	  kCFStringEncodingUTF8,
																	  false,
																	  kCFAllocatorMalloc);
					
					if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUM"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataAlbumTitleKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataArtistKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataAlbumArtistKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataComposerKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataGenreKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DATE"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataReleaseDateKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataCommentKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataTitleKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKNUMBER"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataTrackNumberKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataTrackTotalKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataCompilationKey, CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataDiscNumberKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataDiscTotalKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("LYRICS"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataLyricsKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("BPM"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataBPMKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("RATING"), kCFCompareCaseInsensitive)) {
						int num = CFStringGetIntValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
						CFDictionarySetValue(mMetadata, kMetadataRatingKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataISRCKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
						CFDictionarySetValue(mMetadata, kMetadataMCNKey, value);
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive)) {
						double num = CFStringGetDoubleValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
						CFDictionarySetValue(mMetadata, kReplayGainReferenceLoudnessKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive)) {
						double num = CFStringGetDoubleValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
						CFDictionarySetValue(mMetadata, kReplayGainTrackGainKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive)) {
						double num = CFStringGetDoubleValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
						CFDictionarySetValue(mMetadata, kReplayGainTrackPeakKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive)) {
						double num = CFStringGetDoubleValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
						CFDictionarySetValue(mMetadata, kReplayGainAlbumGainKey, number);
						CFRelease(number), number = NULL;
					}
					else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive)) {
						double num = CFStringGetDoubleValue(value);
						CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
						CFDictionarySetValue(mMetadata, kReplayGainAlbumPeakKey, number);
						CFRelease(number), number = NULL;
					}
					// Put all unknown tags into the additional metadata
					else
						CFDictionarySetValue(additionalMetadata, key, value);
					
					CFRelease(key), key = NULL;
					CFRelease(value), value = NULL;
					
					fieldName = NULL;
					fieldValue = NULL;
				}
				break;
				
			case FLAC__METADATA_TYPE_PICTURE:
				switch(block->data.picture.type) {
					case FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER:
					{
						CFDataRef data = CFDataCreate(kCFAllocatorDefault, block->data.picture.data, block->data.picture.data_length);
						CFDictionarySetValue(mMetadata, kAlbumArtFrontCoverKey, data);
						CFRelease(data), data = NULL;

						break;
					}
						
						// TODO: Other artwork types will be handled in the future
					default:												break;
				}
			break;
				
			case FLAC__METADATA_TYPE_STREAMINFO:
			{
				CFNumberRef sampleRate = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &block->data.stream_info.sample_rate);
				CFDictionarySetValue(mMetadata, kPropertiesSampleRateKey, sampleRate);
				CFRelease(sampleRate), sampleRate = NULL;

				CFNumberRef channelsPerFrame = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &block->data.stream_info.channels);
				CFDictionarySetValue(mMetadata, kPropertiesChannelsPerFrameKey, channelsPerFrame);
				CFRelease(channelsPerFrame), channelsPerFrame = NULL;

				CFNumberRef bitsPerChannel = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &block->data.stream_info.bits_per_sample);
				CFDictionarySetValue(mMetadata, kPropertiesBitsPerChannelKey, bitsPerChannel);
				CFRelease(bitsPerChannel), bitsPerChannel = NULL;
				
				CFNumberRef totalFrames = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &block->data.stream_info.total_samples);
				CFDictionarySetValue(mMetadata, kPropertiesTotalFramesKey, totalFrames);
				CFRelease(totalFrames), totalFrames = NULL;

				double length = static_cast<double>(block->data.stream_info.total_samples / block->data.stream_info.sample_rate);
				CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &length);
				CFDictionarySetValue(mMetadata, kPropertiesDurationKey, duration);
				CFRelease(duration), duration = NULL;

				double losslessBitrate = static_cast<double>(block->data.stream_info.sample_rate * block->data.stream_info.channels * block->data.stream_info.bits_per_sample) / 1000;
				CFNumberRef bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &losslessBitrate);
				CFDictionarySetValue(mMetadata, kPropertiesBitrateKey, bitrate);
				CFRelease(bitrate), bitrate = NULL;

				break;
			}

			case FLAC__METADATA_TYPE_PADDING:						break;
			case FLAC__METADATA_TYPE_APPLICATION:					break;
			case FLAC__METADATA_TYPE_SEEKTABLE:						break;
			case FLAC__METADATA_TYPE_CUESHEET:						break;
			case FLAC__METADATA_TYPE_UNDEFINED:						break;

			default:												break;
		}
	} while(FLAC__metadata_iterator_next(iterator));

	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(mMetadata, kMetadataAdditionalMetadataKey, additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = NULL;
	
	FLAC__metadata_iterator_delete(iterator), iterator = NULL;
	FLAC__metadata_chain_delete(chain), chain = NULL;
	
	return true;
}

bool FLACMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
	
	// ENOMEM sux
	if(NULL == chain)
		return false;
	
	if(!FLAC__metadata_chain_read(chain, reinterpret_cast<const char *>(buf))) {
		
		// Attempt to provide a descriptive error message
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			switch(FLAC__metadata_chain_status(chain)) {
				case FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
					
					
				case FLAC__METADATA_CHAIN_STATUS_BAD_METADATA:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file contains bad metadata."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
					
				default:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Not a FLAC file"), ""));
					
					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
					
					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;
					
					break;
				}
			}
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataFileFormatNotRecognizedError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;
		}
		
		FLAC__metadata_chain_delete(chain), chain = NULL;
		
		return false;
	}
	
	FLAC__metadata_chain_sort_padding(chain);
	
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
	
	if(NULL == iterator) {
		FLAC__metadata_chain_delete(chain), chain = NULL;

		return false;
	}
	
	FLAC__metadata_iterator_init(iterator, chain);
	
	// Locate the vorbis comment block if it exists
	while(FLAC__METADATA_TYPE_VORBIS_COMMENT != FLAC__metadata_iterator_get_block_type(iterator) && FLAC__metadata_iterator_next(iterator))
		;
	
	FLAC__StreamMetadata *block = NULL;
	
	// If there isn't a vorbis comment block add one
	if(FLAC__METADATA_TYPE_VORBIS_COMMENT != FLAC__metadata_iterator_get_block_type(iterator)) {
		
		block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
		
		if(NULL == block) {
			FLAC__metadata_chain_delete(chain), chain = NULL;
			FLAC__metadata_iterator_delete(iterator), iterator = NULL;

			return false;
		}
		
		// Add our metadata
		if(!FLAC__metadata_iterator_insert_block_after(iterator, block)) {
			if(NULL != error) {
				CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																				   0,
																				   &kCFTypeDictionaryKeyCallBacks,
																				   &kCFTypeDictionaryValueCallBacks);

				CFStringRef displayName = CreateDisplayNameForURL(mURL);
				CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																   NULL, 
																   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																   displayName);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedDescriptionKey, 
									 errorString);
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedFailureReasonKey, 
									 CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
				
				CFDictionarySetValue(errorDictionary, 
									 kCFErrorLocalizedRecoverySuggestionKey, 
									 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
				
				CFRelease(errorString), errorString = NULL;
				CFRelease(displayName), displayName = NULL;

				*error = CFErrorCreate(kCFAllocatorDefault, 
									   AudioMetadataErrorDomain, 
									   AudioMetadataInputOutputError, 
									   errorDictionary);
				
				CFRelease(errorDictionary), errorDictionary = NULL;				
			}
			
			FLAC__metadata_chain_delete(chain), chain = NULL;
			FLAC__metadata_iterator_delete(iterator), iterator = NULL;
			
			return false;
		}
	}
	else
		block = FLAC__metadata_iterator_get_block(iterator);
	
	// Standard tags
	SetVorbisComment(block, "ALBUM", GetAlbumTitle());
	SetVorbisComment(block, "ARTIST", GetArtist());
	SetVorbisComment(block, "ALBUMARTIST", GetAlbumArtist());
	SetVorbisComment(block, "COMPOSER", GetComposer());
	SetVorbisComment(block, "GENRE", GetGenre());
	SetVorbisComment(block, "DATE", GetReleaseDate());
	SetVorbisComment(block, "DESCRIPTION", GetComment());
	SetVorbisComment(block, "TITLE", GetTitle());
	SetVorbisCommentNumber(block, "TRACKNUMBER", GetTrackNumber());
	SetVorbisCommentNumber(block, "TRACKTOTAL", GetTrackTotal());
	SetVorbisCommentBoolean(block, "COMPILATION", GetCompilation());
	SetVorbisCommentNumber(block, "DISCNUMBER", GetDiscNumber());
	SetVorbisCommentNumber(block, "DISCTOTAL", GetDiscTotal());
	SetVorbisComment(block, "LYRICS", GetLyrics());
	SetVorbisCommentNumber(block, "BPM", GetBPM());
	SetVorbisCommentNumber(block, "RATING", GetRating());
	SetVorbisComment(block, "ISRC", GetISRC());
	SetVorbisComment(block, "MCN", GetMCN());

	// Additional metadata
	CFDictionaryRef additionalMetadata = GetAdditionalMetadata();
	if(NULL != additionalMetadata) {
		CFIndex count = CFDictionaryGetCount(additionalMetadata);
		
		const void * keys [count];
		const void * values [count];
		
		CFDictionaryGetKeysAndValues(additionalMetadata, 
									 reinterpret_cast<const void **>(keys), 
									 reinterpret_cast<const void **>(values));
		
		for(CFIndex i = 0; i < count; ++i) {
			CFIndex keySize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(reinterpret_cast<CFStringRef>(keys[i])), kCFStringEncodingASCII);
			char key [keySize + 1];
			       
			if(!CFStringGetCString(reinterpret_cast<CFStringRef>(keys[i]), key, keySize + 1, kCFStringEncodingASCII)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "CFStringGetCString() failed");
				continue;
			}
			
			SetVorbisComment(block, key, reinterpret_cast<CFStringRef>(values[i]));
		}
	}
	
	// ReplayGain info
	SetVorbisCommentDouble(block, "REPLAYGAIN_REFERENCE_LOUDNESS", GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetVorbisCommentDouble(block, "REPLAYGAIN_TRACK_GAIN", GetReplayGainReferenceLoudness(), CFSTR("%+2.2f dB"));
	SetVorbisCommentDouble(block, "REPLAYGAIN_TRACK_PEAK", GetReplayGainTrackGain(), CFSTR("%1.8f"));
	SetVorbisCommentDouble(block, "REPLAYGAIN_ALBUM_GAIN", GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetVorbisCommentDouble(block, "REPLAYGAIN_ALBUM_PEAK", GetReplayGainAlbumPeak(), CFSTR("%1.8f"));

	// Album art
	
	// Reset the iterator
	FLAC__metadata_iterator_init(iterator, chain);

	// Locate the front cover art block if it exists
	block = NULL;
	do {
		if(FLAC__METADATA_TYPE_PICTURE != FLAC__metadata_iterator_get_block_type(iterator))
			continue;

		// The fact that a FLAC stream can contain multiple pictures of all except two types is
		// conveniently ignored for now, and only the first front cover art block is used
		FLAC__StreamMetadata *localBlock = FLAC__metadata_iterator_get_block(iterator);
		if(FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER == localBlock->data.picture.type) {
			block = localBlock;
			break;
		}
	} while(FLAC__metadata_iterator_next(iterator));

	// Create or delete the front cover art
	if(GetFrontCoverArt()) {
		// Create the block
		if(NULL == block) {
			block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);

			if(NULL == block) {
				FLAC__metadata_chain_delete(chain), chain = NULL;
				FLAC__metadata_iterator_delete(iterator), iterator = NULL;

				return false;
			}

			// Add the block
			if(!FLAC__metadata_iterator_insert_block_after(iterator, block)) {
				if(NULL != error) {
					CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																					   0,
																					   &kCFTypeDictionaryKeyCallBacks,
																					   &kCFTypeDictionaryValueCallBacks);

					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
																	   displayName);

					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedDescriptionKey, 
										 errorString);

					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedFailureReasonKey, 
										 CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));

					CFDictionarySetValue(errorDictionary, 
										 kCFErrorLocalizedRecoverySuggestionKey, 
										 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

					CFRelease(errorString), errorString = NULL;
					CFRelease(displayName), displayName = NULL;

					*error = CFErrorCreate(kCFAllocatorDefault, 
										   AudioMetadataErrorDomain, 
										   AudioMetadataInputOutputError, 
										   errorDictionary);

					CFRelease(errorDictionary), errorDictionary = NULL;				
				}

				FLAC__metadata_chain_delete(chain), chain = NULL;
				FLAC__metadata_iterator_delete(iterator), iterator = NULL;

				return false;
			}
		}

		// Add the image data to the metadata block
		block->data.picture.type = FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER;

		CGImageSourceRef imageSource = CGImageSourceCreateWithData(GetFrontCoverArt(), NULL);
		if(NULL == imageSource) {
			FLAC__metadata_chain_delete(chain), chain = NULL;
			FLAC__metadata_iterator_delete(iterator), iterator = NULL;

			return false;
		}

		// Convert the image's UTI into a MIME type
		CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
		if(mimeType) {
			CFIndex mimeTypeCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(mimeType), kCFStringEncodingASCII);
			char mimeTypeCString [mimeTypeCStringSize + 1];

			if(CFStringGetCString(mimeType, mimeTypeCString, mimeTypeCStringSize + 1, kCFStringEncodingASCII)) {
				if(!FLAC__metadata_object_picture_set_mime_type(block, mimeTypeCString, true))
					LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_picture_set_mime_type() failed");				
			}
			else
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "CFStringGetCString() failed");

			CFRelease(mimeType), mimeType = NULL;
		}

		// Flesh out the height, width, and depth
		CFDictionaryRef imagePropertiesDictionary = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, NULL);
		if(imagePropertiesDictionary) {
			CFNumberRef imageWidth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelWidth);
			CFNumberRef imageHeight = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelHeight);
			CFNumberRef imageDepth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyDepth);

			// Ignore numeric conversion errors
			CFNumberGetValue(imageWidth, kCFNumberIntType, &block->data.picture.width);
			CFNumberGetValue(imageHeight, kCFNumberIntType, &block->data.picture.height);
			CFNumberGetValue(imageDepth, kCFNumberIntType, &block->data.picture.depth);

			CFRelease(imagePropertiesDictionary), imagePropertiesDictionary = NULL;
		}

		CFDataRef frontCoverData = GetFrontCoverArt();
		if(!FLAC__metadata_object_picture_set_data(block, (FLAC__byte *)CFDataGetBytePtr(frontCoverData), (FLAC__uint32)CFDataGetLength(frontCoverData), true))
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_picture_set_data() failed");

		// Validate the picture- if any of the above code failed validation will likely fail too
		const char *errorDescription = NULL;
		if(!FLAC__metadata_object_picture_is_legal(block, &errorDescription)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioMetadata.FLAC", "FLAC__metadata_object_picture_is_legal() failed: " << errorDescription);
			FLAC__metadata_iterator_delete_block(iterator, true);
		}
	}
	else if(block)
		FLAC__metadata_iterator_delete_block(iterator, true);

	// Coalesce the padding before saving
	FLAC__metadata_chain_sort_padding(chain);

	// Write the new metadata to the file
	if(!FLAC__metadata_chain_write(chain, true, false)) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid FLAC file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Unable to write metadata"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioMetadataErrorDomain, 
								   AudioMetadataInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		FLAC__metadata_chain_delete(chain), chain = NULL;
		FLAC__metadata_iterator_delete(iterator), iterator = NULL;
		
		return false;
	}
	
	FLAC__metadata_chain_delete(chain), chain = NULL;
	FLAC__metadata_iterator_delete(iterator), iterator = NULL;
	
	MergeChangedMetadataIntoMetadata();
	
	return true;
}
