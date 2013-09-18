/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include <libkern/OSAtomic.h>
#include <pthread.h>
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_error.h>
#include <mach/sync_policy.h>
#include <stdexcept>
#include <new>
#include <algorithm>
#include <iomanip>

#include "AudioPlayer.h"
#include "RingBuffer.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "Logger.h"

// ========================================
// Macros
// ========================================
#define RING_BUFFER_CAPACITY_FRAMES				16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6

// ========================================
// Enums
// ========================================
enum {
	eDecoderStateDataFlagDecodingStarted	= 1u << 0,
	eDecoderStateDataFlagDecodingFinished	= 1u << 1,
	eDecoderStateDataFlagRenderingStarted	= 1u << 2,
	eDecoderStateDataFlagRenderingFinished	= 1u << 3,
	eDecoderStateDataFlagStopDecoding		= 1u << 4
};

enum {
	eAudioPlayerFlagMuteOutput				= 1u << 0,
	eAudioPlayerFlagFormatMismatch			= 1u << 1
};

static void InitializeLoggingSubsystem() __attribute__ ((constructor));
static void InitializeLoggingSubsystem()
{
	// Turn off logging by default
	::logger::SetCurrentLevel(::logger::disabled);
}

// ========================================
// State data for decoders that are decoding and/or rendering
// ========================================
class AudioPlayer::DecoderStateData
{

public:

	// Takes ownership of decoder
	DecoderStateData(AudioDecoder *decoder)
		: DecoderStateData()
	{
		assert(nullptr != decoder);
		mDecoder = decoder;

		// NB: The decoder may return an estimate of the total frames
		mTotalFrames = mDecoder->GetTotalFrames();
	}

	~DecoderStateData()
	{
		// Delete the decoder
		if(mDecoder)
			delete mDecoder, mDecoder = nullptr;

		DeallocateBufferList();
	}

	DecoderStateData(const DecoderStateData& rhs) = delete;
	DecoderStateData& operator=(const DecoderStateData& rhs) = delete;

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

	AudioDecoder			*mDecoder;

	AudioBufferList			*mBufferList;
	UInt32					mBufferCapacityFrames;

	SInt64					mTimeStamp;

	SInt64					mTotalFrames;
	volatile SInt64			mFramesRendered;

	SInt64					mFrameToSeek;

	volatile uint32_t		mFlags;

private:

	DecoderStateData()
		: mDecoder(nullptr), mBufferList(nullptr), mBufferCapacityFrames(0), mTimeStamp(0), mTotalFrames(0), mFramesRendered(0), mFrameToSeek(-1), mFlags(0)
	{}

};

// ========================================
// Set the calling thread's timesharing and importance
// ========================================
static bool
setThreadPolicy(integer_t importance)
{
	return true;
	// Turn off timesharing
	thread_extended_policy_data_t extendedPolicy = {
		.timeshare = false
	};
	kern_return_t error = thread_policy_set(mach_thread_self(),
											THREAD_EXTENDED_POLICY,
											(thread_policy_t)&extendedPolicy, 
											THREAD_EXTENDED_POLICY_COUNT);
	
	if(KERN_SUCCESS != error) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Couldn't set thread's extended policy: " << mach_error_string(error));
		return false;
	}
	
	// Give the thread the specified importance
	thread_precedence_policy_data_t precedencePolicy = {
		.importance = importance
	};
	error = thread_policy_set(mach_thread_self(), 
							  THREAD_PRECEDENCE_POLICY, 
							  (thread_policy_t)&precedencePolicy, 
							  THREAD_PRECEDENCE_POLICY_COUNT);
	
	if (error != KERN_SUCCESS) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Couldn't set thread's precedence policy: " << mach_error_string(error));
		return false;
	}
	
	return true;
}

