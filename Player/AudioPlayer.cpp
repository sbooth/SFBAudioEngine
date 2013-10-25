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
	eAudioPlayerFlagFormatMismatch			= 1u << 1,
	eAudioPlayerFlagRequestMute				= 1u << 2,
	eAudioPlayerFlagRingBufferNeedsReset	= 1u << 3
};

namespace {

	// ========================================
	// Turn off logging by default
	void InitializeLoggingSubsystem() __attribute__ ((constructor));
	void InitializeLoggingSubsystem()
	{
		::SFB::Logger::SetCurrentLevel(::SFB::Logger::disabled);
	}

}

// ========================================
// State data for decoders that are decoding and/or rendering
// ========================================
class SFB::Audio::Player::DecoderStateData
{

public:

	// Takes ownership of decoder
	DecoderStateData(Decoder *decoder)
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

	UInt32 ReadAudio(UInt32 frameCount)
	{
		ResetBufferList();

		frameCount = std::min(frameCount, mBufferCapacityFrames);
		return mDecoder->ReadAudio(mBufferList, frameCount);
	}

	Decoder					*mDecoder;

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

namespace {

	// ========================================
	// Set the calling thread's timesharing and importance
	bool setThreadPolicy(integer_t importance)
	{
		// Turn off timesharing
		thread_extended_policy_data_t extendedPolicy = {
			.timeshare = false
		};
		kern_return_t error = thread_policy_set(mach_thread_self(),
												THREAD_EXTENDED_POLICY,
												(thread_policy_t)&extendedPolicy,
												THREAD_EXTENDED_POLICY_COUNT);

		if(KERN_SUCCESS != error) {
			LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Couldn't set thread's extended policy: " << mach_error_string(error));
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
			LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Couldn't set thread's precedence policy: " << mach_error_string(error));
			return false;
		}

		return true;
	}

	// ========================================
	// AUGraph input callback
	OSStatus myAURenderCallback(void							*inRefCon,
								AudioUnitRenderActionFlags		*ioActionFlags,
								const AudioTimeStamp			*inTimeStamp,
								UInt32							inBusNumber,
								UInt32							inNumberFrames,
								AudioBufferList					*ioData)
	{
		assert(nullptr != inRefCon);

		auto player = static_cast<SFB::Audio::Player *>(inRefCon);
		return player->Render(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	}

	// ========================================
	// AUGraph render notify callback
	OSStatus auGraphRenderNotify(void							*inRefCon,
								 AudioUnitRenderActionFlags		*ioActionFlags,
								 const AudioTimeStamp			*inTimeStamp,
								 UInt32							inBusNumber,
								 UInt32							inNumberFrames,
								 AudioBufferList				*ioData)
	{
		assert(nullptr != inRefCon);

		auto player = static_cast<SFB::Audio::Player *>(inRefCon);
		return player->RenderNotify(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	}

	// ========================================
	// The decoder thread's entry point
	void * decoderEntry(void *arg)
	{
		assert(nullptr != arg);

		auto player = static_cast<SFB::Audio::Player *>(arg);
		return player->DecoderThreadEntry();
	}

	// ========================================
	// The collector thread's entry point
	void * collectorEntry(void *arg)
	{
		assert(nullptr != arg);

		auto player = static_cast<SFB::Audio::Player *>(arg);
		return player->CollectorThreadEntry();
	}

	// ========================================
	// AudioConverter input callback
	OSStatus myAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
												  UInt32						*ioNumberDataPackets,
												  AudioBufferList				*ioData,
												  AudioStreamPacketDescription	**outDataPacketDescription,
												  void							*inUserData)
	{

#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

		assert(nullptr != inUserData);
		assert(nullptr != ioNumberDataPackets);

		auto decoderStateData = static_cast<SFB::Audio::Player::DecoderStateData *>(inUserData);
		UInt32 framesRead = decoderStateData->ReadAudio(*ioNumberDataPackets);

		// Point ioData at our decoded audio
		ioData->mNumberBuffers = decoderStateData->mBufferList->mNumberBuffers;
		for(UInt32 bufferIndex = 0; bufferIndex < decoderStateData->mBufferList->mNumberBuffers; ++bufferIndex)
			ioData->mBuffers[bufferIndex] = decoderStateData->mBufferList->mBuffers[bufferIndex];
		
		*ioNumberDataPackets = framesRead;
		
		return noErr;
	}

}

#pragma mark Creation/Destruction

SFB::Audio::Player::Player()
	: mAUGraph(nullptr), mOutputNode(-1), mMixerNode(-1), mDefaultMaximumFramesPerSlice(0), mFlags(0), mDecoderQueue(nullptr), mRingBuffer(nullptr), mRingBufferChannelLayout(nullptr), mRingBufferCapacity(RING_BUFFER_CAPACITY_FRAMES), mRingBufferWriteChunkSize(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES), mFramesDecoded(0), mFramesRendered(0), mMutex(), mSemaphore(), mDecoderSemaphore(), mCollectorSemaphore(), mFramesRenderedLastPass(0), mFormatMismatchBlock(nullptr)
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
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "pthread_create failed: " << strerror(creationResult));
		
		CFRelease(mDecoderQueue), mDecoderQueue = nullptr;
		delete mRingBuffer, mRingBuffer = nullptr;

		throw std::runtime_error("pthread_create failed");
	}

	// ========================================
	// Launch the collector thread
	mKeepCollecting = true;
	creationResult = pthread_create(&mCollectorThread, nullptr, collectorEntry, this);
	if(0 != creationResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "pthread_create failed: " << strerror(creationResult));
		
		mKeepDecoding = false;
		mDecoderSemaphore.Signal();
		
		int joinResult = pthread_join(mDecoderThread, nullptr);
		if(0 != joinResult)
			LOGGER_WARNING("org.sbooth.AudioEngine.Player", "pthread_join failed: " << strerror(joinResult));
		
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
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

