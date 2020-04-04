/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include <AudioToolbox/AudioFormat.h>
#include <CoreFoundation/CoreFoundation.h>

#include "AudioDecoder.h"
#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "HTTPInputSource.h"
#include "LoopableRegionDecoder.h"
#include "SFBCStringForOSType.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef SFB::Audio::Decoder::ErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioDecoder");

#pragma mark Static Methods

std::atomic_bool SFB::Audio::Decoder::sAutomaticallyOpenDecoders = ATOMIC_VAR_INIT(false);
std::vector<SFB::Audio::Decoder::SubclassInfo> SFB::Audio::Decoder::sRegisteredSubclasses;

CFArrayRef SFB::Audio::Decoder::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedFileExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderFileExtensions(subclassInfo.mCreateSupportedFileExtensions());
		CFArrayAppendArray(supportedFileExtensions, decoderFileExtensions, CFRangeMake(0, CFArrayGetCount(decoderFileExtensions)));
	}

	return supportedFileExtensions;
}

CFArrayRef SFB::Audio::Decoder::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for(auto subclassInfo : sRegisteredSubclasses) {
		SFB::CFArray decoderMIMETypes(subclassInfo.mCreateSupportedMIMETypes());
		CFArrayAppendArray(supportedMIMETypes, decoderMIMETypes, CFRangeMake(0, CFArrayGetCount(decoderMIMETypes)));
	}

	return supportedMIMETypes;
}

bool SFB::Audio::Decoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesFilesWithExtension(extension))
			return true;
	}

	return false;
}

bool SFB::Audio::Decoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesMIMEType(mimeType))
			return true;
	}

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::Decoder::CreateForURL(CFURLRef url, CFErrorRef *error)
{
	return CreateForURL(url, nullptr, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::Decoder::CreateForURL(CFURLRef url, CFStringRef mimeType, CFErrorRef *error)
{
	return CreateForInputSource(InputSource::CreateForURL(url, 0, error), mimeType, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::Decoder::CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error)
{
	return CreateForInputSource(std::move(inputSource), nullptr, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::Decoder::CreateForInputSource(InputSource::unique_ptr inputSource, CFStringRef mimeType, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	// Open the input source if it isn't already
	if(AutomaticallyOpenDecoders() && !inputSource->IsOpen() && !inputSource->Open(error))
		return nullptr;

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
		for(auto subclassInfo : sRegisteredSubclasses) {
			if(subclassInfo.mHandlesMIMEType(mimeType)) {
				unique_ptr decoder(subclassInfo.mCreateDecoder(std::move(inputSource)));
				if(!AutomaticallyOpenDecoders())
					return decoder;
				else {
					 if(decoder->Open(error))
						 return decoder;
					// Take back the input source for reuse if opening fails
					else
						 inputSource = std::move(decoder->mInputSource);
				}
			}
		}

#if 0
		if(releaseMIMEType)
			CFRelease(mimeType), mimeType = nullptr;
#endif
	}

	// If no MIME type was specified, use the extension-based resolvers

	CFURLRef inputURL = inputSource->GetURL();
	if(!inputURL)
		return nullptr;

	SFB::CFString pathExtension(CFURLCopyPathExtension(inputURL));
	if(!pathExtension) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The type of the file “%@” could not be determined."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unknown file type"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may be missing or may not match the file's type."), ""));

			*error = CreateErrorForURL(InputSource::ErrorDomain, InputSource::FileNotFoundError, description, inputURL, failureReason, recoverySuggestion);
		}

		return nullptr;
	}

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
	// and if openDecoder is false the wrong decoder type may be returned, since the file isn't analyzed
	// until Open() is called

	for(auto subclassInfo : sRegisteredSubclasses) {
		if(subclassInfo.mHandlesFilesWithExtension(pathExtension)) {
			unique_ptr decoder(subclassInfo.mCreateDecoder(std::move(inputSource)));
			if(!AutomaticallyOpenDecoders())
				return decoder;
			else {
				if(decoder->Open(error))
					return decoder;
				// Take back the input source for reuse if opening fails
				else
					inputSource = std::move(decoder->mInputSource);
			}
		}
	}

	return nullptr;
}

#pragma mark Creation and Destruction

SFB::Audio::Decoder::Decoder()
	: mInputSource(nullptr), mRepresentedObject(nullptr), mRepresentedObjectCleanupBlock(nullptr), mIsOpen(false)
{
	memset(&mFormat, 0, sizeof(mFormat));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
}

SFB::Audio::Decoder::Decoder(InputSource::unique_ptr inputSource)
	: mInputSource(std::move(inputSource)), mRepresentedObject(nullptr), mRepresentedObjectCleanupBlock(nullptr), mIsOpen(false)
{
	assert(nullptr != mInputSource);

	memset(&mFormat, 0, sizeof(mFormat));
	memset(&mSourceFormat, 0, sizeof(mSourceFormat));
}

SFB::Audio::Decoder::~Decoder()
{
	if(mRepresentedObject && mRepresentedObjectCleanupBlock) {
		mRepresentedObjectCleanupBlock(mRepresentedObject);
		mRepresentedObject = nullptr;
	}
	if(mRepresentedObjectCleanupBlock) {
		Block_release(mRepresentedObjectCleanupBlock);
		mRepresentedObjectCleanupBlock = nullptr;
	}
}

#pragma mark Represented Object Support

void SFB::Audio::Decoder::SetRepresentedObject(void *representedObject)
{
	if(mRepresentedObject && mRepresentedObjectCleanupBlock)
		mRepresentedObjectCleanupBlock(mRepresentedObject);
	mRepresentedObject = representedObject;
}

void SFB::Audio::Decoder::SetRepresentedObjectCleanupBlock(RepresentedObjectCleanupBlock block)
{
	if(mRepresentedObjectCleanupBlock) {
		Block_release(mRepresentedObjectCleanupBlock);
		mRepresentedObjectCleanupBlock = nullptr;
	}
	if(block)
		mRepresentedObjectCleanupBlock = Block_copy(block);
}

#pragma mark Base Functionality

bool SFB::Audio::Decoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "Open() called on a Decoder that is already open");
		return true;
	}

	// Ensure the input source is open
	if(!GetInputSource().IsOpen() && !GetInputSource().Open(error))
		return false;

	bool result = _Open(error);
	if(result)
		mIsOpen = true;
	return result;
}