// ========================================
// The AUGraph input callback
// ========================================
static OSStatus
myAURenderCallback(void *							inRefCon,
				   AudioUnitRenderActionFlags *		ioActionFlags,
				   const AudioTimeStamp *			inTimeStamp,
				   UInt32							inBusNumber,
				   UInt32							inNumberFrames,
				   AudioBufferList *				ioData)
{
	assert(nullptr != inRefCon);

	AudioPlayer *player = static_cast<AudioPlayer *>(inRefCon);
	return player->Render(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

static OSStatus
auGraphDidRender(void *							inRefCon,
				 AudioUnitRenderActionFlags *	ioActionFlags,
				 const AudioTimeStamp *			inTimeStamp,
				 UInt32							inBusNumber,
				 UInt32							inNumberFrames,
				 AudioBufferList *				ioData)
{
	assert(nullptr != inRefCon);

	AudioPlayer *player = static_cast<AudioPlayer *>(inRefCon);
	return player->DidRender(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

// ========================================
// The decoder thread's entry point
// ========================================
static void *
decoderEntry(void *arg)
{
	assert(nullptr != arg);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(arg);
	return player->DecoderThreadEntry();
}

// ========================================
// The collector thread's entry point
// ========================================
static void *
collectorEntry(void *arg)
{
	assert(nullptr != arg);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(arg);
	return player->CollectorThreadEntry();
}

// ========================================
// AudioConverter input callback
// ========================================
static OSStatus
myAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
									 UInt32							*ioNumberDataPackets,
									 AudioBufferList				*ioData,
									 AudioStreamPacketDescription	**outDataPacketDescription,
									 void							*inUserData)
{

#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

	assert(nullptr != inUserData);
	assert(nullptr != ioNumberDataPackets);

	AudioPlayer::DecoderStateData *decoderStateData = static_cast<AudioPlayer::DecoderStateData *>(inUserData);

	decoderStateData->ResetBufferList();

	UInt32 framesRead = decoderStateData->mDecoder->ReadAudio(decoderStateData->mBufferList, *ioNumberDataPackets);

	// Point ioData at our decoded audio
	ioData->mNumberBuffers = decoderStateData->mBufferList->mNumberBuffers;
	for(UInt32 bufferIndex = 0; bufferIndex < decoderStateData->mBufferList->mNumberBuffers; ++bufferIndex)
		ioData->mBuffers[bufferIndex] = decoderStateData->mBufferList->mBuffers[bufferIndex];

	*ioNumberDataPackets = framesRead;

	return noErr;
}

#pragma mark Creation/Destruction

AudioPlayer::AudioPlayer()
	: mAUGraph(nullptr), mOutputNode(-1), mMixerNode(-1), mDefaultMaximumFramesPerSlice(0), mFlags(0), mDecoderQueue(nullptr), mRingBuffer(nullptr), mRingBufferChannelLayout(nullptr), mRingBufferCapacity(RING_BUFFER_CAPACITY_FRAMES), mRingBufferWriteChunkSize(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES), mFramesDecoded(0), mFramesRendered(0), mGuard(), mSemaphore(), mDecoderSemaphore(), mCollectorSemaphore(), mFramesRenderedLastPass(0), mFormatMismatchBlock(nullptr)
{
	memset(&mDecoderEventBlocks, 0, sizeof(mDecoderEventBlocks));
	memset(&mRenderEventBlocks, 0, sizeof(mRenderEventBlocks));

	mDecoderQueue = CFArrayCreateMutable(kCFAllocatorDefault, 0, nullptr);
	
	if(nullptr == mDecoderQueue)
		throw std::bad_alloc();

	mRingBuffer = new RingBuffer();

	// ========================================
	// Initialize the decoder array
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex)
		mActiveDecoders[bufferIndex] = nullptr;

	// ========================================
	// Launch the decoding thread
	mKeepDecoding = true;
	int creationResult = pthread_create(&mDecoderThread, nullptr, decoderEntry, this);
	if(0 != creationResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.AudioPlayer", "pthread_create failed: " << strerror(creationResult));
		
		CFRelease(mDecoderQueue), mDecoderQueue = nullptr;
		delete mRingBuffer, mRingBuffer = nullptr;

		throw std::runtime_error("pthread_create failed");
	}

	// ========================================
	// Launch the collector thread
	mKeepCollecting = true;
	creationResult = pthread_create(&mCollectorThread, nullptr, collectorEntry, this);
	if(0 != creationResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.AudioPlayer", "pthread_create failed: " << strerror(creationResult));
		
		mKeepDecoding = false;
		mDecoderSemaphore.Signal();
		
		int joinResult = pthread_join(mDecoderThread, nullptr);
		if(0 != joinResult)
			LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "pthread_join failed: " << strerror(joinResult));
		
		mDecoderThread = (pthread_t)0;
		
		CFRelease(mDecoderQueue), mDecoderQueue = nullptr;
		delete mRingBuffer, mRingBuffer = nullptr;

		throw std::runtime_error("pthread_create failed");
	}
	
	// ========================================
	// The AUGraph will always receive audio in the canonical Core Audio format
	mRingBufferFormat.mFormatID				= kAudioFormatLinearPCM;
	mRingBufferFormat.mFormatFlags			= kAudioFormatFlagsAudioUnitCanonical;

	mRingBufferFormat.mSampleRate			= 0;
	mRingBufferFormat.mChannelsPerFrame		= 0;
	mRingBufferFormat.mBitsPerChannel		= 8 * sizeof(AudioUnitSampleType);
	
	mRingBufferFormat.mBytesPerPacket		= (mRingBufferFormat.mBitsPerChannel / 8);
	mRingBufferFormat.mFramesPerPacket		= 1;
	mRingBufferFormat.mBytesPerFrame		= mRingBufferFormat.mBytesPerPacket * mRingBufferFormat.mFramesPerPacket;
	
	mRingBufferFormat.mReserved				= 0;

	// ========================================
	// Set up output
	if(!OpenOutput()) {
		LOGGER_CRIT("org.sbooth.AudioEngine.AudioPlayer", "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

AudioPlayer::~AudioPlayer()
{
	Stop();

	// Stop the processing graph and reclaim its resources
	if(!CloseOutput())
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "CloseOutput() failed");

	// End the decoding thread
	mKeepDecoding = false;
	mDecoderSemaphore.Signal();

	int joinResult = pthread_join(mDecoderThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "pthread_join failed: " << strerror(joinResult));
	
	mDecoderThread = (pthread_t)0;

	// End the collector thread
	mKeepCollecting = false;
	mCollectorSemaphore.Signal();
	
	joinResult = pthread_join(mCollectorThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "pthread_join failed: " << strerror(joinResult));
	
	mCollectorThread = (pthread_t)0;

	// Force any decoders left hanging by the collector to end
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		if(nullptr != mActiveDecoders[bufferIndex])
			delete mActiveDecoders[bufferIndex], mActiveDecoders[bufferIndex] = nullptr;
	}
	
	// Clean up any queued decoders
	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>((void *)CFArrayGetValueAtIndex(mDecoderQueue, 0));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}
	
	CFRelease(mDecoderQueue), mDecoderQueue = nullptr;

	// Clean up the ring buffer and associated resources
	if(mRingBuffer)
		delete mRingBuffer, mRingBuffer = nullptr;

	if(mRingBufferChannelLayout)
		free(mRingBufferChannelLayout), mRingBufferChannelLayout = nullptr;

	// Free the block callbacks
	if(mDecoderEventBlocks[0])
		Block_release(mDecoderEventBlocks[0]), mDecoderEventBlocks[0] = nullptr;
	if(mDecoderEventBlocks[1])
		Block_release(mDecoderEventBlocks[1]), mDecoderEventBlocks[1] = nullptr;
	if(mDecoderEventBlocks[2])
		Block_release(mDecoderEventBlocks[2]), mDecoderEventBlocks[2] = nullptr;
	if(mDecoderEventBlocks[3])
		Block_release(mDecoderEventBlocks[3]), mDecoderEventBlocks[3] = nullptr;

	if(mRenderEventBlocks[0])
		Block_release(mRenderEventBlocks[0]), mRenderEventBlocks[0] = nullptr;
	if(mRenderEventBlocks[1])
		Block_release(mRenderEventBlocks[1]), mRenderEventBlocks[1] = nullptr;

	if(mFormatMismatchBlock)
		Block_release(mFormatMismatchBlock), mFormatMismatchBlock = nullptr;
}

#pragma mark Playback Control

bool AudioPlayer::Play()
{
	if(!OutputIsRunning())
		return StartOutput();

	return true;
}

bool AudioPlayer::Pause()
{
	if(OutputIsRunning())
		StopOutput();

	return true;
}

bool AudioPlayer::Stop()
{
	Guard::Locker lock(mGuard);

	if(OutputIsRunning())
		StopOutput();

	StopActiveDecoders();
	
	ResetOutput();

	mFramesDecoded = 0;
	mFramesRendered = 0;

	return true;
}

AudioPlayer::PlayerState AudioPlayer::GetPlayerState() const
{
	if(OutputIsRunning())
		return PlayerState::Playing;

	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return PlayerState::Stopped;

	if(eDecoderStateDataFlagRenderingStarted & currentDecoderState->mFlags)
		return PlayerState::Paused;

	if(eDecoderStateDataFlagDecodingStarted & currentDecoderState->mFlags)
		return PlayerState::Pending;

	return PlayerState::Stopped;
}

CFURLRef AudioPlayer::GetPlayingURL() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return nullptr;
	
	return currentDecoderState->mDecoder->GetURL();
}

void * AudioPlayer::GetPlayingRepresentedObject() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return nullptr;

	return currentDecoderState->mDecoder->GetRepresentedObject();
}

#pragma mark Block-based callback support

void AudioPlayer::SetDecodingStartedBlock(AudioPlayer::AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[0])
		Block_release(mDecoderEventBlocks[0]), mDecoderEventBlocks[0] = nullptr;
	if(block)
		mDecoderEventBlocks[0] = Block_copy(block);
}

void AudioPlayer::SetDecodingFinishedBlock(AudioPlayer::AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[1])
		Block_release(mDecoderEventBlocks[1]), mDecoderEventBlocks[1] = nullptr;
	if(block)
		mDecoderEventBlocks[1] = Block_copy(block);
}

void AudioPlayer::SetRenderingStartedBlock(AudioPlayer::AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[2])
		Block_release(mDecoderEventBlocks[2]), mDecoderEventBlocks[2] = nullptr;
	if(block)
		mDecoderEventBlocks[2] = Block_copy(block);
}

void AudioPlayer::SetRenderingFinishedBlock(AudioPlayer::AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[3])
		Block_release(mDecoderEventBlocks[3]), mDecoderEventBlocks[3] = nullptr;
	if(block)
		mDecoderEventBlocks[3] = Block_copy(block);
}

void AudioPlayer::SetPreRenderBlock(AudioPlayer::AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[0])
		Block_release(mRenderEventBlocks[0]), mRenderEventBlocks[0] = nullptr;
	if(block)
		mRenderEventBlocks[0] = Block_copy(block);
}

void AudioPlayer::SetPostRenderBlock(AudioPlayer::AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[1])
		Block_release(mRenderEventBlocks[1]), mRenderEventBlocks[1] = nullptr;
	if(block)
		mRenderEventBlocks[1] = Block_copy(block);
}

void AudioPlayer::SetFormatMismatchBlock(AudioPlayerFormatMismatchBlock block)
{
	if(mFormatMismatchBlock)
		Block_release(mFormatMismatchBlock), mFormatMismatchBlock = nullptr;
	if(block)
		mFormatMismatchBlock = Block_copy(block);
}

#pragma mark Playback Properties