SFB::Audio::Player::~Player()
{
	Stop();

	// Stop the processing graph and reclaim its resources
	if(!CloseOutput())
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "CloseOutput() failed");

	// End the decoding thread
	mKeepDecoding = false;
	mDecoderSemaphore.Signal();

	int joinResult = pthread_join(mDecoderThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "pthread_join failed: " << strerror(joinResult));
	
	mDecoderThread = (pthread_t)0;

	// End the collector thread
	mKeepCollecting = false;
	mCollectorSemaphore.Signal();
	
	joinResult = pthread_join(mCollectorThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "pthread_join failed: " << strerror(joinResult));
	
	mCollectorThread = (pthread_t)0;

	// Force any decoders left hanging by the collector to end
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		if(nullptr != mActiveDecoders[bufferIndex])
			delete mActiveDecoders[bufferIndex], mActiveDecoders[bufferIndex] = nullptr;
	}
	
	// Clean up any queued decoders
	while(0 < CFArrayGetCount(mDecoderQueue)) {
		Decoder *decoder = static_cast<Decoder *>((void *)CFArrayGetValueAtIndex(mDecoderQueue, 0));
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

bool SFB::Audio::Player::Play()
{
	if(!OutputIsRunning())
		return StartOutput();

	return true;
}

bool SFB::Audio::Player::Pause()
{
	if(OutputIsRunning())
		StopOutput();

	return true;
}

bool SFB::Audio::Player::Stop()
{
	Mutex::Tryer lock(mMutex);
	if(!lock)
		return false;

	if(OutputIsRunning())
		StopOutput();

	StopActiveDecoders();
	
	ResetOutput();

	// Reset the ring buffer
	mFramesDecoded = 0;
	mFramesRendered = 0;

	OSAtomicTestAndSetBarrier(4 /* eAudioPlayerFlagRingBufferNeedsReset */, &mFlags);

	return true;
}

SFB::Audio::Player::PlayerState SFB::Audio::Player::GetPlayerState() const
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

CFURLRef SFB::Audio::Player::GetPlayingURL() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return nullptr;
	
	return currentDecoderState->mDecoder->GetURL();
}

void * SFB::Audio::Player::GetPlayingRepresentedObject() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return nullptr;

	return currentDecoderState->mDecoder->GetRepresentedObject();
}

#pragma mark Block-based callback support

void SFB::Audio::Player::SetDecodingStartedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[0])
		Block_release(mDecoderEventBlocks[0]), mDecoderEventBlocks[0] = nullptr;
	if(block)
		mDecoderEventBlocks[0] = Block_copy(block);
}

void SFB::Audio::Player::SetDecodingFinishedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[1])
		Block_release(mDecoderEventBlocks[1]), mDecoderEventBlocks[1] = nullptr;
	if(block)
		mDecoderEventBlocks[1] = Block_copy(block);
}

void SFB::Audio::Player::SetRenderingStartedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[2])
		Block_release(mDecoderEventBlocks[2]), mDecoderEventBlocks[2] = nullptr;
	if(block)
		mDecoderEventBlocks[2] = Block_copy(block);
}

void SFB::Audio::Player::SetRenderingFinishedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[3])
		Block_release(mDecoderEventBlocks[3]), mDecoderEventBlocks[3] = nullptr;
	if(block)
		mDecoderEventBlocks[3] = Block_copy(block);
}

void SFB::Audio::Player::SetPreRenderBlock(AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[0])
		Block_release(mRenderEventBlocks[0]), mRenderEventBlocks[0] = nullptr;
	if(block)
		mRenderEventBlocks[0] = Block_copy(block);
}

void SFB::Audio::Player::SetPostRenderBlock(AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[1])
		Block_release(mRenderEventBlocks[1]), mRenderEventBlocks[1] = nullptr;
	if(block)
		mRenderEventBlocks[1] = Block_copy(block);
}

void SFB::Audio::Player::SetFormatMismatchBlock(AudioPlayerFormatMismatchBlock block)
{
	if(mFormatMismatchBlock)
		Block_release(mFormatMismatchBlock), mFormatMismatchBlock = nullptr;
	if(block)
		mFormatMismatchBlock = Block_copy(block);
}

#pragma mark Playback Properties

bool SFB::Audio::Player::GetCurrentFrame(SInt64& currentFrame) const
{
	SInt64 totalFrames;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool SFB::Audio::Player::GetTotalFrames(SInt64& totalFrames) const
{
	SInt64 currentFrame;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool SFB::Audio::Player::GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	currentFrame	= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	totalFrames		= currentDecoderState->mTotalFrames;

	return true;
}

bool SFB::Audio::Player::GetCurrentTime(CFTimeInterval& currentTime) const
{
	CFTimeInterval totalTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool SFB::Audio::Player::GetTotalTime(CFTimeInterval& totalTime) const
{
	CFTimeInterval currentTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool SFB::Audio::Player::GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool SFB::Audio::Player::GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool SFB::Audio::Player::SeekForward(CFTimeInterval secondsToSkip)
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

bool SFB::Audio::Player::SeekBackward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;

	SInt64 frameCount		= (SInt64)(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 desiredFrame		= currentFrame - frameCount;
	
	return SeekToFrame(std::max(0LL, desiredFrame));
}

bool SFB::Audio::Player::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	SInt64 desiredFrame		= (SInt64)(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool SFB::Audio::Player::SeekToFrame(SInt64 frame)
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

	// Force a flush of the ring buffer to prevent audible seek artifacts
	if(!OutputIsRunning())
		OSAtomicTestAndSetBarrier(4 /* eAudioPlayerFlagRingBufferNeedsReset */, &mFlags);

	mDecoderSemaphore.Signal();

	return true;	
}

bool SFB::Audio::Player::SupportsSeeking() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}

#pragma mark Player Parameters

bool SFB::Audio::Player::GetVolume(Float32& volume) const
{
	return GetVolumeForChannel(0, volume);
}

bool SFB::Audio::Player::SetVolume(Float32 volume)
{
	return SetVolumeForChannel(0, volume);
}

bool SFB::Audio::Player::GetVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitGetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::SetVolumeForChannel(UInt32 channel, Float32 volume)
{
	if(0 > volume || 1 < volume)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Volume for channel " << channel << " set to " << volume);

	return true;
}

bool SFB::Audio::Player::GetPreGain(Float32& preGain) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}
	
	result = AudioUnitGetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, &preGain);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}
	
	return true;
}

