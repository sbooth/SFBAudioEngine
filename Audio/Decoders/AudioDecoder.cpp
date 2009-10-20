/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <AudioToolbox/AudioFormat.h>
#include <CoreServices/CoreServices.h>

#include "AudioDecoder.h"
#include "CoreAudioDecoder.h"
//#import "FLACDecoder.h"
//#import "OggFLACDecoder.h"
//#import "OggVorbisDecoder.h"
//#import "MusepackDecoder.h"
//#import "CoreAudioDecoder.h"
//#import "WavPackDecoder.h"
//#import "MonkeysAudioDecoder.h"
//#import "MPEGDecoder.h"
//
//#import "AudioStream.h"
//#import "UtilityFunctions.h"


// ========================================
// Constants
// ========================================
CFStringRef const AudioDecoderErrorDomain = CFSTR("org.sbooth.Play.ErrorDomain.AudioDecoder");


// ========================================
// Utility Functions
// ========================================
#if DEBUG
static inline void
CFLog(CFStringRef format, ...)
{
	va_list args;
	va_start(args, format);
	
	CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault,
															   NULL,
															   format,
															   args);

	CFShow(message);
	CFRelease(message);
}
#endif


#pragma mark Static Methods


AudioDecoder * AudioDecoder::CreateDecoderForURL(CFURLRef url, CFErrorRef *error)
{
	assert(NULL != url);
	
	AudioDecoder *decoder = NULL;
	
	// Determine if the file exists
	SInt32 propertyCreated = 0;
	CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault,
																						url,
																						kCFURLFileExists,
																						&propertyCreated));

	if(noErr != propertyCreated) {
		return NULL;
	}
	
	Boolean exists = CFBooleanGetValue(fileExists);
	CFRelease(fileExists), fileExists = NULL;
	
	if(!exists) {
		if(NULL != error) {
			CFStringRef displayName = NULL;
			OSStatus result = LSCopyDisplayNameForURL(url, &displayName);

			if(noErr != result)
				displayName = CFURLCopyLastPathComponent(url);
			
			CFStringRef errorDescription = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("The file \"%@\" was not found."), displayName);

			CFTypeRef userInfoKeys [] = {
				kCFErrorLocalizedDescriptionKey,
				kCFErrorLocalizedFailureReasonKey,
				kCFErrorLocalizedRecoverySuggestionKey
			};
			
			CFTypeRef userInfoValues [] = {
				errorDescription,
				CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""),
				CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "")
			};
			
			*error = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault,
															AudioDecoderErrorDomain,
															AudioDecoderFileNotFoundError, 
															(const void * const *)&userInfoKeys, 
															(const void * const *)&userInfoValues,
															3);
			
			CFRelease(displayName);
			CFRelease(errorDescription);
		}
		
		return NULL;
	}
	
	CFStringRef pathExtension = CFURLCopyPathExtension(url);
	
	if(CoreAudioDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new CoreAudioDecoder(url, error);
	}
	
	CFRelease(pathExtension);
	
	return decoder;
	
#if 0
	AudioDecoder		*result				= nil;
	NSString			*path				= [url path];
	NSString			*pathExtension		= [[path pathExtension] lowercaseString];

