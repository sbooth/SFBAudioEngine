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

#include <AudioToolbox/AudioFormat.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdexcept>

#include "HTTPInputSource.h"
#include "AudioDecoder.h"
#include "Logger.h"
#include "CreateChannelLayout.h"
#include "CreateDisplayNameForURL.h"
#include "CreateStringForOSType.h"
#include "LoopableRegionDecoder.h"

#include "CoreAudioDecoder.h"
#include "FLACDecoder.h"
#include "WavPackDecoder.h"
#include "MPEGDecoder.h"
#include "OggVorbisDecoder.h"
#if !TARGET_OS_IPHONE
# include "MusepackDecoder.h"
# include "MonkeysAudioDecoder.h"
#endif
#include "OggSpeexDecoder.h"
#if !TARGET_OS_IPHONE
# include "MODDecoder.h"
#endif
#include "LibsndfileDecoder.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef	AudioDecoderErrorDomain					= CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioDecoder");

#pragma mark Static Methods

bool AudioDecoder::sAutomaticallyOpenDecoders = false;

CFArrayRef AudioDecoder::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderExtensions = NULL;

	decoderExtensions = FLACDecoder::CreateSupportedFileExtensions();
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

#if !TARGET_OS_IPHONE
	decoderExtensions = MusepackDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

	decoderExtensions = MonkeysAudioDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
#endif

	decoderExtensions = OggSpeexDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;

#if !TARGET_OS_IPHONE
	decoderExtensions = MODDecoder::CreateSupportedFileExtensions();
	CFArrayAppendArray(supportedExtensions, decoderExtensions, CFRangeMake(0, CFArrayGetCount(decoderExtensions)));
	CFRelease(decoderExtensions), decoderExtensions = NULL;
#endif

	decoderExtensions = LibsndfileDecoder::CreateSupportedFileExtensions();
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
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	CFArrayRef decoderMIMETypes = NULL;

	decoderMIMETypes = FLACDecoder::CreateSupportedMIMETypes();
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
	
#if !TARGET_OS_IPHONE
	decoderMIMETypes = MusepackDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
	
	decoderMIMETypes = MonkeysAudioDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
#endif

	decoderMIMETypes = OggSpeexDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;

#if !TARGET_OS_IPHONE
	decoderMIMETypes = MODDecoder::CreateSupportedMIMETypes();
	CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	CFRelease(decoderMIMETypes), decoderMIMETypes = NULL;
#endif

	decoderMIMETypes = LibsndfileDecoder::CreateSupportedMIMETypes();
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
	if(NULL == extension)
		return false;
	
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
	if(NULL == mimeType)
		return false;

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

AudioDecoder * AudioDecoder::CreateDecoderForURL(CFURLRef url, CFErrorRef *error)
{
	return CreateDecoderForURL(url, NULL, error);
}

AudioDecoder * AudioDecoder::CreateDecoderForURL(CFURLRef url, CFStringRef mimeType, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;

	// Create the input source which will feed the decoder
	InputSource *inputSource = InputSource::CreateInputSourceForURL(url, 0, error);
	
	if(NULL == inputSource)
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSource(inputSource, mimeType, error);
	
	if(NULL == decoder)
		delete inputSource, inputSource = NULL;
	
	return decoder;
}

// If this returns NULL, the caller is responsible for deleting inputSource
// If this returns an AudioDecoder instance, the instance takes ownership of inputSource
AudioDecoder * AudioDecoder::CreateDecoderForInputSource(InputSource *inputSource, CFErrorRef *error)
{
	return CreateDecoderForInputSource(inputSource, NULL, error);
}

