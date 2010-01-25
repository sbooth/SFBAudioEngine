/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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

#include <AudioToolbox/AudioFormat.h>
#include <CoreServices/CoreServices.h>
#include <stdexcept>

#include "AudioEngineDefines.h"
#include "AudioDecoder.h"
#include "LoopableRegionDecoder.h"
#include "CoreAudioDecoder.h"
#include "FLACDecoder.h"
#include "WavPackDecoder.h"
#include "MPEGDecoder.h"
#include "OggVorbisDecoder.h"
#include "MusepackDecoder.h"


#pragma mark Static Methods


CFArrayRef AudioDecoder::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 32, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderExtensions = FLACDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = WavPackDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MPEGDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = OggVorbisDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MusepackDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = CoreAudioDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
	
	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedExtensions);
	
	CFRelease(supportedExtensions), supportedExtensions = NULL;
	
	return result;
}

CFArrayRef AudioDecoder::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 32, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderMIMETypes = FLACDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = WavPackDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = MPEGDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = OggVorbisDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = MusepackDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = CoreAudioDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, supportedMIMETypes);
	
	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return result;
}

bool AudioDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	CFArrayRef supportedExtensions = CreateSupportedFileExtensions();
	if(NULL == supportedExtensions)
		return false;
	
	bool extensionIsSupported = false;
	
	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedExtensions, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive)) {
			extensionIsSupported = true;
			break;
		}
	}
	
	CFRelease(supportedExtensions), supportedExtensions = NULL;

	return extensionIsSupported;
}

bool AudioDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);

	CFArrayRef supportedMIMETypes = CreateSupportedMIMETypes();
	if(NULL == supportedMIMETypes)
		return false;
	
	bool mimeTypeIsSupported = false;
	
	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		CFStringRef currentMIMEType = static_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex));
		if(kCFCompareEqualTo == CFStringCompare(mimeType, currentMIMEType, kCFCompareCaseInsensitive)) {
			mimeTypeIsSupported = true;
			break;
		}
	}

	CFRelease(supportedMIMETypes), supportedMIMETypes = NULL;
	
	return mimeTypeIsSupported;
}