bool AudioPlayer::GetCurrentFrame(SInt64& currentFrame) const
{
	SInt64 totalFrames;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool AudioPlayer::GetTotalFrames(SInt64& totalFrames) const
{
	SInt64 currentFrame;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool AudioPlayer::GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	currentFrame	= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	totalFrames		= currentDecoderState->mTotalFrames;

	return true;
}

bool AudioPlayer::GetCurrentTime(CFTimeInterval& currentTime) const
{
	CFTimeInterval totalTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool AudioPlayer::GetTotalTime(CFTimeInterval& totalTime) const
{
	CFTimeInterval currentTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool AudioPlayer::GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	Float64 sampleRate		= currentDecoderState->mDecoder->GetFormat().mSampleRate;
	currentTime				= currentFrame / sampleRate;
	totalTime				= totalFrames / sampleRate;

	return true;
}

bool AudioPlayer::GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	totalFrames			= currentDecoderState->mTotalFrames;
	Float64 sampleRate	= currentDecoderState->mDecoder->GetFormat().mSampleRate;
	currentTime			= currentFrame / sampleRate;
	totalTime			= totalFrames / sampleRate;

	return true;	
}

#pragma mark Seeking

bool AudioPlayer::SeekForward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;

	SInt64 frameCount		= (SInt64)(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 desiredFrame		= currentFrame + frameCount;
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::min(desiredFrame, totalFrames - 1));
}

bool AudioPlayer::SeekBackward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;

	SInt64 frameCount		= (SInt64)(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 desiredFrame		= currentFrame - frameCount;
	
	return SeekToFrame(std::max(0LL, desiredFrame));
}

bool AudioPlayer::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	SInt64 desiredFrame		= (SInt64)(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool AudioPlayer::SeekToFrame(SInt64 frame)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	if(!currentDecoderState->mDecoder->SupportsSeeking())
		return false;
	
	if(0 > frame || frame >= currentDecoderState->mTotalFrames)
		return false;

	if(!OSAtomicCompareAndSwap64Barrier(currentDecoderState->mFrameToSeek, frame, &currentDecoderState->mFrameToSeek))
		return false;

	mDecoderSemaphore.Signal();

	return true;	
}

bool AudioPlayer::SupportsSeeking() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}

#pragma mark Player Parameters

bool AudioPlayer::GetVolume(Float32& volume) const
{
	return GetVolumeForChannel(0, volume);
}

bool AudioPlayer::SetVolume(Float32 volume)
{
	return SetVolumeForChannel(0, volume);
}

bool AudioPlayer::GetVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitGetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetVolumeForChannel(UInt32 channel, Float32 volume)
{
	if(0 > volume || 1 < volume)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Volume for channel " << channel << " set to " << volume);

	return true;
}

bool AudioPlayer::GetPreGain(Float32& preGain) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}
	
	result = AudioUnitGetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, &preGain);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetPreGain(Float32 preGain)
{
	if(0 > preGain || 1 < preGain)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, preGain, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Pregain set to " << preGain);

	return true;
}

bool AudioPlayer::IsPerformingSampleRateConversion() const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 sampleRate;
	UInt32 dataSize = sizeof(sampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate) failed: " << result);
		return false;
	}

	return (sampleRate != mRingBufferFormat.mSampleRate);
}

bool AudioPlayer::GetSampleRateConverterComplexity(UInt32& complexity) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(complexity);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetSampleRateConverterComplexity(UInt32 complexity)
{
	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Setting sample rate converter quality to " << complexity);

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, sizeof(complexity));
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

#if !TARGET_OS_IPHONE

#pragma mark Hog Mode

bool AudioPlayer::OutputDeviceIsHogged() const
{
	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool AudioPlayer::StartHoggingOutputDevice()
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Taking hog mode for device 0x" << std::hex << deviceID);

	// Is it hogged already?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// The device is already hogged
	if(hogPID != (pid_t)-1) {
		LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Device is already hogged by pid: " << hogPID);
		return false;
	}

	bool restartIO = OutputIsRunning();
	if(restartIO)
		StopOutput();

	hogPID = getpid();

	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(restartIO && !OutputIsRunning())
		StartOutput();

	return true;
}

bool AudioPlayer::StopHoggingOutputDevice()
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Releasing hog mode for device 0x" << std::hex << deviceID);

	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If we don't own hog mode we can't release it
	if(hogPID != getpid())
		return false;

	bool restartIO = OutputIsRunning();
	if(restartIO)
		StopOutput();

	// Release hog mode.
	hogPID = (pid_t)-1;

	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	if(restartIO && !OutputIsRunning())
		StartOutput();

	return true;
}

#pragma mark Device Parameters

bool AudioPlayer::GetDeviceMasterVolume(Float32& volume) const
{
	return GetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::SetDeviceMasterVolume(Float32 volume)
{
	return SetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyVolumeScalar, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= channel 
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetDeviceVolumeForChannel(UInt32 channel, Float32 volume)
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Setting output device 0x" << std::hex << deviceID << " channel " << channel << " volume to " << volume);

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyVolumeScalar, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= channel 
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	OSStatus result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(volume), &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::GetDeviceChannelCount(UInt32& channelCount) const
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyStreamConfiguration, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) is false");
		return false;
	}

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);

	if(nullptr == bufferList) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Unable to allocate << " << dataSize << " bytes");
		return false;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, bufferList);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		free(bufferList), bufferList = nullptr;
		return false;
	}

	channelCount = 0;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;

	free(bufferList), bufferList = nullptr;
	return true;
}

bool AudioPlayer::GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyPreferredChannelsForStereo, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed is false");
		return false;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &preferredChannels);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	preferredStereoChannels.first = preferredChannels[0];
	preferredStereoChannels.second = preferredChannels[1];

	return true;
}

#pragma mark DSP Effects

bool AudioPlayer::AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit1)
{
	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Adding DSP effect: " << subType << " " << manufacturer);

	// Get the source node for the graph's output node
	UInt32 numInteractions = 0;
	OSStatus result = AUGraphCountNodeInteractions(mAUGraph, mOutputNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [numInteractions];

	result = AUGraphGetNodeInteractions(mAUGraph, mOutputNode, &numInteractions, interactions);	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeInteractions failed: " << result);
		return false;
	}

	AUNode sourceNode = -1;
	for(UInt32 interactionIndex = 0; interactionIndex < numInteractions; ++interactionIndex) {
		AUNodeInteraction interaction = interactions[interactionIndex];

		if(kAUNodeInteraction_Connection == interaction.nodeInteractionType && mOutputNode == interaction.nodeInteraction.connection.destNode) {
			sourceNode = interaction.nodeInteraction.connection.sourceNode;
			break;
		}
	}						

	// Unable to determine the preceding node, so bail
	if(-1 == sourceNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to determine input node");
		return false;
	}

	// Create the effect node and set its format
	AudioComponentDescription desc = { kAudioUnitType_Effect, subType, manufacturer, flags, mask };

	AUNode effectNode = -1;
	result = AUGraphAddNode(mAUGraph, &desc, &effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphAddNode failed: " << result);
		return false;
	}

	AudioUnit effectUnit = nullptr;
	result = AUGraphNodeInfo(mAUGraph, effectNode, nullptr, &effectUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphRemoveNode failed: " << result);

		return false;
	}

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphRemoveNode failed: " << result);

		return false;
	}
#endif

//	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &mRingBufferFormat, sizeof(mRingBufferFormat));
//	if(noErr != result) {
////		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);
//
//		// If the property couldn't be set (the AU may not support this format), remove the new node
//		result = AUGraphRemoveNode(mAUGraph, effectNode);
//		if(noErr != result)
//			;//			ERR("AUGraphRemoveNode failed: %i", result);
//
//		return false;
//	}
//
//	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &mRingBufferFormat, sizeof(mRingBufferFormat));
//	if(noErr != result) {
////		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);
//
//		// If the property couldn't be set (the AU may not support this format), remove the new node
//		result = AUGraphRemoveNode(mAUGraph, effectNode);
//		if(noErr != result)
//			;			//ERR("AUGraphRemoveNode failed: %i", result);
//
//		return false;
//	}

	// Insert the effect at the end of the graph, before the output node
	result = AUGraphDisconnectNodeInput(mAUGraph, mOutputNode, 0);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphDisconnectNodeInput failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphRemoveNode failed: " << result);

		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, effectNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphUpdate failed: " << result);

		// If the update failed, restore the previous node state
		result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, mOutputNode, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
			return false;
		}
	}

	if(nullptr != effectUnit1)
		*effectUnit1 = effectUnit;

	return true;
}