bool SFB::Audio::Player::SetPreGain(Float32 preGain)
{
	if(0 > preGain || 1 < preGain)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, preGain, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Pregain set to " << preGain);

	return true;
}

bool SFB::Audio::Player::IsPerformingSampleRateConversion() const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 sampleRate;
	UInt32 dataSize = sizeof(sampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate) failed: " << result);
		return false;
	}

	return (sampleRate != mRingBufferFormat.mSampleRate);
}

bool SFB::Audio::Player::GetSampleRateConverterComplexity(UInt32& complexity) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(complexity);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::SetSampleRateConverterComplexity(UInt32 complexity)
{
	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting sample rate converter quality to " << complexity);

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, (UInt32)sizeof(complexity));
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

#pragma mark DSP Effects

bool SFB::Audio::Player::AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit1)
{
	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Adding DSP effect: " << subType << " " << manufacturer);

	// Get the source node for the graph's output node
	UInt32 numInteractions = 0;
	OSStatus result = AUGraphCountNodeInteractions(mAUGraph, mOutputNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [numInteractions];

	result = AUGraphGetNodeInteractions(mAUGraph, mOutputNode, &numInteractions, interactions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeInteractions failed: " << result);
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to determine input node");
		return false;
	}

	// Create the effect node and set its format
	AudioComponentDescription componentDescription = {
		.componentType = kAudioUnitType_Effect,
		.componentSubType = subType,
		.componentManufacturer = manufacturer,
		.componentFlags = flags,
		.componentFlagsMask = mask
	};

	AUNode effectNode = -1;
	result = AUGraphAddNode(mAUGraph, &componentDescription, &effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphAddNode failed: " << result);
		return false;
	}

	AudioUnit effectUnit = nullptr;
	result = AUGraphNodeInfo(mAUGraph, effectNode, nullptr, &effectUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphRemoveNode failed: " << result);

		return false;
	}

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, (UInt32)sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphRemoveNode failed: " << result);

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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphDisconnectNodeInput failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphRemoveNode failed: " << result);

		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, effectNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphUpdate failed: " << result);

		// If the update failed, restore the previous node state
		result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, mOutputNode, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
			return false;
		}
	}

	if(nullptr != effectUnit1)
		*effectUnit1 = effectUnit;

	return true;
}

bool SFB::Audio::Player::RemoveEffect(AudioUnit effectUnit)
{
	if(nullptr == effectUnit)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Removing DSP effect: " << effectUnit);

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	AUNode effectNode = -1;
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = -1;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		// This is the unit to remove
		if(effectUnit == au) {
			effectNode = node;
			break;
		}
	}

	if(-1 == effectNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to find the AUNode for the specified AudioUnit");
		return false;
	}

	// Get the current input and output nodes for the node to delete
	UInt32 numInteractions = 0;
	result = AUGraphCountNodeInteractions(mAUGraph, effectNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [numInteractions];

	result = AUGraphGetNodeInteractions(mAUGraph, effectNode, &numInteractions, interactions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeInteractions failed: " << result);

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

	if(-1 == sourceNode || -1 == destNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to find the source or destination nodes");
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphRemoveNode(mAUGraph, effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphRemoveNode failed: " << result);
		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphUpdate failed: " << result);
		return false;
	}
	
	return true;
}

#if !TARGET_OS_IPHONE

#pragma mark Hog Mode

bool SFB::Audio::Player::OutputDeviceIsHogged() const
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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool SFB::Audio::Player::StartHoggingOutputDevice()
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Taking hog mode for device 0x" << std::hex << deviceID);

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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// The device is already hogged
	if(hogPID != (pid_t)-1) {
		LOGGER_INFO("org.sbooth.AudioEngine.Player", "Device is already hogged by pid: " << hogPID);
		return false;
	}

	bool restartIO = OutputIsRunning();
	if(restartIO)
		StopOutput();

	hogPID = getpid();

	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(restartIO && !OutputIsRunning())
		StartOutput();

	return true;
}

bool SFB::Audio::Player::StopHoggingOutputDevice()
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Releasing hog mode for device 0x" << std::hex << deviceID);

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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	if(restartIO && !OutputIsRunning())
		StartOutput();

	return true;
}

#pragma mark Device Parameters

bool SFB::Audio::Player::GetDeviceMasterVolume(Float32& volume) const
{
	return GetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool SFB::Audio::Player::SetDeviceMasterVolume(Float32 volume)
{
	return SetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool SFB::Audio::Player::GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const
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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::SetDeviceVolumeForChannel(UInt32 channel, Float32 volume)
{
	AudioDeviceID deviceID;
	if(!GetOutputDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting output device 0x" << std::hex << deviceID << " channel " << channel << " volume to " << volume);

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyVolumeScalar, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= channel 
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	OSStatus result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(volume), &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::GetDeviceChannelCount(UInt32& channelCount) const
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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectHasProperty (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) is false");
		return false;
	}

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);

	if(nullptr == bufferList) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Unable to allocate << " << dataSize << " bytes");
		return false;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, bufferList);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		free(bufferList), bufferList = nullptr;
		return false;
	}

	channelCount = 0;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;

	free(bufferList), bufferList = nullptr;
	return true;
}