bool SFB::Audio::Decoder::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "Close() called on a Decoder that hasn't been opened");
		return true;
	}

	// Close the decoder
	bool result = _Close(error);
	if(result)
		mIsOpen = false;

	// Close the input source
	if(!GetInputSource().Close(error))
		return false;

	return result;
}

CFStringRef SFB::Audio::Decoder::CreateFormatDescription() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "CreateFormatDescription() called on a Decoder that hasn't been opened");
		return nullptr;
	}

	CFStringRef		sourceFormatDescription		= nullptr;
	UInt32			specifierSize				= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName,
																		 sizeof(mFormat),
																		 &mFormat,
																		 &specifierSize,
																		 &sourceFormatDescription);

	if(noErr != result)
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

	return sourceFormatDescription;
}

CFStringRef SFB::Audio::Decoder::CreateSourceFormatDescription() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "CreateSourceFormatDescription() called on a Decoder that hasn't been opened");
		return nullptr;
	}

	return _GetSourceFormatDescription().Relinquish();
}

CFStringRef SFB::Audio::Decoder::CreateChannelLayoutDescription() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "CreateChannelLayoutDescription() called on a Decoder that hasn't been opened");
		return nullptr;
	}

	CFStringRef		channelLayoutDescription	= nullptr;
	UInt32			specifierSize				= sizeof(channelLayoutDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName,
																		 sizeof(mChannelLayout.GetACL()),
																		 mChannelLayout.GetACL(),
																		 &specifierSize,
																		 &channelLayoutDescription);

	if(noErr != result)
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

	return channelLayoutDescription;
}

UInt32 SFB::Audio::Decoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "ReadAudio() called on a Decoder that hasn't been opened");
		return 0;
	}

	if(nullptr == bufferList || 0 == frameCount) {
		os_log_debug(OS_LOG_DEFAULT, "ReadAudio() called with invalid parameters");
		return 0;
	}

	return _ReadAudio(bufferList, frameCount);
}

SInt64 SFB::Audio::Decoder::GetTotalFrames() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "GetTotalFrames() called on a Decoder that hasn't been opened");
		return -1;
	}

	return _GetTotalFrames();
}

SInt64 SFB::Audio::Decoder::GetCurrentFrame() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "GetCurrentFrame() called on a Decoder that hasn't been opened");
		return -1;
	}

	return _GetCurrentFrame();
}

bool SFB::Audio::Decoder::SupportsSeeking() const
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "SupportsSeeking() called on a Decoder that hasn't been opened");
		return false;
	}

	return _SupportsSeeking();
}

SInt64 SFB::Audio::Decoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen()) {
		os_log_info(OS_LOG_DEFAULT, "SeekToFrame() called on a Decoder that hasn't been opened");
		return -1;
	}

	if(0 > frame || frame >= GetTotalFrames()) {
		os_log_debug(OS_LOG_DEFAULT, "SeekToFrame() called with invalid parameters");
		return -1;
	}

	return _SeekToFrame(frame);
}
