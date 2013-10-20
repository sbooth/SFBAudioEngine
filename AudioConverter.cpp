/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include <algorithm>

#include "AudioConverter.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "Logger.h"
#include "CFWrapper.h"
#include "CreateStringForOSType.h"

#define BUFFER_SIZE_FRAMES 512u

// ========================================
// State data for conversion
// ========================================
class SFB::AudioConverter::ConverterStateData
{	
public:	

	ConverterStateData(AudioDecoder *decoder)
		: mDecoder(decoder), mBufferList(nullptr), mBufferCapacityFrames(0)
	{}

	~ConverterStateData()
	{
		// Only a weak reference is held to mDecoder
		mDecoder = nullptr;
		DeallocateBufferList();
	}

	ConverterStateData(const ConverterStateData& rhs) = delete;
	ConverterStateData& operator=(const ConverterStateData& rhs) = delete;

	void AllocateBufferList(UInt32 capacityFrames)
	{
		DeallocateBufferList();

		mBufferCapacityFrames = capacityFrames;
		mBufferList = AllocateABL(mDecoder->GetFormat(), mBufferCapacityFrames);
	}

	void DeallocateBufferList()
	{
		if(mBufferList) {
			mBufferCapacityFrames = 0;
			mBufferList = DeallocateABL(mBufferList);
		}
	}

	void ResetBufferList()
	{
		AudioStreamBasicDescription formatDescription = mDecoder->GetFormat();

		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
			mBufferList->mBuffers[i].mDataByteSize = mBufferCapacityFrames * formatDescription.mBytesPerFrame;
	}

	UInt32 ReadAudio(UInt32 frameCount)
	{
		ResetBufferList();

		frameCount = std::min(frameCount, mBufferCapacityFrames);
		return mDecoder->ReadAudio(mBufferList, frameCount);
	}

	AudioDecoder			*mDecoder;
	AudioBufferList			*mBufferList;
	UInt32					mBufferCapacityFrames;

private:
	ConverterStateData()
	{}
};

namespace {

	// AudioConverter input callback
	OSStatus myAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
												  UInt32						*ioNumberDataPackets,
												  AudioBufferList				*ioData,
												  AudioStreamPacketDescription	**outDataPacketDescription,
												  void							*inUserData)
	{
		SFB::AudioConverter::ConverterStateData *converterStateData = static_cast<SFB::AudioConverter::ConverterStateData *>(inUserData);
		UInt32 framesRead = converterStateData->ReadAudio(*ioNumberDataPackets);

		ioData->mNumberBuffers = converterStateData->mBufferList->mNumberBuffers;
		for(UInt32 bufferIndex = 0; bufferIndex < converterStateData->mBufferList->mNumberBuffers; ++bufferIndex)
			ioData->mBuffers[bufferIndex] = converterStateData->mBufferList->mBuffers[bufferIndex];

		*ioNumberDataPackets = framesRead;

		return noErr;
	}
}

SFB::AudioConverter::AudioConverter(AudioDecoder *decoder, const AudioStreamBasicDescription& format, AudioChannelLayout *channelLayout)
	: mDecoder(decoder), mFormat(format), mConverter(nullptr), mConverterState(nullptr), mIsOpen(false)
{
	mChannelLayout = CopyChannelLayout(channelLayout);
}

SFB::AudioConverter::~AudioConverter()
{
	if(IsOpen())
		Close();

	if(mChannelLayout)
		free(mChannelLayout), mChannelLayout = nullptr;
}

bool SFB::AudioConverter::Open(CFErrorRef *error)
{
	if(nullptr == mDecoder)
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

		delete mConverterState, mConverterState = nullptr;
		delete mDecoder, mDecoder = nullptr;

		return false;
	}

	// TODO: Set kAudioConverterPropertyCalculateInputBufferSize

	mConverterState = new ConverterStateData(mDecoder);
	mConverterState->AllocateBufferList(BUFFER_SIZE_FRAMES);

	// Create the channel map
	if(mChannelLayout) {
		SInt32 channelMap [ mFormat.mChannelsPerFrame ];
		UInt32 dataSize = (UInt32)sizeof(channelMap);

		AudioChannelLayout *channelLayouts [] = {
			mDecoder->GetChannelLayout(),
			mChannelLayout
		};

		result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(channelLayouts), channelLayouts, &dataSize, channelMap);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioConverter", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);

			if(error)
				*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, nullptr);

			delete mConverterState, mConverterState = nullptr;
			delete mDecoder, mDecoder = nullptr;
			
			return false;
		}
	}

	mIsOpen = true;
	return true;
}

bool SFB::AudioConverter::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioConverter", "Close() called on an AudioConverter that hasn't been opened");
		return true;
	}

	if(mConverterState)
		delete mConverterState, mConverterState = nullptr;
	
	if(mDecoder)
		delete mDecoder, mDecoder = nullptr;
	
	if(mConverter)
		AudioConverterDispose(mConverter), mConverter = nullptr;

	mIsOpen = false;
	return true;
}

CFStringRef SFB::AudioConverter::CreateFormatDescription() const
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

CFStringRef SFB::AudioConverter::CreateChannelLayoutDescription() const
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


UInt32 SFB::AudioConverter::ConvertAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	OSStatus result = AudioConverterFillComplexBuffer(mConverter, myAudioConverterComplexInputDataProc, mConverterState, &frameCount, bufferList, nullptr);
	if(noErr != result)
		return 0;

	return frameCount;
}

bool SFB::AudioConverter::Reset()
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