bool SFB::Audio::Player::GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const
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
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed is false");
		return false;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &preferredChannels);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	preferredStereoChannels.first = preferredChannels[0];
	preferredStereoChannels.second = preferredChannels[1];

	return true;
}

#pragma mark Device Management

bool SFB::Audio::Player::CreateOutputDeviceUID(CFStringRef& deviceUID) const
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: " << result);
		return nullptr;
	}

	return true;
}

bool SFB::Audio::Player::SetOutputDeviceUID(CFStringRef deviceUID)
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
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << result);
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
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: " << result);
			return false;
		}
	}

	// The device isn't connected or doesn't exist
	if(kAudioDeviceUnknown == deviceID)
		return false;

	return SetOutputDeviceID(deviceID);
}

bool SFB::Audio::Player::GetOutputDeviceID(AudioDeviceID& deviceID) const
{
	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(deviceID);

	result = AudioUnitGetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::SetOutputDeviceID(AudioDeviceID deviceID)
{
	if(kAudioDeviceUnknown == deviceID)
		return false;

	AudioUnit au = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Update our output AU to use the specified device
	result = AudioUnitSetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, (UInt32)sizeof(deviceID));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::GetOutputDeviceSampleRate(Float64& sampleRate) const
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::Player::SetOutputDeviceSampleRate(Float64 sampleRate)
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	// Nothing to do
	if(currentSampleRate == sampleRate)
		return true;

	// Set the sample rate
	dataSize = sizeof(sampleRate);
	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(sampleRate), &sampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

#endif

#pragma mark Playlist Management

bool SFB::Audio::Player::Play(CFURLRef url)
{
	if(nullptr == url)
		return false;

	Decoder *decoder = Decoder::CreateDecoderForURL(url);

	if(nullptr == decoder)
		return false;

	bool success = Play(decoder);

	if(!success)
		delete decoder;

	return success;
}

bool SFB::Audio::Player::Play(Decoder *decoder)
{
	if(nullptr == decoder)
		return false;

	Stop();

	ClearQueuedDecoders();

	if(!Enqueue(decoder))
		return false;

	return Play();
}

bool SFB::Audio::Player::Enqueue(CFURLRef url)
{
	if(nullptr == url)
		return false;
	
	Decoder *decoder = Decoder::CreateDecoderForURL(url);
	
	if(nullptr == decoder)
		return false;
	
	bool success = Enqueue(decoder);
	
	if(!success)
		delete decoder;
	
	return success;
}

bool SFB::Audio::Player::Enqueue(Decoder *decoder)
{
	if(nullptr == decoder)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Enqueuing \"" << decoder->GetURL() << "\"");

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
	// In practice, the only time I've seen this happen is when using GuardMalloc, presumably because the
	// normal execution time of Enqueue() isn't sufficient to lead to this condition.
	Mutex::Tryer lock(mMutex);
	if(!lock)
		return false;

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

bool SFB::Audio::Player::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Skipping \"" << currentDecoderState->mDecoder->GetURL() << "\"");

	if(OutputIsRunning()) {
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagRequestMute */, &mFlags);

		mach_timespec_t renderTimeout = {
			.tv_sec = 0,
			.tv_nsec = NSEC_PER_SEC / 10
		};

		// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
		while(eAudioPlayerFlagRequestMute & mFlags)
			mSemaphore.TimedWait(renderTimeout);
	}
	else
		OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &currentDecoderState->mFlags);

	// Signal the decoding thread that decoding should stop (inner loop)
	mDecoderSemaphore.Signal();

	// Wait for decoding to finish or a SIGSEGV could occur if the collector collects an active decoder
	mach_timespec_t timeout = {
		.tv_sec = 0,
		.tv_nsec = NSEC_PER_SEC / 10
	};

	while(!(eDecoderStateDataFlagDecodingFinished & currentDecoderState->mFlags))
		mSemaphore.TimedWait(timeout);

	OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &currentDecoderState->mFlags);

	// Signal the decoding thread to start the next decoder (outer loop)
	mDecoderSemaphore.Signal();

	OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	return true;
}

bool SFB::Audio::Player::ClearQueuedDecoders()
{
	Mutex::Tryer lock(mMutex);
	if(!lock)
		return false;

	while(0 < CFArrayGetCount(mDecoderQueue)) {
		Decoder *decoder = static_cast<Decoder *>((void *)CFArrayGetValueAtIndex(mDecoderQueue, 0));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}

	return true;	
}

#pragma mark Ring Buffer Parameters

bool SFB::Audio::Player::SetRingBufferCapacity(uint32_t bufferCapacity)
{
	if(0 == bufferCapacity || mRingBufferWriteChunkSize > bufferCapacity)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting ring buffer capacity to " << bufferCapacity);

	return OSAtomicCompareAndSwap32Barrier((int32_t)mRingBufferCapacity, (int32_t)bufferCapacity, (int32_t *)&mRingBufferCapacity);
}

bool SFB::Audio::Player::SetRingBufferWriteChunkSize(uint32_t chunkSize)
{
	if(0 == chunkSize || mRingBufferCapacity < chunkSize)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting ring buffer write chunk size to " << chunkSize);

	return OSAtomicCompareAndSwap32Barrier((int32_t)mRingBufferWriteChunkSize, (int32_t)chunkSize, (int32_t *)&mRingBufferWriteChunkSize);
}

#pragma mark Callbacks