AudioDecoder * AudioDecoder::CreateDecoderForInputSource(InputSource *inputSource, CFStringRef mimeType, CFErrorRef *error)
{
	if(NULL == inputSource)
		return NULL;

	AudioDecoder *decoder = NULL;

	// Open the input source if it isn't already
	if(AutomaticallyOpenDecoders() && !inputSource->IsOpen() && !inputSource->Open(error))
		return NULL;

	// As a factory this class has knowledge of its subclasses
	// It would be possible (and perhaps preferable) to switch to a generic
	// plugin interface at a later date

#if 0
	// If the input is an instance of HTTPInputSource, use the MIME type from the server
	// This code is disabled because most HTTP servers don't send the correct MIME types
	HTTPInputSource *httpInputSource = dynamic_cast<HTTPInputSource *>(inputSource);
	bool releaseMIMEType = false;
	if(!mimeType && httpInputSource && httpInputSource->IsOpen()) {
		mimeType = httpInputSource->CopyContentMIMEType();
		if(mimeType)
			releaseMIMEType = true;
	}
#endif

	// The MIME type takes precedence over the file extension
	if(mimeType) {
		if(FLACDecoder::HandlesMIMEType(mimeType)) {
			decoder = new FLACDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
		if(NULL == decoder && WavPackDecoder::HandlesMIMEType(mimeType)) {
			decoder = new WavPackDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
		if(NULL == decoder && MPEGDecoder::HandlesMIMEType(mimeType)) {
			decoder = new MPEGDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
		if(NULL == decoder && OggVorbisDecoder::HandlesMIMEType(mimeType)) {
			decoder = new OggVorbisDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
#if !TARGET_OS_IPHONE
		if(NULL == decoder && MusepackDecoder::HandlesMIMEType(mimeType)) {
			decoder = new MusepackDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
		if(NULL == decoder && MonkeysAudioDecoder::HandlesMIMEType(mimeType)) {
			decoder = new MonkeysAudioDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
#endif
		if(NULL == decoder && OggSpeexDecoder::HandlesMIMEType(mimeType)) {
			decoder = new OggSpeexDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
#if !TARGET_OS_IPHONE
		if(NULL == decoder && MODDecoder::HandlesMIMEType(mimeType)) {
			decoder = new MODDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
#endif
		if(NULL == decoder && LibsndfileDecoder::HandlesMIMEType(mimeType)) {
			decoder = new LibsndfileDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}
		if(NULL == decoder && CoreAudioDecoder::HandlesMIMEType(mimeType)) {
			decoder = new CoreAudioDecoder(inputSource);
			if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
				decoder->mInputSource = NULL;
				delete decoder, decoder = NULL;
			}
		}

#if 0
		if(releaseMIMEType)
			CFRelease(mimeType), mimeType = NULL;
#endif

		if(decoder)
			return decoder;
	}

	// If no MIME type was specified, use the extension-based resolvers

	CFURLRef inputURL = inputSource->GetURL();
	if(!inputURL)
		return NULL;

	// Determining the extension isn't as simple as using CFURLCopyPathExtension (wouldn't that be nice?),
	// because although the behavior on Lion works like one would expect, on Snow Leopard it returns
	// a number that I believe is part of the inode number, but is definitely NOT the extension
	CFStringRef pathExtension = NULL;
#if !TARGET_OS_IPHONE
	CFURLRef filePathURL = CFURLCreateFilePathURL(kCFAllocatorDefault, inputURL, NULL);
	if(filePathURL) {
		pathExtension = CFURLCopyPathExtension(filePathURL);
		CFRelease(filePathURL), filePathURL = NULL;
	}
	else
#endif
		pathExtension = CFURLCopyPathExtension(inputURL);

	if(!pathExtension) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   0,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);

			CFStringRef displayName = CFURLCopyLastPathComponent(inputURL);
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The type of the file “%@” could not be determined."), ""), 
															   displayName);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Unknown file type"), ""));

			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may be missing or may not match the file's type."), ""));

			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;

			*error = CFErrorCreate(kCFAllocatorDefault, 
								   InputSourceErrorDomain, 
								   InputSourceFileNotFoundError,
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}

		return NULL;
	}

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
	// and if openDecoder is false the wrong decoder type may be returned, since the file isn't analyzed
	// until Open() is called
	
	if(FLACDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new FLACDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
	if(NULL == decoder && WavPackDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new WavPackDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
	if(NULL == decoder && MPEGDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new MPEGDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
	if(NULL == decoder && OggVorbisDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new OggVorbisDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
#if !TARGET_OS_IPHONE
	if(NULL == decoder && MusepackDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new MusepackDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
	if(NULL == decoder && MonkeysAudioDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new MonkeysAudioDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
#endif
	if(NULL == decoder && OggSpeexDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new OggSpeexDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
#if !TARGET_OS_IPHONE
	if(NULL == decoder && MODDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new MODDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
#endif
	if(NULL == decoder && LibsndfileDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new LibsndfileDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}
	if(NULL == decoder && CoreAudioDecoder::HandlesFilesWithExtension(pathExtension)) {
		decoder = new CoreAudioDecoder(inputSource);
		if(AutomaticallyOpenDecoders() && !decoder->Open(error)) {
			decoder->mInputSource = NULL;
			delete decoder, decoder = NULL;
		}
	}

	CFRelease(pathExtension), pathExtension = NULL;

	return decoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;

	InputSource *inputSource = InputSource::CreateInputSourceForURL(url, 0, error);

	if(NULL == inputSource)
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSourceRegion(inputSource, startingFrame, error);

	if(NULL == decoder)
		delete inputSource, inputSource = NULL;

	return decoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;

	InputSource *inputSource = InputSource::CreateInputSourceForURL(url, 0, error);

	if(NULL == inputSource)
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSourceRegion(inputSource, startingFrame, frameCount, error);

	if(NULL == decoder)
		delete inputSource, inputSource = NULL;

	return decoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;

	InputSource *inputSource = InputSource::CreateInputSourceForURL(url, 0, error);

	if(NULL == inputSource)
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSourceRegion(inputSource, startingFrame, frameCount, repeatCount, error);

	if(NULL == decoder)
		delete inputSource, inputSource = NULL;

	return decoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, CFErrorRef *error)
{
	if(NULL == inputSource)
		return NULL;

	if(!inputSource->SupportsSeeking())
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSource(inputSource, error);

	if(NULL == decoder)
		return NULL;

	if(!decoder->SupportsSeeking()) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	AudioDecoder *regionDecoder = CreateDecoderForDecoderRegion(decoder, startingFrame, error);

	if(NULL == regionDecoder) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	return regionDecoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error)
{
	if(NULL == inputSource)
		return NULL;

	if(!inputSource->SupportsSeeking())
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSource(inputSource, error);

	if(NULL == decoder)
		return NULL;

	if(!decoder->SupportsSeeking()) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	AudioDecoder *regionDecoder = CreateDecoderForDecoderRegion(decoder, startingFrame, frameCount, error);

	if(NULL == regionDecoder) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	return regionDecoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error)
{
	if(NULL == inputSource)
		return NULL;

	if(!inputSource->SupportsSeeking())
		return NULL;

	AudioDecoder *decoder = CreateDecoderForInputSource(inputSource, error);

	if(NULL == decoder)
		return NULL;

	if(!decoder->SupportsSeeking()) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	AudioDecoder *regionDecoder = CreateDecoderForDecoderRegion(decoder, startingFrame, frameCount, repeatCount, error);

	if(NULL == regionDecoder) {
		delete decoder, decoder = NULL;
		return NULL;
	}

	return regionDecoder;
}

AudioDecoder * AudioDecoder::CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, CFErrorRef */*error*/)
{
	if(NULL == decoder)
		return NULL;
	
	if(!decoder->SupportsSeeking())
		return NULL;
	
	return new LoopableRegionDecoder(decoder, startingFrame);
}

AudioDecoder * AudioDecoder::CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, CFErrorRef */*error*/)
{
	if(NULL == decoder)
		return NULL;
	
	if(!decoder->SupportsSeeking())
		return NULL;
	
	return new LoopableRegionDecoder(decoder, startingFrame, frameCount);
}

AudioDecoder * AudioDecoder::CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *)
{
	if(NULL == decoder)
		return NULL;
	
	if(!decoder->SupportsSeeking())
		return NULL;
	
	return new LoopableRegionDecoder(decoder, startingFrame, frameCount, repeatCount);
}

#pragma mark Creation and Destruction

AudioDecoder::AudioDecoder()
	: mInputSource(NULL), mChannelLayout(NULL), mIsOpen(false)
{
	memset(&mCallbacks, 0, sizeof(mCallbacks));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
}

AudioDecoder::AudioDecoder(InputSource *inputSource)
	: mInputSource(inputSource), mChannelLayout(NULL), mIsOpen(false)
{
	assert(NULL != inputSource);

	memset(&mCallbacks, 0, sizeof(mCallbacks));
	memset(&mFormat, 0, sizeof(mSourceFormat));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
}

AudioDecoder::AudioDecoder(const AudioDecoder& rhs)
	: mInputSource(NULL), mChannelLayout(NULL), mIsOpen(false)
{
	*this = rhs;
}

AudioDecoder::~AudioDecoder()
{
	if(mInputSource)
		delete mInputSource, mInputSource = NULL;

	if(mChannelLayout)
		free(mChannelLayout),mChannelLayout = NULL;
}

#pragma mark Operator Overloads

AudioDecoder& AudioDecoder::operator=(const AudioDecoder& rhs)
{
	if(this == &rhs)
		return *this;

	if(mInputSource)
		delete mInputSource, mInputSource = NULL;
	
	if(mChannelLayout)
		free(mChannelLayout), mChannelLayout = NULL;

	if(rhs.mInputSource)
		mInputSource = rhs.mInputSource;

	mFormat				= rhs.mFormat;
	mSourceFormat		= rhs.mSourceFormat;
	mChannelLayout		= CopyChannelLayout(rhs.mChannelLayout);
	mIsOpen				= rhs.mIsOpen;

	memcpy(&mCallbacks, &rhs.mCallbacks, sizeof(rhs.mCallbacks));

	return *this;
}

#pragma mark Base Functionality

CFStringRef AudioDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return NULL;

	CFStringRef		sourceFormatDescription		= NULL;
	UInt32			sourceFormatNameSize		= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
																		 sizeof(mSourceFormat), 
																		 &mSourceFormat, 
																		 &sourceFormatNameSize, 
																		 &sourceFormatDescription);

	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder", "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: " << result << osType);
		CFRelease(osType), osType = NULL;
	}
	
	return sourceFormatDescription;
}

CFStringRef AudioDecoder::CreateFormatDescription() const
{
	if(!IsOpen())
		return NULL;

	CFStringRef		sourceFormatDescription		= NULL;
	UInt32			specifierSize				= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName, 
																		 sizeof(mFormat), 
																		 &mFormat, 
																		 &specifierSize, 
																		 &sourceFormatDescription);

	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder", "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: " << result << osType);
		CFRelease(osType), osType = NULL;
	}
	
	return sourceFormatDescription;
}

CFStringRef AudioDecoder::CreateChannelLayoutDescription() const
{
	if(!IsOpen())
		return NULL;

	CFStringRef		channelLayoutDescription	= NULL;
	UInt32			specifierSize				= sizeof(channelLayoutDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, 
																		 sizeof(mChannelLayout), 
																		 mChannelLayout, 
																		 &specifierSize, 
																		 &channelLayoutDescription);

	if(noErr != result) {
		CFStringRef osType = CreateStringForOSType(result);
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder", "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: " << result << osType);
		CFRelease(osType), osType = NULL;
	}
	
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