bool AudioPlayer::RemoveEffect(AudioUnit effectUnit)
{
	if(nullptr == effectUnit)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Removing DSP effect: " << effectUnit);

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	AUNode effectNode = -1;
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = -1;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		// This is the unit to remove
		if(effectUnit == au) {
			effectNode = node;
			break;
		}
	}

	if(-1 == effectNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to find the AUNode for the specified AudioUnit");
		return false;
	}

	// Get the current input and output nodes for the node to delete
	UInt32 numInteractions = 0;
	result = AUGraphCountNodeInteractions(mAUGraph, effectNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction *interactions = (AUNodeInteraction *)calloc(numInteractions, sizeof(AUNodeInteraction));
	if(nullptr == interactions) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to allocate memory");
		return false;
	}

	result = AUGraphGetNodeInteractions(mAUGraph, effectNode, &numInteractions, interactions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeInteractions failed: " << result);

		free(interactions), interactions = nullptr;

		return false;
	}

	AUNode sourceNode = -1, destNode = -1;
	for(UInt32 interactionIndex = 0; interactionIndex < numInteractions; ++interactionIndex) {
		AUNodeInteraction interaction = interactions[interactionIndex];

		if(kAUNodeInteraction_Connection == interaction.nodeInteractionType) {
			if(effectNode == interaction.nodeInteraction.connection.destNode)
				sourceNode = interaction.nodeInteraction.connection.sourceNode;
			else if(effectNode == interaction.nodeInteraction.connection.sourceNode)
				destNode = interaction.nodeInteraction.connection.destNode;
		}
	}						

	free(interactions), interactions = nullptr;

	if(-1 == sourceNode || -1 == destNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to find the source or destination nodes");
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphRemoveNode(mAUGraph, effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphRemoveNode failed: " << result);
		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphUpdate failed: " << result);
		return false;
	}

	return true;
}

#pragma mark Device Management

bool AudioPlayer::CreateOutputDeviceUID(CFStringRef& deviceUID) const
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyDeviceUID, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	UInt32 dataSize = sizeof(deviceUID);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &deviceUID);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: " << result);
		return nullptr;
	}

	return true;
}

bool AudioPlayer::SetOutputDeviceUID(CFStringRef deviceUID)
{
	AudioDeviceID deviceID = kAudioDeviceUnknown;

	// If nullptr was passed as the device UID, use the default output device
	if(nullptr == deviceUID) {
		AudioObjectPropertyAddress propertyAddress = { 
			.mSelector	= kAudioHardwarePropertyDefaultOutputDevice, 
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster 
		};

		UInt32 specifierSize = sizeof(deviceID);

		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &specifierSize, &deviceID);
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << result);
			return false;
		}
	}
	else {
		AudioObjectPropertyAddress propertyAddress = { 
			.mSelector	= kAudioHardwarePropertyDeviceForUID, 
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster 
		};

		AudioValueTranslation translation = {
			&deviceUID, sizeof(deviceUID),
			&deviceID, sizeof(deviceID)
		};

		UInt32 specifierSize = sizeof(translation);

		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &specifierSize, &translation);
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: " << result);
			return false;
		}
	}

	// The device isn't connected or doesn't exist
	if(kAudioDeviceUnknown == deviceID)
		return false;

	return SetOutputDeviceID(deviceID);
}

bool AudioPlayer::GetOutputDeviceID(AudioDeviceID& deviceID) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(deviceID);

	result = AudioUnitGetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetOutputDeviceID(AudioDeviceID deviceID)
{
	if(kAudioDeviceUnknown == deviceID)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Update our output AU to use the specified device
	result = AudioUnitSetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, sizeof(deviceID));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::GetOutputDeviceSampleRate(Float64& sampleRate) const
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyNominalSampleRate, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	UInt32 dataSize = sizeof(sampleRate);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &sampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetOutputDeviceSampleRate(Float64 sampleRate)
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	// Determine if this will actually be a change
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyNominalSampleRate, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	Float64 currentSampleRate;
	UInt32 dataSize = sizeof(currentSampleRate);
	
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &currentSampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	// Nothing to do
	if(currentSampleRate == sampleRate)
		return true;

	// Set the sample rate
	dataSize = sizeof(sampleRate);
	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(sampleRate), &sampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

#endif

#pragma mark Playlist Management

bool AudioPlayer::Enqueue(CFURLRef url)
{
	if(nullptr == url)
		return false;
	
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(url);
	
	if(nullptr == decoder)
		return false;
	
	bool success = Enqueue(decoder);
	
	if(!success)
		delete decoder;
	
	return success;
}

bool AudioPlayer::Enqueue(AudioDecoder *decoder)
{
	if(nullptr == decoder)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Enqueuing \"" << decoder->GetURL() << "\"");

	// The lock is held for the entire method, because enqueuing a track is an inherently
	// sequential operation.  Without the lock, if Enqueue() is called from multiple
	// threads a crash can occur in mRingBuffer->Allocate() under a sitation similar to the following:
	//  1. Thread A calls Enqueue() for decoder A
	//  2. Thread B calls Enqueue() for decoder B
	//  3. Both threads enter the if(nullptr == GetCurrentDecoderState() && queueEmpty) block
	//  4. Thread A is suspended
	//  5. Thread B finishes the ring buffer setup, and signals the decoding thread
	//  6. The decoding thread starts decoding
	//  7. Thread A is awakened, and immediately allocates a new ring buffer
	//  8. The decoding or rendering threads crash, because the memory they are using was freed out
	//     from underneath them
	// In practce, the only time I've seen this happen is when using GuardMalloc, presumably because the 
	// normal execution time of Enqueue() isn't sufficient to lead to this condition.
	Mutex::Locker lock(mGuard);

	bool queueEmpty = (0 == CFArrayGetCount(mDecoderQueue));		

	// If there are no decoders in the queue, set up for playback
	if(nullptr == GetCurrentDecoderState() && queueEmpty) {
		if(!SetupAUGraphAndRingBufferForDecoder(decoder))
			return false;
	}

	// Add the decoder to the queue
	CFArrayAppendValue(mDecoderQueue, decoder);

	mDecoderSemaphore.Signal();
	
	return true;
}

bool AudioPlayer::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &currentDecoderState->mFlags);

	// Signal the decoding thread that decoding is finished (inner loop)
	mDecoderSemaphore.Signal();

	// Wait for decoding to finish or a SIGSEGV could occur if the collector collects an active decoder
	mach_timespec_t timeout = {
		.tv_sec = 0,
		.tv_nsec = NSEC_PER_SEC / 10
	};

	while(!(eDecoderStateDataFlagDecodingFinished & currentDecoderState->mFlags))
		mSemaphore.TimedWait(timeout);

	OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &currentDecoderState->mFlags);

	// Effect a flush of the ring buffer
	mFramesDecoded = 0;
	mFramesRendered = 0;
	
	// Signal the decoding thread to start the next decoder (outer loop)
	mDecoderSemaphore.Signal();

	OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	return true;
}

bool AudioPlayer::ClearQueuedDecoders()
{
	Mutex::Tryer lock(mGuard);
	if(!lock)
		return false;

	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>((void *)CFArrayGetValueAtIndex(mDecoderQueue, 0));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}

	return true;	
}

#pragma mark Ring Buffer Parameters