OSStatus SFB::Audio::Player::Render(AudioUnitRenderActionFlags		*ioActionFlags,
								const AudioTimeStamp			*inTimeStamp,
								UInt32							inBusNumber,
								UInt32							inNumberFrames,
								AudioBufferList					*ioData)
{

#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)

	assert(nullptr != ioActionFlags);
	assert(nullptr != ioData);

	size_t framesAvailableToRead = mRingBuffer->GetFramesAvailableToRead();

	// Output silence if muted or the ring buffer is empty
	if(eAudioPlayerFlagMuteOutput & mFlags || 0 == framesAvailableToRead) {
		*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;

		size_t byteCountToZero = inNumberFrames * sizeof(AudioUnitSampleType);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			memset(ioData->mBuffers[bufferIndex].mData, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
		}

		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min((UInt32)framesAvailableToRead, inNumberFrames);
	UInt32 framesRead = (UInt32)mRingBuffer->ReadAudio(ioData, framesToRead);
	if(framesRead != framesToRead) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "RingBuffer::ReadAudio failed: Requested " << framesToRead << " frames, got " << framesRead);
		return 1;
	}

	mFramesRenderedLastPass = framesRead;
	OSAtomicAdd64Barrier(framesRead, &mFramesRendered);

	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesRead != inNumberFrames) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Insufficient audio in ring buffer: " << framesRead << " frames available, " << inNumberFrames << " requested");
		
		UInt32 framesOfSilence = inNumberFrames - framesRead;
		size_t byteCountToZero = framesOfSilence * sizeof(AudioUnitSampleType);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			AudioUnitSampleType *bufferAlias = (AudioUnitSampleType *)ioData->mBuffers[bufferIndex].mData;
			memset(bufferAlias + framesRead, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize += byteCountToZero;
		}
	}

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();
	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
		mDecoderSemaphore.Signal();

	return noErr;
}

