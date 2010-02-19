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

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioFormat.h>
#include <libkern/OSAtomic.h>
#include <stdexcept>
#include <typeinfo>

#include <FLAC/metadata.h>

#include "AudioEngineDefines.h"
#include "FLACMetadata.h"


// ========================================
// Get the localized display name for a URL
// ========================================
static CFStringRef
CreateDisplayNameForURL(CFURLRef url)
{
	assert(NULL != url);
	
	FSRef ref;
	Boolean success = CFURLGetFSRef(url, &ref);
	if(FALSE == success) {
		ERR("Unable to get FSRef for URL");
		
		return NULL;
	}
	
	CFStringRef displayName = NULL;
	OSStatus result = LSCopyItemAttribute(&ref, kLSRolesAll, kLSItemDisplayName, reinterpret_cast<CFTypeRef *>(&displayName));
	
	if(noErr != result) {
		ERR("LSCopyItemAttribute (kLSItemDisplayName) failed: %i", result);
		
		return NULL;
	}
	
	return displayName;
}

// ========================================
// Vorbis comment utilities
// ========================================
static bool
setVorbisComment(FLAC__StreamMetadata		*block,
				 CFStringRef				key,
				 CFStringRef				value)
{
	assert(NULL != block);
	assert(NULL != key);

	CFIndex keyCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key), kCFStringEncodingASCII)  + 1;
	char keyCString [keyCStringSize];
	
	if(false == CFStringGetCString(key, keyCString, keyCStringSize, kCFStringEncodingASCII)) {
		ERR("CFStringGetCString failed");
		return false;
	}
	
	// Remove the existing comment with this name
	if(-1 == FLAC__metadata_object_vorbiscomment_remove_entry_matching(block, keyCString)) {
		ERR("FLAC__metadata_object_vorbiscomment_remove_entry_matching() failed");
		return false;
	}
	
	// Nothing left to do if value is NULL
	if(NULL == value)
		return true;
	
	CFIndex valueCStringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8)  + 1;
	char valueCString [valueCStringSize];
	
	if(false == CFStringGetCString(value, valueCString, valueCStringSize, kCFStringEncodingUTF8)) {
		ERR("CFStringGetCString failed");
		return false;
	}
	
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	
	if(false == FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, keyCString, valueCString)) {
		ERR("FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair failed");
		return false;
	}
	
	if(false == FLAC__metadata_object_vorbiscomment_replace_comment(block, entry, false, false)) {
		ERR("FLAC__metadata_object_vorbiscomment_replace_comment failed");
		return false;
	}
	
	return true;
}