bool AudioPlayer::SetRingBufferCapacity(uint32_t bufferCapacity)
{
	if(0 == bufferCapacity || mRingBufferWriteChunkSize > bufferCapacity)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Setting ring buffer capacity to " << bufferCapacity);

	return OSAtomicCompareAndSwap32Barrier((int32_t)mRingBufferCapacity, (int32_t)bufferCapacity, (int32_t *)&mRingBufferCapacity);
}

bool AudioPlayer::SetRingBufferWriteChunkSize(uint32_t chunkSize)
{
	if(0 == chunkSize || mRingBufferCapacity < chunkSize)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Setting ring buffer write chunk size to " << chunkSize);

	return OSAtomicCompareAndSwap32Barrier((int32_t)mRingBufferWriteChunkSize, (int32_t)chunkSize, (int32_t *)&mRingBufferWriteChunkSize);
}

#pragma mark Callbacks

OSStatus AudioPlayer::Render(AudioUnitRenderActionFlags		*ioActionFlags,
								const AudioTimeStamp			*inTimeStamp,
								UInt32							inBusNumber,
								UInt32							inNumberFrames,
								AudioBufferList					*ioData)
{

#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)

	assert(nullptr != ioActionFlags);
	assert(nullptr != ioData);

	// Mute functionality
	if(eAudioPlayerFlagMuteOutput & mFlags)
		return noErr;

	// If the ring buffer doesn't contain any valid audio, skip some work
	UInt32 framesAvailableToRead = (UInt32)(mFramesDecoded - mFramesRendered);
	if(0 == framesAvailableToRead) {
		*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		
		size_t byteCountToZero = inNumberFrames * sizeof(AudioUnitSampleType);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			memset(ioData->mBuffers[bufferIndex].mData, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
		}
		
		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min(framesAvailableToRead, inNumberFrames);
	if(!mRingBuffer->Fetch(ioData, framesToRead, mFramesRendered)) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "RingBuffer::Fetch failed: Requested " << framesToRead << " frames from " << mFramesRendered);
		return 1;
	}

	// Call the pre-render block
	if(mRenderEventBlocks[0])
		mRenderEventBlocks[0](ioData, framesToRead);

	mFramesRenderedLastPass = framesToRead;
	OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);

	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesToRead != inNumberFrames) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Insufficient audio in ring buffer: " << framesToRead << " frames available, " << inNumberFrames << " requested");
		
		UInt32 framesOfSilence = inNumberFrames - framesToRead;
		size_t byteCountToZero = framesOfSilence * sizeof(AudioUnitSampleType);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			AudioUnitSampleType *bufferAlias = (AudioUnitSampleType *)ioData->mBuffers[bufferIndex].mData;
			memset(bufferAlias + framesToRead, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize += byteCountToZero;
		}
	}

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = (UInt32)(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));
	
	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
		mDecoderSemaphore.Signal();

	return noErr;
}

OSStatus AudioPlayer::DidRender(AudioUnitRenderActionFlags		*ioActionFlags,
								   const AudioTimeStamp				*inTimeStamp,
								   UInt32							inBusNumber,
								   UInt32							inNumberFrames,
								   AudioBufferList					*ioData)
{
	
#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)
#pragma unused(inNumberFrames)
#pragma unused(ioData)
	
	if(kAudioUnitRenderAction_PostRender & (*ioActionFlags)) {

		// Call the post-render block
		if(mRenderEventBlocks[1])
			mRenderEventBlocks[1](ioData, inNumberFrames);

		// There is nothing more to do if no frames were rendered
		if(0 == mFramesRenderedLastPass)
			return noErr;

		// mFramesRenderedLastPass contains the number of valid frames that were rendered
		// However, these could have come from any number of decoders depending on the buffer sizes
		// So it is necessary to split them up here

		SInt64 framesRemainingToDistribute = mFramesRenderedLastPass;
		DecoderStateData *decoderState = GetCurrentDecoderState();

		// mActiveDecoders is not an ordered array, so to ensure that callbacks are performed
		// in the proper order multiple passes are made here
		while(nullptr != decoderState) {
			SInt64 timeStamp = decoderState->mTimeStamp;

			SInt64 decoderFramesRemaining = (-1 == decoderState->mTotalFrames ? mFramesRenderedLastPass : decoderState->mTotalFrames - decoderState->mFramesRendered);
			SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, (SInt64)mFramesRenderedLastPass);

			if(0 == decoderState->mFramesRendered && !(eDecoderStateDataFlagRenderingStarted & decoderState->mFlags)) {
				// Call the rendering started block
				if(mDecoderEventBlocks[2])
					mDecoderEventBlocks[2](decoderState->mDecoder);
				OSAtomicTestAndSetBarrier(5 /* eDecoderStateDataFlagRenderingStarted */, &decoderState->mFlags);
			}

			OSAtomicAdd64Barrier(framesFromThisDecoder, &decoderState->mFramesRendered);

			if((eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) && decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)*/) {
				// Call the rendering finished block
				if(mDecoderEventBlocks[3])
					mDecoderEventBlocks[3](decoderState->mDecoder);

				OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
				decoderState = nullptr;

				// Since rendering is finished, signal the collector to clean up this decoder
				mCollectorSemaphore.Signal();
			}

			framesRemainingToDistribute -= framesFromThisDecoder;

			if(0 == framesRemainingToDistribute)
				break;

			decoderState = GetDecoderStateStartingAfterTimeStamp(timeStamp);
		}

		if(mFramesDecoded == mFramesRendered && nullptr == GetCurrentDecoderState()) {
			// Signal the decoding thread that it is safe to manipulate the ring buffer
			if(eAudioPlayerFlagFormatMismatch & mFlags) {
				OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
				OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagFormatMismatch */, &mFlags);
				mSemaphore.Signal();
			}
			else
				StopOutput();
		}
	}

	return noErr;
}

#pragma mark Thread Entry Points

