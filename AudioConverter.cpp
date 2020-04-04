/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>

#include <os/log.h>

#include "AudioBufferList.h"
#include "AudioConverter.h"
#include "CFWrapper.h"
#include "SFBCStringForOSType.h"

namespace {

	// AudioConverter input callback
	OSStatus MyAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
												  UInt32						*ioNumberDataPackets,
												  AudioBufferList				*ioData,
												  AudioStreamPacketDescription	**outDataPacketDescription,
												  void							*inUserData)
	{
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

		SFB::Audio::Converter *converter = static_cast<SFB::Audio::Converter *>(inUserData);
		UInt32 framesRead = converter->DecodeAudio(ioData, *ioNumberDataPackets);

		*ioNumberDataPackets = framesRead;

		return noErr;
	}
}

SFB::Audio::Converter::Converter(Decoder::unique_ptr decoder, const AudioStreamBasicDescription& format, ChannelLayout channelLayout)
	: mFormat(format), mChannelLayout(channelLayout), mDecoder(std::move(decoder)), mConverter(nullptr), mIsOpen(false)
{}

SFB::Audio::Converter::~Converter()
{
	if(IsOpen())
		Close();
}

bool SFB::Audio::Converter::Open(UInt32 preferredBufferSizeFrames, CFErrorRef *error)
{
	if(!mDecoder)
		return false;

	// Open the decoder if necessary
	if(!mDecoder->IsOpen() && !mDecoder->Open(error)) {
		if(error)
			os_log_error(OS_LOG_DEFAULT, "Error opening decoder: %{public}@", *error);

		return false;
	}

	AudioStreamBasicDescription inputFormat = mDecoder->GetFormat();
	OSStatus result = AudioConverterNew(&inputFormat, &mFormat, &mConverter);
	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioConverterNew failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, nullptr);

		return false;
	}

	// Calculate input buffer size required for preferred output buffer size
	UInt32 inputBufferSize = preferredBufferSizeFrames * mFormat.mBytesPerFrame;
	UInt32 dataSize = sizeof(inputBufferSize);
	result = AudioConverterGetProperty(mConverter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &inputBufferSize);
	if(noErr != result)
		os_log_error(OS_LOG_DEFAULT, "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %d", result);

	UInt32 inputBufferSizeFrames = noErr == result ? (UInt32)mDecoder->GetFormat().ByteCountToFrameCount(inputBufferSize) : preferredBufferSizeFrames;
	if(!mBufferList.Allocate(mDecoder->GetFormat(), inputBufferSizeFrames)) {
		os_log_error(OS_LOG_DEFAULT, "Error allocating conversion buffer");

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	// Set the channel layouts
	if(mDecoder->GetChannelLayout()) {
		result = AudioConverterSetProperty(mConverter, kAudioConverterInputChannelLayout, (UInt32)mDecoder->GetChannelLayout().GetACLSize(), mDecoder->GetChannelLayout().GetACL());
		if(noErr != result) {
			os_log_error(OS_LOG_DEFAULT, "AudioConverterSetProperty (kAudioConverterInputChannelLayout) failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

			if(error)
				*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, nullptr);

			return false;
		}
	}

	if(mChannelLayout) {
		result = AudioConverterSetProperty(mConverter, kAudioConverterOutputChannelLayout, (UInt32)mChannelLayout.GetACLSize(), mChannelLayout.GetACL());
		if(noErr != result) {
			os_log_error(OS_LOG_DEFAULT, "AudioConverterSetProperty (kAudioConverterOutputChannelLayout) failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

			if(error)
				*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, nullptr);

			return false;
		}
	}

	mIsOpen = true;
	return true;
}

bool SFB::Audio::Converter::Close(CFErrorRef *error)
{
#pragma unused(error)

	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "Close() called on an AudioConverter that hasn't been opened");
		return true;
	}

	mDecoder.reset();

	if(mConverter) {
		AudioConverterDispose(mConverter);
		mConverter = nullptr;
	}

	mIsOpen = false;
	return true;
}

CFStringRef SFB::Audio::Converter::CreateFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	CFStringRef		sourceFormatDescription		= nullptr;
	UInt32			specifierSize				= sizeof(sourceFormatDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_FormatName,
																		 sizeof(mFormat),
																		 &mFormat,
																		 &specifierSize,
																		 &sourceFormatDescription);

	if(noErr != result)
		os_log_debug(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

	return sourceFormatDescription;
}

CFStringRef SFB::Audio::Converter::CreateChannelLayoutDescription() const
{
	if(!IsOpen())
		return nullptr;

	CFStringRef		channelLayoutDescription	= nullptr;
	UInt32			specifierSize				= sizeof(channelLayoutDescription);
	OSStatus		result						= AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName,
																		 sizeof(mChannelLayout),
																		 mChannelLayout,
																		 &specifierSize,
																		 &channelLayoutDescription);

	if(noErr != result)
		os_log_debug(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

	return channelLayoutDescription;
}


UInt32 SFB::Audio::Converter::ConvertAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	OSStatus result = AudioConverterFillComplexBuffer(mConverter, MyAudioConverterComplexInputDataProc, this, &frameCount, bufferList, nullptr);
	if(noErr != result)
		return 0;

	return frameCount;
}

bool SFB::Audio::Converter::Reset()
{
	if(!IsOpen())
		return false;

	OSStatus result = AudioConverterReset(mConverter);
	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioConverterReset failed: %d '%{public}.4s", result, SFBCStringForOSType(result));
		return false;
	}

	return true;
}

UInt32 SFB::Audio::Converter::DecodeAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	mBufferList.Reset();

	frameCount = std::min(frameCount, mBufferList.GetCapacityFrames());
	UInt32 framesRead = mDecoder->ReadAudio(mBufferList, frameCount);

	bufferList->mNumberBuffers = mBufferList->mNumberBuffers;
	for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex)
		bufferList->mBuffers[bufferIndex] = mBufferList->mBuffers[bufferIndex];

	return framesRead;
}
