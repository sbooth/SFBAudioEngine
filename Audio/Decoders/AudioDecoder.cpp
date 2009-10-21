/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Stephen F. Booth nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY STEPHEN F. BOOTH ''AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL STEPHEN F. BOOTH BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AudioToolbox/AudioFormat.h>
#include <CoreServices/CoreServices.h>

#include "AudioEngineDefines.h"
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
CFStringRef const AudioDecoderErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioDecoder");


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

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i (%.4s)", result, reinterpret_cast<const char *> (&result));
	
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

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i (%.4s)", result, reinterpret_cast<const char *> (&result));
	
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

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: %i (%.4s)", result, reinterpret_cast<const char *> (&result));
	
	return mChannelLayoutDescription;
}