void * AudioPlayer::DecoderThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Decoder");

	// ========================================
	// Make ourselves a high priority thread
	if(!setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Couldn't set decoder thread importance");

	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = {
		.tv_sec = 2,
		.tv_nsec = 0
	};

	while(mKeepDecoding) {

		// ========================================
		// Try to lock the queue and remove the head element, which contains the next decoder to use
		DecoderStateData *decoderState = nullptr;
		{
			Mutex::Tryer lock(mGuard);

			if(lock && 0 < CFArrayGetCount(mDecoderQueue)) {
				AudioDecoder *decoder = (AudioDecoder *)CFArrayGetValueAtIndex(mDecoderQueue, 0);

				// Create the decoder state
				decoderState = new DecoderStateData(decoder);
				decoderState->mTimeStamp = mFramesDecoded;

				CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
			}
		}

		// ========================================
		// Open the decoder if necessary
		if(decoderState) {
			CFErrorRef error = nullptr;
			if(!decoderState->mDecoder->IsOpen() && !decoderState->mDecoder->Open(&error))  {
				if(error) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Error opening decoder: " << error);
					CFRelease(error), error = nullptr;
				}

				// TODO: Perform CouldNotOpenDecoder() callback ??

				delete decoderState, decoderState = nullptr;
			}
		}

		// ========================================
		// Ensure the decoder's format is compatible with the ring buffer
		if(decoderState) {
			AudioStreamBasicDescription		nextFormat			= decoderState->mDecoder->GetFormat();
			AudioChannelLayout				*nextChannelLayout	= decoderState->mDecoder->GetChannelLayout();

			// The two files can be joined seamlessly only if they have the same sample rates and channel counts
			bool formatsMatch = true;

			if(nextFormat.mSampleRate != mRingBufferFormat.mSampleRate) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Gapless join failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mChannelsPerFrame != mRingBufferFormat.mChannelsPerFrame) {
				LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Gapless join failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
				formatsMatch = false;
			}

			// Enqueue the decoder if its channel layout matches the ring buffer's channel layout (so the channel map in the output AU will remain valid)
			if(nextChannelLayout && mRingBufferChannelLayout) {
				AudioChannelLayout *layouts [] = {
					nextChannelLayout,
					mRingBufferChannelLayout
				};

				UInt32 layoutsEqual = false;
				UInt32 propertySize = sizeof(layoutsEqual);
				OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), (void *)layouts, &propertySize, &layoutsEqual);

				if(noErr != result)
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: " << result);

				if(!layoutsEqual) {
					LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "Gapless join failed: Ring buffer channel layout (" << mRingBufferChannelLayout << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
					formatsMatch = false;
				}
			}
			else if((nullptr == nextChannelLayout || nullptr == mRingBufferChannelLayout) && nextChannelLayout != mRingBufferChannelLayout)
				formatsMatch = false;

			// If the formats don't match, the decoder can't be used with the current ring buffer format
			if(!formatsMatch) {
				// Set a flag indicating the rendering thread should output silence after the current decoder completes
				OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagFormatMismatch */, &mFlags);

				// Wait for the currently rendering decoder to finish
				mach_timespec_t renderTimeout = {
					.tv_sec = 0,
					.tv_nsec = NSEC_PER_SEC / 100
				};

				// The rendering thread will clear eAudioPlayerFlagFormatMismatch when rendering is finished
				while(eAudioPlayerFlagFormatMismatch & mFlags)
					mSemaphore.TimedWait(renderTimeout);

				if(mFormatMismatchBlock)
					mFormatMismatchBlock(mRingBufferFormat, nextFormat);

				// Adjust the formats
				{
					Mutex::Tryer lock(mGuard);
					if(lock) {
						SetupAUGraphAndRingBufferForDecoder(decoderState->mDecoder);

						// Effect a flush of the ring buffer so if Stop() was called in mFormatMismatchBlock playback can still continue
						mFramesDecoded = 0;
						mFramesRendered = 0;

						decoderState->mTimeStamp = mFramesDecoded;
					}
					else
						delete decoderState, decoderState = nullptr;
				}

				// Clear the mute flag that was set in the rendering thread
				OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
			}
		}

		// ========================================
		// Append the decoder state to the list of active decoders
		if(decoderState) {
			for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
				if(nullptr != mActiveDecoders[bufferIndex])
					continue;
				
				if(OSAtomicCompareAndSwapPtrBarrier(nullptr, decoderState, (void **)&mActiveDecoders[bufferIndex]))
					break;
				else
					LOGGER_WARNING("org.sbooth.AudioEngine.AudioPlayer", "OSAtomicCompareAndSwapPtrBarrier() failed");
			}
		}
		
		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(decoderState) {
			AudioDecoder *decoder = decoderState->mDecoder;

			LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Decoding starting for \"" << decoder->GetURL() << "\"");
			LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Decoder format: " << decoder->GetFormat());
			LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Decoder channel layout: " << decoder->GetChannelLayout());

			SInt64 startTime = decoderState->mTimeStamp;

			AudioStreamBasicDescription decoderFormat = decoder->GetFormat();

			// ========================================
			// Create the AudioConverter which will convert from the decoder's format to the graph's format
			AudioConverterRef audioConverter = nullptr;
			OSStatus result = AudioConverterNew(&decoderFormat, &mRingBufferFormat, &audioConverter);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioConverterNew failed: " << result);

				OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
				OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);

				decoderState = nullptr;

				// If this happens, output will be impossible
				mCollectorSemaphore.Signal();

				continue;
			}

			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
			UInt32 inputBufferSize = mRingBufferWriteChunkSize * mRingBufferFormat.mBytesPerFrame;
			UInt32 dataSize = sizeof(inputBufferSize);
			result = AudioConverterGetProperty(audioConverter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &inputBufferSize);
			if(noErr != result)
				LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);
			
			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer			
			decoderState->AllocateBufferList(inputBufferSize / decoderFormat.mBytesPerFrame);

			AudioBufferList *bufferList = AllocateABL(mRingBufferFormat, mRingBufferWriteChunkSize);

			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(mKeepDecoding && decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags)) {

				// Fill the ring buffer with as much data as possible
				for(;;) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = (UInt32)(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));

					// Force writes to the ring buffer to be at least mRingBufferWriteChunkSize
					if(mRingBufferWriteChunkSize <= framesAvailableToWrite) {

						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "Seeking to frame " << decoderState->mFrameToSeek);

							OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
							
							SInt64 currentFrameBeforeSeeking = decoder->GetCurrentFrame();
							
							SInt64 newFrame = decoder->SeekToFrame(decoderState->mFrameToSeek);
							
							if(newFrame != decoderState->mFrameToSeek)
								LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Error seeking to frame  " << decoderState->mFrameToSeek);
							
							// Update the seek request
							if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFrameToSeek, -1, &decoderState->mFrameToSeek))
								LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");
							
							// If the seek failed do not update the counters
							if(-1 != newFrame) {
								SInt64 framesSkipped = newFrame - currentFrameBeforeSeeking;
								
								// Treat the skipped frames as if they were rendered, and update the counters accordingly
								if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFramesRendered, newFrame, &decoderState->mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");
								
								OSAtomicAdd64Barrier(framesSkipped, &mFramesDecoded);
								if(!OSAtomicCompareAndSwap64Barrier(mFramesRendered, mFramesDecoded, &mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");

								// Reset the converter and output to flush any buffers
								result = AudioConverterReset(audioConverter);
								if(noErr != result)
									LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioConverterReset failed: " << result);
							}

							OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
						}

						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to determine starting frame number ");
							break;
						}

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber && !(eDecoderStateDataFlagDecodingStarted & decoderState->mFlags)) {
							// Call the decoding started block
							if(mDecoderEventBlocks[0])
								mDecoderEventBlocks[0](decoder);
							OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingStarted */, &decoderState->mFlags);
						}

						// Read the input chunk, converting from the decoder's format to the AUGraph's format
						UInt32 framesDecoded = mRingBufferWriteChunkSize;
						
						result = AudioConverterFillComplexBuffer(audioConverter, myAudioConverterComplexInputDataProc, decoderState, &framesDecoded, bufferList, nullptr);
						if(noErr != result)
							LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioConverterFillComplexBuffer failed: " << result);

						// Store the decoded audio
						if(0 != framesDecoded) {
							if(!mRingBuffer->Store(bufferList, framesDecoded, startingFrameNumber + startTime))
								LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "RingBuffer::Store failed");

							OSAtomicAdd64Barrier(framesDecoded, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)*/) {
							LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Decoding finished for \"" << decoder->GetURL() << "\"");

							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update 
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							// Call the decoding finished block
							if(mDecoderEventBlocks[1])
								mDecoderEventBlocks[1](decoder);
							
							// Decoding is complete
							OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
							decoderState = nullptr;

							break;
						}
					}
					// Not enough space remains in the ring buffer to write an entire decoded chunk
					else
						break;
				}

				// Wait for the audio rendering thread to signal us that it could use more data, or for the timeout to happen
				mDecoderSemaphore.TimedWait(timeout);
			}
			
			// ========================================
			// Clean up
			// Set the appropriate flags for collection if decoding was stopped early
			if(decoderState) {
				OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
				decoderState = nullptr;

				// If eAudioPlayerFlagMuteOutput is set SkipToNextTrack() is waiting for this decoder to finish
				if(eAudioPlayerFlagMuteOutput & mFlags)
					mSemaphore.Signal();
			}

			if(bufferList)
				DeallocateABL(bufferList), bufferList = nullptr;
			
			if(audioConverter) {
				result = AudioConverterDispose(audioConverter);
				if(noErr != result)
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioConverterDispose failed: " << result);
				audioConverter = nullptr;
			}
		}

		// Wait for another thread to wake us, or for the timeout to happen
		mDecoderSemaphore.TimedWait(timeout);
	}

	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Decoding thread terminating");

	return nullptr;
}