OSStatus SFB::Audio::Player::RenderNotify(AudioUnitRenderActionFlags		*ioActionFlags,
										  const AudioTimeStamp				*inTimeStamp,
										  UInt32							inBusNumber,
										  UInt32							inNumberFrames,
										  AudioBufferList					*ioData)
{
	
#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)
#pragma unused(inNumberFrames)
#pragma unused(ioData)

	// Pre-rendering actions
	if(kAudioUnitRenderAction_PreRender & (*ioActionFlags)) {

		// Call the pre-render block
		if(mRenderEventBlocks[0])
			mRenderEventBlocks[0](ioData, inNumberFrames);

		// Mute output if requested
		if(eAudioPlayerFlagRequestMute & mFlags) {
			OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
			OSAtomicTestAndClearBarrier(5 /* eAudioPlayerFlagRequestMute */, &mFlags);

			mSemaphore.Signal();
		}
	}
	// Post-rendering actions
	else if(kAudioUnitRenderAction_PostRender & (*ioActionFlags)) {

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

void * SFB::Audio::Player::DecoderThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Decoder");

	// ========================================
	// Make ourselves a high priority thread
	if(!setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Couldn't set decoder thread importance");

	mach_timespec_t timeout = {
		.tv_sec = 5,
		.tv_nsec = 0
	};

	while(mKeepDecoding) {

		int64_t decoderCounter = 0;

		DecoderStateData *decoderState = nullptr;
		{
			// ========================================
			// Lock the queue and remove the head element that contains the next decoder to use
			Decoder *decoder = nullptr;
			{
				Mutex::Tryer lock(mMutex);

				if(lock && 0 < CFArrayGetCount(mDecoderQueue)) {
					decoder = (Decoder *)CFArrayGetValueAtIndex(mDecoderQueue, 0);
					CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
				}
			}

			// ========================================
			// Open the decoder if necessary
			if(decoder && !decoder->IsOpen()) {
				CFErrorRef error = nullptr;
				if(!decoder->Open(&error))  {
					if(error) {
						LOGGER_ERR("org.sbooth.AudioEngine.Player", "Error opening decoder: " << error);
						CFRelease(error), error = nullptr;
					}

					// TODO: Perform CouldNotOpenDecoder() callback ??

					delete decoder, decoder = nullptr;
				}
			}

			// Create the decoder state
			if(decoder) {
				decoderState = new DecoderStateData(decoder);
				decoderState->mTimeStamp = decoderCounter++;
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
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mChannelsPerFrame != mRingBufferFormat.mChannelsPerFrame) {
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
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
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: " << result);

				if(!layoutsEqual) {
					LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Ring buffer channel layout (" << mRingBufferChannelLayout << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
					formatsMatch = false;
				}
			}
			else if((nullptr == nextChannelLayout || nullptr == mRingBufferChannelLayout) && nextChannelLayout != mRingBufferChannelLayout)
				formatsMatch = false;

			// If the formats don't match, the decoder can't be used with the current ring buffer format
			if(!formatsMatch) {
				// Ensure output is muted before performing operations that aren't thread safe
				if(OutputIsRunning()) {
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagFormatMismatch */, &mFlags);

					// Wait for the currently rendering decoder to finish
					mach_timespec_t renderTimeout = {
						.tv_sec = 0,
						.tv_nsec = NSEC_PER_SEC / 100
					};

					// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
					while(eAudioPlayerFlagFormatMismatch & mFlags)
						mSemaphore.TimedWait(renderTimeout);
				}

				if(mFormatMismatchBlock)
					mFormatMismatchBlock(mRingBufferFormat, nextFormat);

				// Adjust the formats
				{
					Mutex::Tryer lock(mMutex);
					if(lock)
						SetupAUGraphAndRingBufferForDecoder(decoderState->mDecoder);
					else
						delete decoderState, decoderState = nullptr;
				}

				// Clear the mute flag that was set in the rendering thread so output will resume
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
					LOGGER_WARNING("org.sbooth.AudioEngine.Player", "OSAtomicCompareAndSwapPtrBarrier() failed");
			}
		}
		
		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(decoderState) {
			Decoder *decoder = decoderState->mDecoder;

			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoding starting for \"" << decoder->GetURL() << "\"");
			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoder format: " << decoder->GetFormat());
			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoder channel layout: " << decoder->GetChannelLayout());

			AudioStreamBasicDescription decoderFormat = decoder->GetFormat();

			// ========================================
			// Create the AudioConverter which will convert from the decoder's format to the graph's format
			AudioConverterRef audioConverter = nullptr;
			OSStatus result = AudioConverterNew(&decoderFormat, &mRingBufferFormat, &audioConverter);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterNew failed: " << result);

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
				LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);
			
			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer			
			decoderState->AllocateBufferList(inputBufferSize / decoderFormat.mBytesPerFrame);

			AudioBufferList *bufferList = AllocateABL(mRingBufferFormat, mRingBufferWriteChunkSize);

			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(mKeepDecoding && decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags)) {

				// Fill the ring buffer with as much data as possible
				for(;;) {

					// Reset the ring buffer if required
					if(eAudioPlayerFlagRingBufferNeedsReset & mFlags) {

						OSAtomicTestAndClearBarrier(4 /* eAudioPlayerFlagRingBufferNeedsReset */, &mFlags);

						// Ensure output is muted before performing operations that aren't thread safe
						if(OutputIsRunning()) {
							OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagRequestMute */, &mFlags);

							mach_timespec_t renderTimeout = {
								.tv_sec = 0,
								.tv_nsec = NSEC_PER_SEC / 100
							};

							// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
							while(eAudioPlayerFlagRequestMute & mFlags)
								mSemaphore.TimedWait(renderTimeout);
						}
						else
							OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

						// Reset the converter to flush any buffers
						result = AudioConverterReset(audioConverter);
						if(noErr != result)
							LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterReset failed: " << result);

						// Reset() is not thread safe but the rendering thread is outputting silence
						mRingBuffer->Reset();

						// Clear the mute flag
						OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
					}

					// Determine how many frames are available in the ring buffer
					size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();

					// Force writes to the ring buffer to be at least mRingBufferWriteChunkSize
					if(mRingBufferWriteChunkSize <= framesAvailableToWrite) {

						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Seeking to frame " << decoderState->mFrameToSeek);

							// Ensure output is muted before performing operations that aren't thread safe
							if(OutputIsRunning()) {
								OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagRequestMute */, &mFlags);

								mach_timespec_t renderTimeout = {
									.tv_sec = 0,
									.tv_nsec = NSEC_PER_SEC / 100
								};

								// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
								while(eAudioPlayerFlagRequestMute & mFlags)
									mSemaphore.TimedWait(renderTimeout);
							}
							else
								OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);

							SInt64 newFrame = decoder->SeekToFrame(decoderState->mFrameToSeek);

							if(newFrame != decoderState->mFrameToSeek)
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "Error seeking to frame  " << decoderState->mFrameToSeek);
							
							// Update the seek request
							if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFrameToSeek, -1, &decoderState->mFrameToSeek))
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "OSAtomicCompareAndSwap64Barrier() failed ");
							
							// Update the counters accordingly
							if(-1 != newFrame) {
								if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFramesRendered, newFrame, &decoderState->mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.Player", "OSAtomicCompareAndSwap64Barrier() failed ");

								if(!OSAtomicCompareAndSwap64Barrier(mFramesDecoded, newFrame, &mFramesDecoded))
									LOGGER_ERR("org.sbooth.AudioEngine.Player", "OSAtomicCompareAndSwap64Barrier() failed ");

								if(!OSAtomicCompareAndSwap64Barrier(mFramesRendered, newFrame, &mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.Player", "OSAtomicCompareAndSwap64Barrier() failed ");

								// Reset the converter to flush any buffers
								result = AudioConverterReset(audioConverter);
								if(noErr != result)
									LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterReset failed: " << result);

								// Reset the ring buffer
								mRingBuffer->Reset();
							}

							// Clear the mute flag
							OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagMuteOutput */, &mFlags);
						}

						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to determine starting frame number ");
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
							LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterFillComplexBuffer failed: " << result);

						// Store the decoded audio
						if(0 != framesDecoded) {
							UInt32 framesWritten = (UInt32)mRingBuffer->WriteAudio(bufferList, framesDecoded);
							if(framesWritten != framesDecoded)
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "RingBuffer::Store failed");

							OSAtomicAdd64Barrier(framesWritten, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)*/) {
							LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoding finished for \"" << decoder->GetURL() << "\"");

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
				bufferList = DeallocateABL(bufferList);
			
			if(audioConverter) {
				result = AudioConverterDispose(audioConverter);
				if(noErr != result)
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterDispose failed: " << result);
				audioConverter = nullptr;
			}
		}

		// Wait for another thread to wake us, or for the timeout to happen
		mDecoderSemaphore.TimedWait(timeout);
	}

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoding thread terminating");

	return nullptr;
}

void * SFB::Audio::Player::CollectorThreadEntry()
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

			if(swapSucceeded) {
				LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Collecting decoder: \"" << decoderState->mDecoder->GetURL() << "\"");
				delete decoderState, decoderState = nullptr;
			}
		}
		
		// Wait for any thread to signal us to try and collect finished decoders
		mCollectorSemaphore.TimedWait(timeout);
	}
	
	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Collecting thread terminating");
	
	return nullptr;
}

#pragma mark AudioHardware Utilities