static bool
setVorbisCommentNumber(FLAC__StreamMetadata		*block,
					   CFStringRef				key,
					   CFNumberRef				value,
					   CFStringRef				format = NULL)
{
	assert(NULL != block);
	assert(NULL != key);
	
	CFStringRef numberString = NULL;
	
	if(NULL != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, 
												NULL, 
												(NULL == format ? CFSTR("%@") : format), 
												value);
	
	bool result = setVorbisComment(block, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = NULL;
	
	return result;
}

static bool
setVorbisCommentBoolean(FLAC__StreamMetadata	*block,
						CFStringRef				key,
						CFBooleanRef			value)
{
	assert(NULL != block);
	assert(NULL != key);
	
	if(CFBooleanGetValue(value))
		return setVorbisComment(block, key, CFSTR("1"));
	else
		return setVorbisComment(block, key, CFSTR("0"));
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
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("flac"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool FLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
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
	
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(FALSE == success)
		return false;
	
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	// ENOMEM sux
	if(NULL == chain)
		return false;
	
	if(false == FLAC__metadata_chain_read(chain, reinterpret_cast<const char *>(buf))) {

		// Attempt to provide a descriptive error message
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			switch(FLAC__metadata_chain_status(chain)) {
				case FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
	
	FLAC__metadata_iterator_init(iterator, chain);
	
	FLAC__StreamMetadata *block = NULL;
	
	do {
		block = FLAC__metadata_iterator_get_block(iterator);
		
		if(NULL == block)
			break;
		
		switch(block->type) {					
			case FLAC__METADATA_TYPE_VORBIS_COMMENT:				
				for(unsigned i = 0; i < block->data.vorbis_comment.num_comments; ++i) {
					
					char *fieldName = NULL;
					char *fieldValue = NULL;
					
					// Let FLAC parse the comment for us
					if(false == FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(block->data.vorbis_comment.comments[i], &fieldName, &fieldValue)) {
						// Ignore malformed comments
						continue;
					}
					
					CFStringRef key = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
																	reinterpret_cast<const UInt8 *>(fieldName),
																	strlen(fieldName), 
																	kCFStringEncodingASCII,
																	FALSE,
																	kCFAllocatorMalloc);

					CFStringRef value = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
																	  reinterpret_cast<const UInt8 *>(fieldValue),
																	  strlen(fieldValue), 
																	  kCFStringEncodingUTF8,
																	  FALSE,
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
						CFDictionarySetValue(mMetadata, kMetadataTrackTotalKey, CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
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
					
					CFRelease(key);
					CFRelease(value);
					
					fieldName	= NULL;
					fieldValue	= NULL;
				}
				break;
				
			case FLAC__METADATA_TYPE_PICTURE:
			{
				CFDataRef data = CFDataCreate(kCFAllocatorDefault, block->data.picture.data, block->data.picture.data_length);
				CFDictionarySetValue(mMetadata, kAlbumArtFrontCoverKey, data);
				CFRelease(data), data = NULL;
			}
			break;
				
			case FLAC__METADATA_TYPE_STREAMINFO:					break;
			case FLAC__METADATA_TYPE_PADDING:						break;
			case FLAC__METADATA_TYPE_APPLICATION:					break;
			case FLAC__METADATA_TYPE_SEEKTABLE:						break;
			case FLAC__METADATA_TYPE_CUESHEET:						break;
			case FLAC__METADATA_TYPE_UNDEFINED:						break;

			default:												break;
		}
	} while(FLAC__metadata_iterator_next(iterator));
	
	FLAC__metadata_iterator_delete(iterator), iterator = NULL;
	FLAC__metadata_chain_delete(chain), chain = NULL;
	
#if DEBUG
	CFShow(mMetadata);
#endif
	
	return true;
}

bool FLACMetadata::WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(FALSE == success)
		return false;
	
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
	
	// ENOMEM sux
	if(NULL == chain)
		return false;
	
	if(false == FLAC__metadata_chain_read(chain, reinterpret_cast<const char *>(buf))) {
		
		// Attempt to provide a descriptive error message
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			switch(FLAC__metadata_chain_status(chain)) {
				case FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE:
				{
					CFStringRef displayName = CreateDisplayNameForURL(mURL);
					CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																	   NULL, 
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
																	   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
	
	// Seek to the vorbis comment block if it exists
	while(FLAC__METADATA_TYPE_VORBIS_COMMENT != FLAC__metadata_iterator_get_block_type(iterator)) {
		if(false == FLAC__metadata_iterator_next(iterator))
			break; // Already at end
	}
	
	FLAC__StreamMetadata *block = NULL;
	
	// If there isn't a vorbis comment block add one
	if(FLAC__METADATA_TYPE_VORBIS_COMMENT != FLAC__metadata_iterator_get_block_type(iterator)) {
		
		// The padding block will be the last block if it exists; add the comment block before it
		if(FLAC__METADATA_TYPE_PADDING == FLAC__metadata_iterator_get_block_type(iterator))
			FLAC__metadata_iterator_prev(iterator);
		
		block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
		
		if(NULL == block) {
			FLAC__metadata_chain_delete(chain), chain = NULL;
			FLAC__metadata_iterator_delete(iterator), iterator = NULL;

			return false;
		}
		
		// Add our metadata
		if(false == FLAC__metadata_iterator_insert_block_after(iterator, block)) {
			if(NULL != error) {
				CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																				   32,
																				   &kCFTypeDictionaryKeyCallBacks,
																				   &kCFTypeDictionaryValueCallBacks);

				CFStringRef displayName = CreateDisplayNameForURL(mURL);
				CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
																   NULL, 
																   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
	
	// Album title
	setVorbisComment(block, CFSTR("ALBUM"), GetAlbumTitle());
	
	// Artist
	setVorbisComment(block, CFSTR("ARTIST"), GetArtist());
	
	// Album Artist
	setVorbisComment(block, CFSTR("ALBUMARTIST"), GetAlbumArtist());
	
	// Composer
	setVorbisComment(block, CFSTR("COMPOSER"), GetComposer());
	
	// Genre
	setVorbisComment(block, CFSTR("GENRE"), GetGenre());
	
	// Date
	setVorbisComment(block, CFSTR("DATE"), GetReleaseDate());
	
	// Comment
	setVorbisComment(block, CFSTR("DESCRIPTION"), GetComment());
	
	// Track title
	setVorbisComment(block, CFSTR("TITLE"), GetTitle());
	
	// Track number
	setVorbisCommentNumber(block, CFSTR("TRACKNUMBER"), GetTrackNumber());
	
	// Total tracks
	setVorbisCommentNumber(block, CFSTR("TRACKTOTAL"), GetTrackTotal());
	
	// Compilation
	setVorbisCommentBoolean(block, CFSTR("COMPILATION"), GetCompilation());
	
	// Disc number
	setVorbisCommentNumber(block, CFSTR("DISCNUMBER"), GetDiscNumber());
	
	// Disc total
	setVorbisCommentNumber(block, CFSTR("DISCTOTAL"), GetDiscTotal());
	
	// ISRC
	setVorbisComment(block, CFSTR("ISRC"), GetISRC());
	
	// MCN
	setVorbisComment(block, CFSTR("MCN"), GetMCN());
	
	// ReplayGain info
	setVorbisCommentNumber(block, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));

	
	setVorbisCommentNumber(block, CFSTR("REPLAYGAIN_TRACK_GAIN"), GetReplayGainReferenceLoudness(), CFSTR("%+2.2f dB"));
	setVorbisCommentNumber(block, CFSTR("REPLAYGAIN_TRACK_PEAK"), GetReplayGainTrackGain(), CFSTR("%1.8f"));
	setVorbisCommentNumber(block, CFSTR("REPLAYGAIN_ALBUM_GAIN"), GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	setVorbisCommentNumber(block, CFSTR("REPLAYGAIN_ALBUM_PEAK"), GetReplayGainAlbumPeak(), CFSTR("%1.8f"));
	
	// Write the new metadata to the file
	if(false == FLAC__metadata_chain_write(chain, true, false)) {
		if(NULL != error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CreateDisplayNameForURL(mURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file \"%@\" is not a valid FLAC file."), ""), 
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
	
	return true;
}