void * AudioPlayer::CollectorThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Collector");

	// The collector should be signaled when there is cleanup to be done, so there is no need for a short timeout
	mach_timespec_t timeout = {
		.tv_sec = 30,
		.tv_nsec = 0
	};

	while(mKeepCollecting) {
		
		for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
			DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
			
			if(nullptr == decoderState)
				continue;

			if(!(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) || !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags))
				continue;

			bool swapSucceeded = OSAtomicCompareAndSwapPtrBarrier(decoderState, nullptr, (void **)&mActiveDecoders[bufferIndex]);

			if(swapSucceeded)
				delete decoderState, decoderState = nullptr;
		}
		
		// Wait for any thread to signal us to try and collect finished decoders
		mCollectorSemaphore.TimedWait(timeout);
	}
	
	LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Collecting thread terminating");
	
	return nullptr;
}

#pragma mark AudioHardware Utilities

bool AudioPlayer::OpenOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "OpenOutput");

	OSStatus result = NewAUGraph(&mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "NewAUGraph failed: " << result);
		return false;
	}

	// The graph will look like:
	// MultiChannelMixer -> Output
	AudioComponentDescription desc;

	// Set up the mixer node
	desc.componentType			= kAudioUnitType_Mixer;
	desc.componentSubType		= kAudioUnitSubType_MultiChannelMixer;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= kAudioComponentFlag_SandboxSafe;
	desc.componentFlagsMask		= 0;

	result = AUGraphAddNode(mAUGraph, &desc, &mMixerNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
	
	// Set up the output node
	desc.componentType			= kAudioUnitType_Output;
#if TARGET_OS_IPHONE
	desc.componentSubType		= kAudioUnitSubType_RemoteIO;
	desc.componentFlags			= 0;
#else
	desc.componentSubType		= kAudioUnitSubType_HALOutput;
	desc.componentFlags			= kAudioComponentFlag_SandboxSafe;
#endif
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlagsMask		= 0;

	result = AUGraphAddNode(mAUGraph, &desc, &mOutputNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, mMixerNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
		
		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);
		
		mAUGraph = nullptr;
		return false;
	}
	
	// Install the input callback
	AURenderCallbackStruct cbs = { myAURenderCallback, this };
	result = AUGraphSetNodeInputCallback(mAUGraph, mMixerNode, 0, &cbs);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphSetNodeInputCallback failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
	
	// Open the graph
	result = AUGraphOpen(mAUGraph);	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphOpen failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Set the mixer's volume on the input and output
	AudioUnit au = nullptr;
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
	
	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
	
	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Output) failed: " << result);
	
	// Install the render notification
	result = AUGraphAddRenderNotify(mAUGraph, auGraphDidRender, this);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphAddRenderNotify failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#else
	// Save the default value of kAudioUnitProperty_MaximumFramesPerSlice for use when performing sample rate conversion
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 dataSize = sizeof(mDefaultMaximumFramesPerSlice);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mDefaultMaximumFramesPerSlice, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#endif

	// Initialize the graph
	result = AUGraphInitialize(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphInitialize failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	return true;
}

bool AudioPlayer::CloseOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "CloseOutput");

	Boolean graphIsRunning = false;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsRunning failed: " << result);
		return false;
	}

	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphStop failed: " << result);
			return false;
		}
	}

	Boolean graphIsInitialized = false;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsInitialized failed: " << result);
		return false;
	}

	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	result = AUGraphClose(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphClose failed: " << result);
		return false;
	}

	result = DisposeAUGraph(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "DisposeAUGraph failed: " << result);
		return false;
	}

	mAUGraph = nullptr;
	mMixerNode = -1;
	mOutputNode = -1;

	return true;
}

bool AudioPlayer::StartOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "StartOutput");

	// We don't want to start output in the middle of a buffer modification
	Mutex::Locker lock(mGuard);

	OSStatus result = AUGraphStart(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphStart failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::StopOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "StopOutput");

	OSStatus result = AUGraphStop(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphStop failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::OutputIsRunning() const
{
	Boolean isRunning = false;
	OSStatus result = AUGraphIsRunning(mAUGraph, &isRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsRunning failed: " << result);
		return false;
	}

	return isRunning;
}

bool AudioPlayer::ResetOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "Resetting output");

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsRunning failed: " << result);
		return false;
	}

	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		result = AudioUnitReset(au, kAudioUnitScope_Global, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitReset failed: " << result);
			return false;
		}
	}

	return true;
}

#pragma mark AUGraph Utilities

bool AudioPlayer::GetAUGraphLatency(Float64& latency) const
{
	latency = 0;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auLatency = 0;
		UInt32 dataSize = sizeof(auLatency);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &auLatency, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_Latency, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		latency += auLatency;
	}

	return true;
}

bool AudioPlayer::GetAUGraphTailTime(Float64& tailTime) const
{
	tailTime = 0;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auTailTime = 0;
		UInt32 dataSize = sizeof(auTailTime);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &auTailTime, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_TailTime, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		tailTime += auTailTime;
	}

	return true;
}

bool AudioPlayer::SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize)
{
	if(nullptr == propertyData || 0 >= propertyDataSize)
		return  false;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	// Iterate through the nodes and attempt to set the property
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);

		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNodeCount failed: " << result);
			return false;
		}

		if(mOutputNode == node) {
			// For AUHAL as the output node, you can't set the device side, so just set the client side
			result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, 0, propertyData, propertyDataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
				return false;
			}
		}
		else {
			UInt32 elementCount = 0;
			UInt32 dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &elementCount, &dataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Input) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Input, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, j, propertyData, propertyDataSize);				
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
					return false;
				}
			}

			elementCount = 0;
			dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &elementCount, &dataSize);			
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Output) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Output, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Output, j, propertyData, propertyDataSize);				
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
					return false;
				}
			}
		}
	}

	return true;
}

bool AudioPlayer::SetAUGraphSampleRateAndChannelsPerFrame(Float64 sampleRate, UInt32 channelsPerFrame)
{
	// ========================================
	// If the graph is running, stop it
	Boolean graphIsRunning = FALSE;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsRunning failed: " << result);
		return false;
	}
	
	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphStop failed: " << result);
			return false;
		}
	}
	
	// ========================================
	// If the graph is initialized, uninitialize it
	Boolean graphIsInitialized = FALSE;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphIsInitialized failed: " << result);
		return false;
	}
	
	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	// ========================================
	// Save the interaction information and then clear all the connections
	UInt32 interactionCount = 0;
	result = AUGraphGetNumberOfInteractions(mAUGraph, &interactionCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetNumberOfInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [interactionCount];

	for(UInt32 i = 0; i < interactionCount; ++i) {
		result = AUGraphGetInteractionInfo(mAUGraph, i, &interactions[i]);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphGetInteractionInfo failed: " << result);
			return false;
		}
	}

	result = AUGraphClearConnections(mAUGraph);	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphClearConnections failed: " << result);
		return false;
	}
	
	AudioStreamBasicDescription format = mRingBufferFormat;
	
	format.mChannelsPerFrame	= channelsPerFrame;
	format.mSampleRate			= sampleRate;

	// ========================================
	// Attempt to set the new stream format
	if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &format, sizeof(format))) {
		// If the new format could not be set, restore the old format to ensure a working graph
		if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &mRingBufferFormat, sizeof(mRingBufferFormat))) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to restore AUGraph format: " << result);
		}

		// Do not free connections here, so graph can be rebuilt
	}
	else
		mRingBufferFormat = format;

	// ========================================
	// Restore the graph's connections and input callbacks
	for(UInt32 i = 0; i < interactionCount; ++i) {
		switch(interactions[i].nodeInteractionType) {

				// Reestablish the connection
			case kAUNodeInteraction_Connection:
			{
				result = AUGraphConnectNodeInput(mAUGraph, 
												 interactions[i].nodeInteraction.connection.sourceNode, 
												 interactions[i].nodeInteraction.connection.sourceOutputNumber,
												 interactions[i].nodeInteraction.connection.destNode, 
												 interactions[i].nodeInteraction.connection.destInputNumber);
				
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphConnectNodeInput failed: " << result);
					return false;
				}

				break;
			}

				// Reestablish the input callback
			case kAUNodeInteraction_InputCallback:
			{
				result = AUGraphSetNodeInputCallback(mAUGraph, 
													 interactions[i].nodeInteraction.inputCallback.destNode, 
													 interactions[i].nodeInteraction.inputCallback.destInputNumber,
													 &interactions[i].nodeInteraction.inputCallback.cback);

				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphSetNodeInputCallback failed: " << result);
					return false;
				}

				break;
			}				
		}
	}