AudioDecoder * AudioDecoder::CreateDecoderForURL(CFURLRef url)
{
	assert(NULL != url);
	
	AudioDecoder *decoder = NULL;
	
	// If this is a file URL, use the extension-based resolvers
	CFStringRef scheme = CFURLCopyScheme(url);
	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		// Verify the file exists
		SInt32 errorCode = noErr;
		CFBooleanRef fileExists = static_cast<CFBooleanRef>(CFURLCreatePropertyFromResource(kCFAllocatorDefault, url, kCFURLFileExists, &errorCode));
		
		if(NULL != fileExists) {
			if(CFBooleanGetValue(fileExists)) {
				CFStringRef pathExtension = CFURLCopyPathExtension(url);
				
				if(NULL != pathExtension) {

					// Creating a decoder may throw an exception for any number of reasons
					
					// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
					// In lieu of adding a FileIsValid() method to each class that would open
					// and evaluate each file before opening, the try/catch madness here has a
					// similar effect without the file opening overhead
					
					// Additionally, as a factory this class has knowledge of its subclasses
					// It would be possible (and perhaps preferable) to switch to a generic
					// plugin interface at a later date
					try {
						if(FLACDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new FLACDecoder(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}
					
					try {
						if(NULL == decoder && WavPackDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new WavPackDecoder(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}

					try {
						if(NULL == decoder && MPEGDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new MPEGDecoder(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}
					try {
						if(NULL == decoder && OggVorbisDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new OggVorbisDecoder(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}

					try {
						if(NULL == decoder && MusepackDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new MusepackDecoder(url);
					}

					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}

					try {
						if(NULL == decoder && CoreAudioDecoder::HandlesFilesWithExtension(pathExtension))
							decoder = new CoreAudioDecoder(url);
					}
					
					catch(std::exception& e) {
						LOG("Exception creating decoder: %s", e.what());
					}
					
					CFRelease(pathExtension), pathExtension = NULL;
				}				
			}
			else
				LOG("The requested URL doesn't exist");
		}
		else
			ERR("CFURLCreatePropertyFromResource failed: %i", errorCode);		

		CFRelease(fileExists), fileExists = NULL;
	}
	// Determine the MIME type for the URL
	else {
		// Get the UTI for this URL
		FSRef ref;
		Boolean success = CFURLGetFSRef(url, &ref);
		if(FALSE == success) {
			ERR("Unable to get FSRef for URL");
			
			return NULL;
		}
		
		CFStringRef uti = NULL;
		OSStatus result = LSCopyItemAttribute(&ref, kLSRolesAll, kLSItemContentType, (CFTypeRef *)&uti);
		
		if(noErr != result) {
			ERR("LSCopyItemAttribute (kLSItemContentType) failed: %i", result);
			
			return NULL;
		}
		
		CFRelease(uti), uti = NULL;
	}
	
	CFRelease(scheme), scheme = NULL;

	return decoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame)
{
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(url);
	
	if(NULL == decoder)
		return NULL;
	
	if(false == decoder->SupportsSeeking()) {
		delete decoder;
		return NULL;
	}

	return new LoopableRegionDecoder(decoder, startingFrame);
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount)
{
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(url);
	
	if(NULL == decoder)
		return NULL;
	
	if(false == decoder->SupportsSeeking()) {
		delete decoder;
		return NULL;
	}
	
	return new LoopableRegionDecoder(decoder, startingFrame, frameCount);
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount)
{
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(url);
	
	if(NULL == decoder)
		return NULL;
	
	// In order to repeat a decoder must support seeking
	if(false == decoder->SupportsSeeking()) {
		delete decoder;
		return NULL;
	}
	
	return new LoopableRegionDecoder(decoder, startingFrame, frameCount, repeatCount);
}


#pragma mark Creation and Destruction


AudioDecoder::AudioDecoder()
	: mURL(NULL)
{
	memset(&mCallbacks, 0, sizeof(mCallbacks));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
}

AudioDecoder::AudioDecoder(CFURLRef url)
	: mURL(NULL)
{
	assert(NULL != url);
	
	mURL = static_cast<CFURLRef>(CFRetain(url));

	memset(&mCallbacks, 0, sizeof(mCallbacks));
	memset(&mFormat, 0, sizeof(mSourceFormat));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
	memset(&mChannelLayout, 0, sizeof(mChannelLayout));
}

AudioDecoder::AudioDecoder(const AudioDecoder& rhs)
	: mURL(NULL)
{
	*this = rhs;
}

AudioDecoder::~AudioDecoder()
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
}


#pragma mark Operator Overloads


AudioDecoder& AudioDecoder::operator=(const AudioDecoder& rhs)
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
	
	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));
	
	mFormat				= rhs.mFormat;
	mChannelLayout		= rhs.mChannelLayout;
	mSourceFormat		= rhs.mSourceFormat;
	
	memcpy(&mCallbacks, &rhs.mCallbacks, sizeof(rhs.mCallbacks));
	
	return *this;
}


#pragma mark Base Functionality


CFStringRef AudioDecoder::CreateSourceFormatDescription()
{
	CFStringRef		sourceFormatDescription		= NULL;
	UInt32			sourceFormatNameSize		= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
																		 sizeof(mSourceFormat), 
																		 &mSourceFormat, 
																		 &sourceFormatNameSize, 
																		 &sourceFormatDescription);

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
	
	return sourceFormatDescription;
}

CFStringRef AudioDecoder::CreateFormatDescription()
{
	CFStringRef		sourceFormatDescription		= NULL;
	UInt32			specifierSize				= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
																		 sizeof(mFormat), 
																		 &mFormat, 
																		 &specifierSize, 
																		 &sourceFormatDescription);

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
	
	return sourceFormatDescription;
}

CFStringRef AudioDecoder::CreateChannelLayoutDescription()
{
	CFStringRef		channelLayoutDescription	= NULL;
	UInt32			specifierSize				= sizeof(channelLayoutDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, 
																		 sizeof(mChannelLayout), 
																		 &mChannelLayout, 
																		 &specifierSize, 
																		 &channelLayoutDescription);

	if(noErr != result)
		ERR("AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: %i (%.4s)", result, reinterpret_cast<const char *>(&result));
	
	return channelLayoutDescription;
}


#pragma mark Callbacks


void AudioDecoder::SetDecodingStartedCallback(AudioDecoderCallback callback, void *context)
{
	mCallbacks[0].mCallback = callback;
	mCallbacks[0].mContext = context;
}

void AudioDecoder::SetDecodingFinishedCallback(AudioDecoderCallback callback, void *context)
{
	mCallbacks[1].mCallback = callback;
	mCallbacks[1].mContext = context;
}

void AudioDecoder::SetRenderingStartedCallback(AudioDecoderCallback callback, void *context)
{
	mCallbacks[2].mCallback = callback;
	mCallbacks[2].mContext = context;
}

void AudioDecoder::SetRenderingFinishedCallback(AudioDecoderCallback callback, void *context)
{
	mCallbacks[3].mCallback = callback;
	mCallbacks[3].mContext = context;
}

void AudioDecoder::PerformDecodingStartedCallback()
{
	if(NULL != mCallbacks[0].mCallback)
		mCallbacks[0].mCallback(mCallbacks[0].mContext, this);
}

void AudioDecoder::PerformDecodingFinishedCallback()
{
	if(NULL != mCallbacks[1].mCallback)
		mCallbacks[1].mCallback(mCallbacks[1].mContext, this);
}

void AudioDecoder::PerformRenderingStartedCallback()
{
	if(NULL != mCallbacks[2].mCallback)
		mCallbacks[2].mCallback(mCallbacks[2].mContext, this);
}

void AudioDecoder::PerformRenderingFinishedCallback()
{
	if(NULL != mCallbacks[3].mCallback)
		mCallbacks[3].mCallback(mCallbacks[3].mContext, this);
}