bool SFB::Audio::Player::OpenOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "OpenOutput");

	OSStatus result = NewAUGraph(&mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "NewAUGraph failed: " << result);
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, mMixerNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
		
		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);
		
		mAUGraph = nullptr;
		return false;
	}
	
	// Install the input callback
	AURenderCallbackStruct cbs = { myAURenderCallback, this };
	result = AUGraphSetNodeInputCallback(mAUGraph, mMixerNode, 0, &cbs);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphSetNodeInputCallback failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
	
	// Open the graph
	result = AUGraphOpen(mAUGraph);	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphOpen failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Set the mixer's volume on the input and output
	AudioUnit au = nullptr;
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
	
	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Output) failed: " << result);
	
	// Install the render notification
	result = AUGraphAddRenderNotify(mAUGraph, auGraphRenderNotify, this);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphAddRenderNotify failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, (UInt32)sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#else
	// Save the default value of kAudioUnitProperty_MaximumFramesPerSlice for use when performing sample rate conversion
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 dataSize = sizeof(mDefaultMaximumFramesPerSlice);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mDefaultMaximumFramesPerSlice, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#endif

	// Initialize the graph
	result = AUGraphInitialize(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphInitialize failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	return true;
}

bool SFB::Audio::Player::CloseOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "CloseOutput");

	Boolean graphIsRunning = false;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsRunning failed: " << result);
		return false;
	}

	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphStop failed: " << result);
			return false;
		}
	}

	Boolean graphIsInitialized = false;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsInitialized failed: " << result);
		return false;
	}

	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	result = AUGraphClose(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphClose failed: " << result);
		return false;
	}

	result = DisposeAUGraph(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "DisposeAUGraph failed: " << result);
		return false;
	}

	mAUGraph = nullptr;
	mMixerNode = -1;
	mOutputNode = -1;

	return true;
}

bool SFB::Audio::Player::StartOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "StartOutput");

	// We don't want to start output in the middle of a buffer modification
	Mutex::Tryer lock(mMutex);
	if(!lock)
		return false;

	OSStatus result = AUGraphStart(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphStart failed: " << result);
		return false;
	}
	
	return true;
}

bool SFB::Audio::Player::StopOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "StopOutput");

	OSStatus result = AUGraphStop(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphStop failed: " << result);
		return false;
	}
	
	return true;
}

bool SFB::Audio::Player::OutputIsRunning() const
{
	Boolean isRunning = false;
	OSStatus result = AUGraphIsRunning(mAUGraph, &isRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsRunning failed: " << result);
		return false;
	}

	return isRunning;
}

bool SFB::Audio::Player::ResetOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Resetting output");

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsRunning failed: " << result);
		return false;
	}

	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		result = AudioUnitReset(au, kAudioUnitScope_Global, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitReset failed: " << result);
			return false;
		}
	}

	return true;
}

#pragma mark AUGraph Utilities

bool SFB::Audio::Player::GetAUGraphLatency(Float64& latency) const
{
	latency = 0;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auLatency = 0;
		UInt32 dataSize = sizeof(auLatency);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &auLatency, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_Latency, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		latency += auLatency;
	}

	return true;
}

bool SFB::Audio::Player::GetAUGraphTailTime(Float64& tailTime) const
{
	tailTime = 0;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auTailTime = 0;
		UInt32 dataSize = sizeof(auTailTime);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &auTailTime, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_TailTime, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		tailTime += auTailTime;
	}

	return true;
}

bool SFB::Audio::Player::SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize)
{
	if(nullptr == propertyData || 0 >= propertyDataSize)
		return  false;

	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	// Iterate through the nodes and attempt to set the property
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);

		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNodeCount failed: " << result);
			return false;
		}

		if(mOutputNode == node) {
			// For AUHAL as the output node, you can't set the device side, so just set the client side
			result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, 0, propertyData, propertyDataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
				return false;
			}
		}
		else {
			UInt32 elementCount = 0;
			UInt32 dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &elementCount, &dataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Input) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Input, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, j, propertyData, propertyDataSize);				
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
					return false;
				}
			}

			elementCount = 0;
			dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &elementCount, &dataSize);			
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Output) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Output, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Output, j, propertyData, propertyDataSize);				
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
					return false;
				}
			}
		}
	}

	return true;
}

bool SFB::Audio::Player::SetAUGraphSampleRateAndChannelsPerFrame(Float64 sampleRate, UInt32 channelsPerFrame)
{
	// ========================================
	// If the graph is running, stop it
	Boolean graphIsRunning = FALSE;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsRunning failed: " << result);
		return false;
	}
	
	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphStop failed: " << result);
			return false;
		}
	}
	
	// ========================================
	// If the graph is initialized, uninitialize it
	Boolean graphIsInitialized = FALSE;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphIsInitialized failed: " << result);
		return false;
	}
	
	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	// ========================================
	// Save the interaction information and then clear all the connections
	UInt32 interactionCount = 0;
	result = AUGraphGetNumberOfInteractions(mAUGraph, &interactionCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetNumberOfInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [interactionCount];

	for(UInt32 i = 0; i < interactionCount; ++i) {
		result = AUGraphGetInteractionInfo(mAUGraph, i, &interactions[i]);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphGetInteractionInfo failed: " << result);
			return false;
		}
	}

	result = AUGraphClearConnections(mAUGraph);	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphClearConnections failed: " << result);
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
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to restore AUGraph format: " << result);
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
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphConnectNodeInput failed: " << result);
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
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphSetNodeInputCallback failed: " << result);
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 inputSampleRate = 0;
	UInt32 dataSize = sizeof(inputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0, &inputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	Float64 outputSampleRate = 0;
	dataSize = sizeof(outputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &outputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Output) failed: " << result);
		return false;
	}

	UInt32 newMaxFrames = mDefaultMaximumFramesPerSlice;

	// If the output unit's input and output sample rates don't match, calculate a working maximum number of frames per slice
	if(inputSampleRate != outputSampleRate) {
		LOGGER_INFO("org.sbooth.AudioEngine.Player", "Input sample rate (" << inputSampleRate << ") and output sample rate (" << outputSampleRate << ") don't match");
		
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);
		return false;
	}

	// Adjust the maximum frames per slice if necessary
	if(newMaxFrames != currentMaxFrames) {
		LOGGER_INFO("org.sbooth.AudioEngine.Player", "Adjusting kAudioUnitProperty_MaximumFramesPerSlice to " << newMaxFrames);

		if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_MaximumFramesPerSlice, &newMaxFrames, sizeof(newMaxFrames))) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "SetPropertyOnAUGraphNodes (kAudioUnitProperty_MaximumFramesPerSlice) failed");
			return false;
		}
	}