/*	FSRef				ref;
	NSString			*uti				= nil;	
	
	FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &ref, NULL);
	
	OSStatus lsResult = LSCopyItemAttribute(&ref, kLSRolesAll, kLSItemContentType, (CFTypeRef *)&uti);

	NSLog(@"UTI for %@:%@", url, uti);
	[uti release];*/
	
	// Ensure the file exists
	if(NO == [[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
		if(nil != error) {
			NSMutableDictionary *errorDictionary = [NSMutableDictionary dictionary];
			
			[errorDictionary setObject:[NSString stringWithFormat:NSLocalizedStringFromTable(@"The file \"%@\" could not be found.", @"Errors", @""), [[NSFileManager defaultManager] displayNameAtPath:path]] forKey:NSLocalizedDescriptionKey];
			[errorDictionary setObject:NSLocalizedStringFromTable(@"File Not Found", @"Errors", @"") forKey:NSLocalizedFailureReasonErrorKey];
			[errorDictionary setObject:NSLocalizedStringFromTable(@"The file may have been renamed or deleted, or exist on removable media.", @"Errors", @"") forKey:NSLocalizedRecoverySuggestionErrorKey];
			
			*error = [NSError errorWithDomain:AudioDecoderErrorDomain 
										 code:AudioDecoderFileNotFoundError 
									 userInfo:errorDictionary];
		}
		return nil;	
	}
	
	if([pathExtension isEqualToString:@"flac"])
		result = [[FLACDecoder alloc] initWithURL:url error:error];
	else if([pathExtension isEqualToString:@"ogg"] || [pathExtension isEqualToString:@"oga"]) {
		OggStreamType type = oggStreamType(url);
		
		if(kOggStreamTypeInvalid == type || kOggStreamTypeUnknown == type || kOggStreamTypeSpeex == type) {
			
			if(nil != error) {
				NSMutableDictionary *errorDictionary = [NSMutableDictionary dictionary];
				
				switch(type) {
					case kOggStreamTypeInvalid:
						[errorDictionary setObject:[NSString stringWithFormat:NSLocalizedStringFromTable(@"The file \"%@\" is not a valid Ogg file.", @"Errors", @""), [[NSFileManager defaultManager] displayNameAtPath:path]] forKey:NSLocalizedDescriptionKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"Not an Ogg file", @"Errors", @"") forKey:NSLocalizedFailureReasonErrorKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"The file's extension may not match the file's type.", @"Errors", @"") forKey:NSLocalizedRecoverySuggestionErrorKey];						
						break;
						
					case kOggStreamTypeUnknown:
						[errorDictionary setObject:[NSString stringWithFormat:NSLocalizedStringFromTable(@"The type of Ogg data in the file \"%@\" could not be determined.", @"Errors", @""), [[NSFileManager defaultManager] displayNameAtPath:path]] forKey:NSLocalizedDescriptionKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"Unknown Ogg file type", @"Errors", @"") forKey:NSLocalizedFailureReasonErrorKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"This data format is not supported for the Ogg container.", @"Errors", @"") forKey:NSLocalizedRecoverySuggestionErrorKey];						
						break;
						
					default:
						[errorDictionary setObject:[NSString stringWithFormat:NSLocalizedStringFromTable(@"The file \"%@\" is not a valid Ogg file.", @"Errors", @""), [[NSFileManager defaultManager] displayNameAtPath:path]] forKey:NSLocalizedDescriptionKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"Not an Ogg file", @"Errors", @"") forKey:NSLocalizedFailureReasonErrorKey];
						[errorDictionary setObject:NSLocalizedStringFromTable(@"The file's extension may not match the file's type.", @"Errors", @"") forKey:NSLocalizedRecoverySuggestionErrorKey];						
						break;
				}
				
				*error = [NSError errorWithDomain:AudioDecoderErrorDomain 
											 code:AudioDecoderFileFormatNotRecognizedError 
										 userInfo:errorDictionary];
			}
			
			return nil;
		}
		
		switch(type) {
			case kOggStreamTypeVorbis:		result = [[OggVorbisDecoder alloc] initWithURL:url error:error];				break;
			case kOggStreamTypeFLAC:		result = [[OggFLACDecoder alloc] initWithURL:url error:error];				break;
//			case kOggStreamTypeSpeex:		result = [[AudioDecoder alloc] initWithURL:url error:error];					break;
			default:						result = nil;												break;
		}
	}
	else if([pathExtension isEqualToString:@"mpc"])
		result = [[MusepackDecoder alloc] initWithURL:url error:error];
	else if([pathExtension isEqualToString:@"wv"])
		result = [[WavPackDecoder alloc] initWithURL:url error:error];
	else if([pathExtension isEqualToString:@"ape"])
		result = [[MonkeysAudioDecoder alloc] initWithURL:url error:error];
	else if([pathExtension isEqualToString:@"mp3"])
		result = [[MPEGDecoder alloc] initWithURL:url error:error];
	else if([getCoreAudioExtensions() containsObject:pathExtension])
		result = [[CoreAudioDecoder alloc] initWithURL:url error:error];
	else {
		if(nil != error) {
			NSMutableDictionary *errorDictionary = [NSMutableDictionary dictionary];
			
			[errorDictionary setObject:[NSString stringWithFormat:NSLocalizedStringFromTable(@"The format of the file \"%@\" was not recognized.", @"Errors", @""), [[NSFileManager defaultManager] displayNameAtPath:path]] forKey:NSLocalizedDescriptionKey];
			[errorDictionary setObject:NSLocalizedStringFromTable(@"File Format Not Recognized", @"Errors", @"") forKey:NSLocalizedFailureReasonErrorKey];
			[errorDictionary setObject:NSLocalizedStringFromTable(@"The file's extension may not match the file's type.", @"Errors", @"") forKey:NSLocalizedRecoverySuggestionErrorKey];
			
			*error = [NSError errorWithDomain:AudioDecoderErrorDomain 
										 code:AudioDecoderFileFormatNotRecognizedError 
									 userInfo:errorDictionary];
		}
		return nil;
	}
	
	return [result autorelease];
