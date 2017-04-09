/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>

#include "AudioConverter.h"
#include "AudioBufferList.h"
#include "Logger.h"
#include "CFWrapper.h"
#include "CreateStringForOSType.h"

#define BUFFER_SIZE_FRAMES 512u

// ========================================
// State data for conversion
// ========================================
class SFB::Audio::Converter::ConverterStateData
{
public:

	ConverterStateData() = delete;

	explicit ConverterStateData(Decoder& decoder)
		: mDecoder(decoder)
	{}

	ConverterStateData(const ConverterStateData& rhs) = delete;
	ConverterStateData& operator=(const ConverterStateData& rhs) = delete;

	void AllocateBufferList(UInt32 capacityFrames)
	{
		mBufferList.Allocate(mDecoder.GetFormat(), capacityFrames);
	}

	UInt32 ReadAudio(UInt32 frameCount)
	{
		mBufferList.Reset();

		frameCount = std::min(frameCount, mBufferList.GetCapacityFrames());
		return mDecoder.ReadAudio(mBufferList, frameCount);
	}

	Decoder&		mDecoder;
	BufferList		mBufferList;
};

namespace {

	// AudioConverter input callback
	OSStatus myAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
												  UInt32						*ioNumberDataPackets,
												  AudioBufferList				*ioData,
												  AudioStreamPacketDescription	**outDataPacketDescription,
												  void							*inUserData)
	{
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

		SFB::Audio::Converter::ConverterStateData *converterStateData = static_cast<SFB::Audio::Converter::ConverterStateData *>(inUserData);
		UInt32 framesRead = converterStateData->ReadAudio(*ioNumberDataPackets);

		ioData->mNumberBuffers = converterStateData->mBufferList->mNumberBuffers;
		for(UInt32 bufferIndex = 0; bufferIndex < converterStateData->mBufferList->mNumberBuffers; ++bufferIndex)
			ioData->mBuffers[bufferIndex] = converterStateData->mBufferList->mBuffers[bufferIndex];

		*ioNumberDataPackets = framesRead;

		return noErr;
	}
}

SFB::Audio::Converter::Converter(Decoder::unique_ptr decoder, const AudioStreamBasicDescription& format, ChannelLayout channelLayout)
	: mFormat(format), mChannelLayout(std::move(channelLayout)), mDecoder(std::move(decoder)), mConverter(nullptr), mConverterState(nullptr), mIsOpen(false)
{}

SFB::Audio::Converter::~Converter()
{
	if(IsOpen())
		Close();
}

bool SFB::Audio::Converter::Open(CFErrorRef *error)
{
	if(!mDecoder)
		return false;

	// Open the decoder if necessary
	if(!mDecoder->IsOpen() && !mDecoder->Open(error)) {
		if(error)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioConverter", "Error opening decoder: " << error);

		return false;
	}

	AudioStreamBasicDescription inputFormat = mDecoder->GetFormat();
	OSStatus result = AudioConverterNew(&inputFormat, &mFormat, &mConverter);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioConverter", "AudioConverterNewfailed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, nullptr);

		return false;
	}

	// TODO: Set kAudioConverterPropertyCalculateInputBufferSize

	mConverterState = std::unique_ptr<ConverterStateData>(new ConverterStateData(*mDecoder));
	mConverterState->AllocateBufferList(BUFFER_SIZE_FRAMES);

	// Create the channel map
	if(mChannelLayout) {
		SInt32 channelMap [ mFormat.mChannelsPerFrame ];
		UInt32 dataSize = (UInt32)sizeof(channelMap);

		const AudioChannelLayout *channelLayouts [] = {
			mDecoder->GetChannelLayout(),
			mChannelLayout
		};

		result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(channelLayouts), channelLayouts, &dataSize, channelMap);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioConverter", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);

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
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioConverter", "Close() called on an AudioConverter that hasn't been opened");
		return true;
	}

	mConverterState.reset();
	mDecoder.reset();

	if(mConverter)
		AudioConverterDispose(mConverter), mConverter = nullptr;

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
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioConverter", "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

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
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioConverter", "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutName) failed: " << result << "'" << SFB::StringForOSType((OSType)result) << "'");

	return channelLayoutDescription;
}


UInt32 SFB::Audio::Converter::ConvertAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	OSStatus result = AudioConverterFillComplexBuffer(mConverter, myAudioConverterComplexInputDataProc, mConverterState.get(), &frameCount, bufferList, nullptr);
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
		LOGGER_ERR("org.sbooth.AudioEngine.AudioConverter", "AudioConverterReset failed: " << result);
		return false;
	}

	return true;
}