#if !TARGET_OS_IPHONE
	// ========================================
	// Output units perform sample rate conversion if the input sample rate is not equal to
	// the output sample rate. For high sample rates, the sample rate conversion can require 
	// more rendered frames than are available by default in kAudioUnitProperty_MaximumFramesPerSlice (512)
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the output device don't match, adjust 
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	AudioUnit au = nullptr;
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 inputSampleRate = 0;
	UInt32 dataSize = sizeof(inputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0, &inputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	Float64 outputSampleRate = 0;
	dataSize = sizeof(outputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &outputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Output) failed: " << result);
		return false;
	}

	UInt32 newMaxFrames = mDefaultMaximumFramesPerSlice;

	// If the output unit's input and output sample rates don't match, calculate a working maximum number of frames per slice
	if(inputSampleRate != outputSampleRate) {
		LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Input sample rate (" << inputSampleRate << ") and output sample rate (" << outputSampleRate << ") don't match");
		
		Float64 ratio = inputSampleRate / outputSampleRate;
		Float64 multiplier = std::max(1.0, ratio);

		// Round up to the nearest 16 frames
		newMaxFrames = (UInt32)ceil(mDefaultMaximumFramesPerSlice * multiplier);
		newMaxFrames += 16;
		newMaxFrames &= 0xFFFFFFF0;
	}

	UInt32 currentMaxFrames = 0;
	dataSize = sizeof(currentMaxFrames);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &currentMaxFrames, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);
		return false;
	}

	// Adjust the maximum frames per slice if necessary
	if(newMaxFrames != currentMaxFrames) {
		LOGGER_INFO("org.sbooth.AudioEngine.AudioPlayer", "Adjusting kAudioUnitProperty_MaximumFramesPerSlice to " << newMaxFrames);

		if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_MaximumFramesPerSlice, &newMaxFrames, sizeof(newMaxFrames))) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "SetPropertyOnAUGraphNodes (kAudioUnitProperty_MaximumFramesPerSlice) failed");
			return false;
		}
	}
#endif

	// If the graph was initialized, reinitialize it
	if(graphIsInitialized) {
		result = AUGraphInitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphInitialize failed: " << result);
			return false;
		}
	}

	// If the graph was running, restart it
	if(graphIsRunning) {
		result = AUGraphStart(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphStart failed: " << result);
			return false;
		}
	}

	return true;
}

bool AudioPlayer::SetOutputUnitChannelMap(AudioChannelLayout *channelLayout)
{
#if !TARGET_OS_IPHONE
	AudioUnit outputUnit = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &outputUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Clear the existing channel map
	result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, nullptr, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	if(nullptr == channelLayout)
		return true;

	// Get the device's preferred channel layout
	UInt32 devicePreferredChannelLayoutSize = 0;
	result = AudioUnitGetPropertyInfo(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, &devicePreferredChannelLayoutSize, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetPropertyInfo (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);
		return false;
	}

	AudioChannelLayout *devicePreferredChannelLayout = (AudioChannelLayout *)malloc(devicePreferredChannelLayoutSize);

	result = AudioUnitGetProperty(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, devicePreferredChannelLayout, &devicePreferredChannelLayoutSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitGetProperty (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);

		if(devicePreferredChannelLayout)
			free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

		return false;
	}

//	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "Device preferred channel layout: " << devicePreferredChannelLayout);
//	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "Audio channel layout: " << channelLayout);

	UInt32 channelCount = 0;
	UInt32 dataSize = sizeof(channelCount);
	result = AudioFormatGetProperty(kAudioFormatProperty_NumberOfChannelsForLayout, devicePreferredChannelLayoutSize, devicePreferredChannelLayout, &dataSize, &channelCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: " << result);

		if(devicePreferredChannelLayout)
			free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;
		
		return false;
	}

	// Create the channel map
	SInt32 channelMap [ channelCount ];
	dataSize = (UInt32)sizeof(channelMap);

	AudioChannelLayout *channelLayouts [] = {
		channelLayout,
		devicePreferredChannelLayout
	};

	result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(channelLayouts), channelLayouts, &dataSize, channelMap);

	if(devicePreferredChannelLayout)
		free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);
		return false;
	}

	LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "Using channel map: ");
	for(UInt32 i = 0; i < channelCount; ++i)
		LOGGER_DEBUG("org.sbooth.AudioEngine.AudioPlayer", "  " << i << " -> " << channelMap[i]);

	// Set the channel map
	result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, channelMap, sizeof(channelMap));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
		return false;
	}
#endif

	return true;
}

#pragma mark Other Utilities

AudioPlayer::DecoderStateData * AudioPlayer::GetCurrentDecoderState() const
{
	DecoderStateData *result = nullptr;
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(nullptr == decoderState)
			continue;
		
		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)
			continue;

		if(nullptr == result)
			result = decoderState;
		else if(decoderState->mTimeStamp < result->mTimeStamp)
			result = decoderState;
	}
	
	return result;
}

AudioPlayer::DecoderStateData * AudioPlayer::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const
{
	DecoderStateData *result = nullptr;
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(nullptr == decoderState)
			continue;
		
		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)
			continue;

		if(nullptr == result && decoderState->mTimeStamp > timeStamp)
			result = decoderState;
		else if(result && decoderState->mTimeStamp > timeStamp && decoderState->mTimeStamp < result->mTimeStamp)
			result = decoderState;
	}
	
	return result;
}

void AudioPlayer::StopActiveDecoders()
{
	// The player must be stopped or a SIGSEGV could occur in this method
	// This must be ensured by the caller!

	// Request that any decoders still actively decoding stop
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(nullptr == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &decoderState->mFlags);
	}

	mDecoderSemaphore.Signal();

	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(nullptr == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
	}

	mCollectorSemaphore.Signal();
}

bool AudioPlayer::SetupAUGraphAndRingBufferForDecoder(AudioDecoder *decoder)
{
	if(nullptr == decoder)
		return false;

	// Open the decoder if necessary
	CFErrorRef error = nullptr;
	if(!decoder->IsOpen() && !decoder->Open(&error)) {
		if(error) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Error opening decoder: " << error);
			CFRelease(error), error = nullptr;
		}

		return false;
	}

	AudioStreamBasicDescription format = decoder->GetFormat();
	if(!SetAUGraphSampleRateAndChannelsPerFrame(format.mSampleRate, format.mChannelsPerFrame))
		return false;

	// Attempt to set the output audio unit's channel map
	AudioChannelLayout *channelLayout = decoder->GetChannelLayout();
	if(!SetOutputUnitChannelMap(channelLayout))
		return false;

	// The decoder's channel layout becomes the ring buffer's channel layout
	if(mRingBufferChannelLayout)
		free(mRingBufferChannelLayout), mRingBufferChannelLayout = nullptr;

	mRingBufferChannelLayout = CopyChannelLayout(channelLayout);

	// Allocate enough space in the ring buffer for the new format
	if(!mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame, mRingBufferFormat.mBytesPerFrame, mRingBufferCapacity)) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioPlayer", "Unable to allocate ring buffer");
		return false;
	}

	return true;
}