#endif

	return NULL;
}

AudioDecoder * AudioDecoder::CreateDecoderForMIMEType(CFStringRef mimeType, CFErrorRef */*error*/)
{
	assert(NULL != mimeType);

	return NULL;
}


#pragma mark Creation and Destruction


AudioDecoder::AudioDecoder()
	: mURL(NULL), mFormat(), mFormatDescription(NULL), mChannelLayoutDescription(NULL), mSourceFormatDescription(NULL)
{	
}

AudioDecoder::AudioDecoder(const AudioDecoder& rhs)
	: mURL(NULL), mFormatDescription(NULL), mChannelLayoutDescription(NULL), mSourceFormatDescription(NULL)
{
	*this = rhs;
}

AudioDecoder::AudioDecoder(CFURLRef url, CFErrorRef */*error*/)
	: mURL(NULL), mFormatDescription(NULL), mChannelLayoutDescription(NULL), mSourceFormatDescription(NULL)
{
	assert(NULL != url);
	
	mURL = static_cast<CFURLRef>(CFRetain(url));
	
	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;		
}

AudioDecoder::~AudioDecoder()
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;

	if(mFormatDescription)
		CFRelease(mFormatDescription), mFormatDescription = NULL;
	
	if(mChannelLayoutDescription)
		CFRelease(mChannelLayoutDescription), mChannelLayoutDescription = NULL;

	if(mSourceFormatDescription)
		CFRelease(mSourceFormatDescription), mSourceFormatDescription = NULL;
}


#pragma mark Operator Overloads


AudioDecoder& AudioDecoder::operator=(const AudioDecoder& rhs)
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
	
	if(mFormatDescription)
		CFRelease(mFormatDescription), mFormatDescription = NULL;
	
	if(mChannelLayoutDescription)
		CFRelease(mChannelLayoutDescription), mChannelLayoutDescription = NULL;
	
	if(mSourceFormatDescription)
		CFRelease(mSourceFormatDescription), mSourceFormatDescription = NULL;
	
	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));
	
	mFormat				= rhs.mFormat;
	mChannelLayout		= rhs.mChannelLayout;
	mSourceFormat		= rhs.mSourceFormat;
	
	return *this;
}


#pragma mark Base Functionality


CFStringRef AudioDecoder::GetSourceFormatDescription()
{
	if(mSourceFormatDescription)
		return mSourceFormatDescription;
	
	AudioStreamBasicDescription		sourceFormat			= mSourceFormat;	
	UInt32							sourceFormatNameSize	= sizeof(mSourceFormatDescription);
	OSStatus						result					= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
																					 sizeof(sourceFormat), 
																					 &sourceFormat, 
																					 &sourceFormatNameSize, 
																					 &mSourceFormatDescription);

	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i"), result);
#endif
	}
	
	return mSourceFormatDescription;
}

CFStringRef AudioDecoder::GetFormatDescription()
{
	if(mFormatDescription)
		return mFormatDescription;

	UInt32		specifierSize	= sizeof(mFormatDescription);
	OSStatus	result			= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
														 sizeof(mFormat), 
														 &mFormat, 
														 &specifierSize, 
														 &mFormatDescription);

	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i"), result);
#endif
	}
	
	return mFormatDescription;
}

CFStringRef AudioDecoder::GetChannelLayoutDescription()
{
	if(mChannelLayoutDescription)
		return mChannelLayoutDescription;
	
	UInt32		specifierSize	= sizeof(mChannelLayoutDescription);
	OSStatus	result			= AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, 
														 sizeof(mChannelLayout), 
														 &mChannelLayout, 
														 &specifierSize, 
														 &mChannelLayoutDescription);

	if(noErr != result) {
#if DEBUG
		CFLog(CFSTR("AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: %i"), result);
#endif
	}
	
	return mChannelLayoutDescription;
}