#endif

	// If the graph was initialized, reinitialize it
	if(graphIsInitialized) {
		result = AUGraphInitialize(mAUGraph);		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphInitialize failed: " << result);
			return false;
		}
	}

	// If the graph was running, restart it
	if(graphIsRunning) {
		result = AUGraphStart(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphStart failed: " << result);
			return false;
		}
	}

	return true;
}

bool SFB::Audio::Player::SetOutputUnitChannelMap(AudioChannelLayout *channelLayout)
{
#if !TARGET_OS_IPHONE
	AudioUnit outputUnit = nullptr;
	OSStatus result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &outputUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Clear the existing channel map
	result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, nullptr, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	if(nullptr == channelLayout)
		return true;

	AudioChannelLayout stereoChannelLayout = {
		.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo
	};

	AudioChannelLayout *layouts [] = {
		channelLayout,
		&stereoChannelLayout
	};

	UInt32 channelLayoutIsStereo = false;
	UInt32 propertySize = sizeof(channelLayoutIsStereo);
	result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), (void *)layouts, &propertySize, &channelLayoutIsStereo);

	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: " << result);

	// Stereo
	if(channelLayoutIsStereo) {
		UInt32 preferredChannelsForStereo [2];
		UInt32 preferredChannelsForStereoSize = sizeof(preferredChannelsForStereo);
		result = AudioUnitGetProperty(outputUnit, kAudioDevicePropertyPreferredChannelsForStereo, kAudioUnitScope_Output, 0, preferredChannelsForStereo, &preferredChannelsForStereoSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioDevicePropertyPreferredChannelsForStereo) failed: " << result);
			return false;
		}

		// Build a channel map using the preferred stereo channels
		AudioStreamBasicDescription outputFormat;
		propertySize = sizeof(outputFormat);
		result = AudioUnitGetProperty(outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outputFormat, &propertySize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output) failed: " << result);
			return false;
		}

		SInt32 channelMap [ outputFormat.mChannelsPerFrame ];
		for(UInt32 i = 0; i <  outputFormat.mChannelsPerFrame; ++i)
			channelMap[i] = -1;

		// TODO: Verify the following statement to be true
		// preferredChannelsForStereo uses 1-based indices
		channelMap[preferredChannelsForStereo[0] - 1] = 0;
		channelMap[preferredChannelsForStereo[1] - 1] = 1;

		LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Using  stereo channel map: ");
		for(UInt32 i = 0; i < outputFormat.mChannelsPerFrame; ++i)
			LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "  " << i << " -> " << channelMap[i]);

		// Set the channel map
		result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, channelMap, (UInt32)sizeof(channelMap));
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
			return false;
		}
	}
	// Multichannel or other non-stereo audio
	else {
		// Use the device's preferred channel layout
		UInt32 devicePreferredChannelLayoutSize = 0;
		result = AudioUnitGetPropertyInfo(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, &devicePreferredChannelLayoutSize, nullptr);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetPropertyInfo (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);
			return false;
		}

		AudioChannelLayout *devicePreferredChannelLayout = (AudioChannelLayout *)malloc(devicePreferredChannelLayoutSize);

		result = AudioUnitGetProperty(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, devicePreferredChannelLayout, &devicePreferredChannelLayoutSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitGetProperty (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);

			if(devicePreferredChannelLayout)
				free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

			return false;
		}

		UInt32 channelCount = 0;
		UInt32 dataSize = sizeof(channelCount);
		result = AudioFormatGetProperty(kAudioFormatProperty_NumberOfChannelsForLayout, devicePreferredChannelLayoutSize, devicePreferredChannelLayout, &dataSize, &channelCount);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: " << result);

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
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);
			return false;
		}

		LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Using multichannel channel map: ");
		for(UInt32 i = 0; i < channelCount; ++i)
			LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "  " << i << " -> " << channelMap[i]);

		// Set the channel map
		result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, channelMap, (UInt32)sizeof(channelMap));
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
			return false;
		}
	}
#endif

	return true;
}

#pragma mark Other Utilities

SFB::Audio::Player::DecoderStateData * SFB::Audio::Player::GetCurrentDecoderState() const
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

SFB::Audio::Player::DecoderStateData * SFB::Audio::Player::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const
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

void SFB::Audio::Player::StopActiveDecoders()
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

bool SFB::Audio::Player::SetupAUGraphAndRingBufferForDecoder(Decoder *decoder)
{
	if(nullptr == decoder)
		return false;

	// Open the decoder if necessary
	CFErrorRef error = nullptr;
	if(!decoder->IsOpen() && !decoder->Open(&error)) {
		if(error) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "Error opening decoder: " << error);
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
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to set output unit channel map");

	// The decoder's channel layout becomes the ring buffer's channel layout
	if(mRingBufferChannelLayout)
		free(mRingBufferChannelLayout), mRingBufferChannelLayout = nullptr;

	mRingBufferChannelLayout = CopyChannelLayout(channelLayout);

	// Allocate enough space in the ring buffer for the new format
	if(!mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame, mRingBufferFormat.mBytesPerFrame, mRingBufferCapacity)) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to allocate ring buffer");
		return false;
	}

	return true;
}
