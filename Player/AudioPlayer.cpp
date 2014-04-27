/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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
#include "CoreAudioOutput.h"
#include "AudioBufferList.h"
#include "Logger.h"

// ========================================
// Macros
// ========================================
#define RING_BUFFER_CAPACITY_FRAMES				16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6

namespace {

	// ========================================
	// Turn off logging by default
	void InitializeLoggingSubsystem() __attribute__ ((constructor));
	void InitializeLoggingSubsystem()
	{
		::SFB::Logger::SetCurrentLevel(::SFB::Logger::disabled);
	}

	// Bit reversal lookup table from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
	static const uint8_t sBitReverseTable256 [256] =
	{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
		R6(0), R6(2), R6(1), R6(3)
	};

	// ========================================
	// Enums
	// ========================================
	enum eDecoderStateDataFlags : unsigned int {
		eDecoderStateDataFlagDecodingStarted	= 1u << 0,
		eDecoderStateDataFlagDecodingFinished	= 1u << 1,
		eDecoderStateDataFlagRenderingStarted	= 1u << 2,
		eDecoderStateDataFlagRenderingFinished	= 1u << 3,
		eDecoderStateDataFlagStopDecoding		= 1u << 4
	};

	enum eAudioPlayerFlags : unsigned int {
		eAudioPlayerFlagMuteOutput				= 1u << 0,
		eAudioPlayerFlagFormatMismatch			= 1u << 1,
		eAudioPlayerFlagRequestMute				= 1u << 2,
		eAudioPlayerFlagRingBufferNeedsReset	= 1u << 3,
		eAudioPlayerFlagStartPlayback			= 1u << 4,

		eAudioPlayerFlagStopDecoding			= 1u << 10,
		eAudioPlayerFlagStopCollecting			= 1u << 11
	};
	
}


// ========================================
// State data for decoders that are decoding and/or rendering
// ========================================
class SFB::Audio::Player::DecoderStateData
{

public:

	DecoderStateData(std::unique_ptr<Decoder> decoder)
		: DecoderStateData()
	{
		assert(nullptr != decoder);

		mDecoder = std::move(decoder);

		// NB: The decoder may return an estimate of the total frames
		mTotalFrames = mDecoder->GetTotalFrames();
	}

	DecoderStateData(const DecoderStateData& rhs) = delete;
	DecoderStateData& operator=(const DecoderStateData& rhs) = delete;

	bool AllocateBufferList(UInt32 capacityFrames)
	{
		return mBufferList.Allocate(mDecoder->GetFormat(), capacityFrames);
	}

	UInt32 ReadAudio(UInt32 frameCount)
	{
		mBufferList.Reset();
		return mDecoder->ReadAudio(mBufferList, std::min(frameCount, mBufferList.GetCapacityFrames()));
	}

	std::unique_ptr<Decoder>	mDecoder;

	BufferList					mBufferList;

	SInt64						mTimeStamp;

	SInt64						mTotalFrames;

	std::atomic_llong			mFramesRendered;
	std::atomic_llong			mFrameToSeek;

	std::atomic_uint			mFlags;

private:

	DecoderStateData()
		: mDecoder(nullptr), mTimeStamp(0), mTotalFrames(0), mFramesRendered(ATOMIC_VAR_INIT(0)), mFrameToSeek(ATOMIC_VAR_INIT(-1)), mFlags(ATOMIC_VAR_INIT(0))
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
}

namespace {

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
	: mFlags(ATOMIC_VAR_INIT(0)), mRingBuffer(new RingBuffer), mRingBufferCapacity(ATOMIC_VAR_INIT(RING_BUFFER_CAPACITY_FRAMES)), mRingBufferWriteChunkSize(ATOMIC_VAR_INIT(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES)), mFramesDecoded(ATOMIC_VAR_INIT(0)), mFramesRendered(ATOMIC_VAR_INIT(0)), mFormatMismatchBlock(nullptr), mOutput(new CoreAudioOutput)
{
	memset(&mDecoderEventBlocks, 0, sizeof(mDecoderEventBlocks));
	memset(&mRenderEventBlocks, 0, sizeof(mRenderEventBlocks));

	// ========================================
	// Initialize the decoder array
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex)
		mActiveDecoders[bufferIndex].store(nullptr, std::memory_order_relaxed);

	// ========================================
	// Launch the decoding thread
	try {
		mDecoderThread = std::thread(&Player::DecoderThreadEntry, this);
	}

	catch(const std::exception& e) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "Unable to create decoder thread: " << e.what());

		throw;
	}

	// ========================================
	// Launch the collector thread
	try {
		mCollectorThread = std::thread(&Player::CollectorThreadEntry, this);
	}

	catch(const std::exception& e) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "Unable to create collector thread: " << e.what());

		mFlags.fetch_or(eAudioPlayerFlagStopDecoding, std::memory_order_relaxed);
		mDecoderSemaphore.Signal();

		try {
			mDecoderThread.join();
		}

		catch(const std::exception& e) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to join decoder thread: " << e.what());
		}

		throw;
	}

	// ========================================
	// Set up output
	mOutput->SetPlayer(this);
	if(!mOutput->Open()) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

SFB::Audio::Player::~Player()
{
	Stop();

	// Stop the processing graph and reclaim its resources
	if(!mOutput->Close())
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "CloseOutput() failed");

	// End the decoding thread
	mFlags.fetch_or(eAudioPlayerFlagStopDecoding, std::memory_order_relaxed);
	mDecoderSemaphore.Signal();

	try {
		mDecoderThread.join();
	}

	catch(const std::exception& e) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to join decoder thread: " << e.what());
	}

	// End the collector thread
	mFlags.fetch_or(eAudioPlayerFlagStopCollecting, std::memory_order_relaxed);
	mCollectorSemaphore.Signal();

	try {
		mCollectorThread.join();
	}

	catch(const std::exception& e) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to join collector thread: " << e.what());
	}

	// Force any decoders left hanging by the collector to end
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		if(nullptr != mActiveDecoders[bufferIndex])
			delete mActiveDecoders[bufferIndex].exchange(nullptr, std::memory_order_relaxed);
	}

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
	if(mOutput->IsRunning())
		return true;

	// We don't want to start output in the middle of a buffer modification
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	return mOutput->Start();
}

bool SFB::Audio::Player::Pause()
{
	if(mOutput->IsRunning())
		return mOutput->Stop();

	return true;
}

bool SFB::Audio::Player::Stop()
{
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	if(mOutput->IsRunning())
		mOutput->Stop();

	StopActiveDecoders();

	if(!mOutput->Reset())
		return false;

	// Reset the ring buffer
	mFramesDecoded.store(0, std::memory_order_relaxed);
	mFramesRendered.store(0, std::memory_order_relaxed);

	mFlags.fetch_or(eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

	return true;
}

SFB::Audio::Player::PlayerState SFB::Audio::Player::GetPlayerState() const
{
	if(mOutput->IsRunning())
		return PlayerState::Playing;

	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return PlayerState::Stopped;

	auto flags = currentDecoderState->mFlags.load(std::memory_order_relaxed);

	if(eDecoderStateDataFlagRenderingStarted & flags)
		return PlayerState::Paused;

	if(eDecoderStateDataFlagDecodingStarted & flags)
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

	SInt64 frameToSeek		= currentDecoderState->mFrameToSeek.load(std::memory_order_relaxed);
	SInt64 framesRendered	= currentDecoderState->mFramesRendered.load(std::memory_order_relaxed);

	currentFrame	= (-1 == frameToSeek ? framesRendered : frameToSeek);
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

	SInt64 frameToSeek		= currentDecoderState->mFrameToSeek.load(std::memory_order_relaxed);
	SInt64 framesRendered	= currentDecoderState->mFramesRendered.load(std::memory_order_relaxed);

	SInt64 currentFrame		= (-1 == frameToSeek ? framesRendered : frameToSeek);
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

	SInt64 frameToSeek		= currentDecoderState->mFrameToSeek.load(std::memory_order_relaxed);
	SInt64 framesRendered	= currentDecoderState->mFramesRendered.load(std::memory_order_relaxed);

	currentFrame		= (-1 == frameToSeek ? framesRendered : frameToSeek);
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

	SInt64 frameToSeek		= currentDecoderState->mFrameToSeek.load(std::memory_order_relaxed);
	SInt64 framesRendered	= currentDecoderState->mFramesRendered.load(std::memory_order_relaxed);

	SInt64 currentFrame		= (-1 == frameToSeek ? framesRendered : frameToSeek);
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

	SInt64 frameToSeek		= currentDecoderState->mFrameToSeek.load(std::memory_order_relaxed);
	SInt64 framesRendered	= currentDecoderState->mFramesRendered.load(std::memory_order_relaxed);

	SInt64 currentFrame		= (-1 == frameToSeek ? framesRendered : frameToSeek);
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

	currentDecoderState->mFrameToSeek.store(frame, std::memory_order_relaxed);

	// Force a flush of the ring buffer to prevent audible seek artifacts
	if(!mOutput->IsRunning())
		mFlags.fetch_or(eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

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

#pragma mark Playlist Management

bool SFB::Audio::Player::Play(CFURLRef url)
{
	if(nullptr == url)
		return false;

	auto decoder = Decoder::CreateDecoderForURL(url);
	return Play(decoder);
}

bool SFB::Audio::Player::Play(Decoder::unique_ptr& decoder)
{
	if(!decoder)
		return false;

	if(!ClearQueuedDecoders())
		return false;

	if(!Stop())
		return false;

	if(!Enqueue(decoder))
		return false;

	// Start playback once decoding has begun
	mFlags.fetch_or(eAudioPlayerFlagStartPlayback, std::memory_order_relaxed);

	mDecoderSemaphore.Signal();

	return true;
}

bool SFB::Audio::Player::Enqueue(CFURLRef url)
{
	if(nullptr == url)
		return false;

	auto decoder = Decoder::CreateDecoderForURL(url);
	return Enqueue(decoder);
}

bool SFB::Audio::Player::Enqueue(Decoder::unique_ptr& decoder)
{
	if(!decoder)
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
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	// If there are no decoders in the queue, set up for playback
	if(nullptr == GetCurrentDecoderState() && !mDecoderQueue.empty()) {
		if(!SetupOutputAndRingBufferForDecoder(*decoder))
			return false;
	}

	// Take ownership of the decoder and add it to the queue
	mDecoderQueue.push_back(std::move(decoder));

	mDecoderSemaphore.Signal();

	return true;
}

bool SFB::Audio::Player::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Skipping \"" << currentDecoderState->mDecoder->GetURL() << "\"");

	if(mOutput->IsRunning()) {
		mFlags.fetch_or(eAudioPlayerFlagRequestMute, std::memory_order_relaxed);

		mach_timespec_t renderTimeout = {
			.tv_sec = 0,
			.tv_nsec = NSEC_PER_SEC / 10
		};

		// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
		while(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed))
			mSemaphore.TimedWait(renderTimeout);
	}
	else
		mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);

	currentDecoderState->mFlags.fetch_or(eDecoderStateDataFlagStopDecoding, std::memory_order_relaxed);

	// Signal the decoding thread that decoding should stop (inner loop)
	mDecoderSemaphore.Signal();

	// Wait for decoding to finish or a SIGSEGV could occur if the collector collects an active decoder
	mach_timespec_t timeout = {
		.tv_sec = 0,
		.tv_nsec = NSEC_PER_SEC / 10
	};

	while(!(eDecoderStateDataFlagDecodingFinished & currentDecoderState->mFlags.load(std::memory_order_relaxed)))
		mSemaphore.TimedWait(timeout);

	currentDecoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);

	// Signal the decoding thread to start the next decoder (outer loop)
	mDecoderSemaphore.Signal();

	mFlags.fetch_and(~eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);

	return true;
}

bool SFB::Audio::Player::ClearQueuedDecoders()
{
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	mDecoderQueue.clear();

	return true;
}

#pragma mark Ring Buffer Parameters

bool SFB::Audio::Player::SetRingBufferCapacity(uint32_t bufferCapacity)
{
	if(0 == bufferCapacity || mRingBufferWriteChunkSize > bufferCapacity)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting ring buffer capacity to " << bufferCapacity);

	mRingBufferCapacity.store(bufferCapacity, std::memory_order_relaxed);
	return true;
}

bool SFB::Audio::Player::SetRingBufferWriteChunkSize(uint32_t chunkSize)
{
	if(0 == chunkSize || mRingBufferCapacity < chunkSize)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Player", "Setting ring buffer write chunk size to " << chunkSize);

	mRingBufferWriteChunkSize.store(chunkSize, std::memory_order_relaxed);
	return true;
}

#pragma mark Callbacks

//OSStatus SFB::Audio::Player::Render(AudioUnitRenderActionFlags		*ioActionFlags,
//									const AudioTimeStamp			*inTimeStamp,
//									UInt32							inBusNumber,
//									UInt32							inNumberFrames,
//									AudioBufferList					*ioData)
//{
//
//#pragma unused(inTimeStamp)
//#pragma unused(inBusNumber)
//
//	assert(nullptr != ioActionFlags);
//	assert(nullptr != ioData);
//
//	size_t framesAvailableToRead = mRingBuffer->GetFramesAvailableToRead();
//
//	// Output silence if muted or the ring buffer is empty
//	if(eAudioPlayerFlagMuteOutput & mFlags || 0 == framesAvailableToRead) {
//		*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
//
//		size_t byteCountToZero = inNumberFrames * sizeof(AudioUnitSampleType);
//		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
//			memset(ioData->mBuffers[bufferIndex].mData, 0, byteCountToZero);
//			ioData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
//		}
//
//		return noErr;
//	}
//
//	// Restrict reads to valid decoded audio
//	UInt32 framesToRead = std::min((UInt32)framesAvailableToRead, inNumberFrames);
//	UInt32 framesRead = (UInt32)mRingBuffer->ReadAudio(ioData, framesToRead);
//	if(framesRead != framesToRead) {
//		LOGGER_ERR("org.sbooth.AudioEngine.Player", "RingBuffer::ReadAudio failed: Requested " << framesToRead << " frames, got " << framesRead);
//		return 1;
//	}
//
//	mFramesRenderedLastPass = framesRead;
//	mFramesRendered.fetch_add(framesRead, std::memory_order_relaxed);
//
//	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
//	if(framesRead != inNumberFrames) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Insufficient audio in ring buffer: " << framesRead << " frames available, " << inNumberFrames << " requested");
//
//		UInt32 framesOfSilence = inNumberFrames - framesRead;
//		size_t byteCountToZero = framesOfSilence * sizeof(AudioUnitSampleType);
//		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
//			AudioUnitSampleType *bufferAlias = (AudioUnitSampleType *)ioData->mBuffers[bufferIndex].mData;
//			memset(bufferAlias + framesRead, 0, byteCountToZero);
//			ioData->mBuffers[bufferIndex].mDataByteSize += byteCountToZero;
//		}
//	}
//
//	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
//	size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();
//	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
//		mDecoderSemaphore.Signal();
//
//	return noErr;
//}
//
//OSStatus SFB::Audio::Player::RenderNotify(AudioUnitRenderActionFlags		*ioActionFlags,
//										  const AudioTimeStamp				*inTimeStamp,
//										  UInt32							inBusNumber,
//										  UInt32							inNumberFrames,
//										  AudioBufferList					*ioData)
//{
//
//#pragma unused(inTimeStamp)
//#pragma unused(inBusNumber)
//#pragma unused(inNumberFrames)
//#pragma unused(ioData)
//
//	// Pre-rendering actions
//	if(kAudioUnitRenderAction_PreRender & (*ioActionFlags)) {
//
//		// Call the pre-render block
//		if(mRenderEventBlocks[0])
//			mRenderEventBlocks[0](ioData, inNumberFrames);
//
//		// Mute output if requested
//		if(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed)) {
//			mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
//			mFlags.fetch_and(~eAudioPlayerFlagRequestMute, std::memory_order_relaxed);
//
//			mSemaphore.Signal();
//		}
//	}
//	// Post-rendering actions
//	else if(kAudioUnitRenderAction_PostRender & (*ioActionFlags)) {
//
//		// Call the post-render block
//		if(mRenderEventBlocks[1])
//			mRenderEventBlocks[1](ioData, inNumberFrames);
//
//		// There is nothing more to do if no frames were rendered
//		if(0 == mFramesRenderedLastPass)
//			return noErr;
//
//		// mFramesRenderedLastPass contains the number of valid frames that were rendered
//		// However, these could have come from any number of decoders depending on the buffer sizes
//		// So it is necessary to split them up here
//
//		SInt64 framesRemainingToDistribute = mFramesRenderedLastPass;
//		DecoderStateData *decoderState = GetCurrentDecoderState();
//
//		// mActiveDecoders is not an ordered array, so to ensure that callbacks are performed
//		// in the proper order multiple passes are made here
//		while(nullptr != decoderState) {
//			SInt64 timeStamp = decoderState->mTimeStamp;
//
//			SInt64 decoderFramesRemaining = (-1 == decoderState->mTotalFrames ? mFramesRenderedLastPass : decoderState->mTotalFrames - decoderState->mFramesRendered);
//			SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, (SInt64)mFramesRenderedLastPass);
//
//			if(0 == decoderState->mFramesRendered && !(eDecoderStateDataFlagRenderingStarted & decoderState->mFlags.load(std::memory_order_relaxed))) {
//				// Call the rendering started block
//				if(mDecoderEventBlocks[2])
//					mDecoderEventBlocks[2](*decoderState->mDecoder);
//				decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingStarted, std::memory_order_relaxed);
//			}
//
//			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder, std::memory_order_relaxed);
//
//			if((eDecoderStateDataFlagDecodingFinished & decoderState->mFlags.load(std::memory_order_relaxed)) && decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags.load(std::memory_order_relaxed))*/) {
//				// Call the rendering finished block
//				if(mDecoderEventBlocks[3])
//					mDecoderEventBlocks[3](*decoderState->mDecoder);
//
//				decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);
//				decoderState = nullptr;
//
//				// Since rendering is finished, signal the collector to clean up this decoder
//				mCollectorSemaphore.Signal();
//			}
//
//			framesRemainingToDistribute -= framesFromThisDecoder;
//
//			if(0 == framesRemainingToDistribute)
//				break;
//
//			decoderState = GetDecoderStateStartingAfterTimeStamp(timeStamp);
//		}
//
//		if(mFramesDecoded == mFramesRendered && nullptr == GetCurrentDecoderState()) {
//			// Signal the decoding thread that it is safe to manipulate the ring buffer
//			if(eAudioPlayerFlagFormatMismatch & mFlags) {
//				mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
//				mFlags.fetch_and(~eAudioPlayerFlagFormatMismatch, std::memory_order_relaxed);
//				mSemaphore.Signal();
//			}
//			else
//				StopOutput();
//		}
//	}
//
//	return noErr;
//}

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

	while(!(eAudioPlayerFlagStopDecoding & mFlags.load(std::memory_order_relaxed))) {

		int64_t decoderCounter = 0;

		DecoderStateData *decoderState = nullptr;
		{
			// ========================================
			// Lock the queue and remove the head element that contains the next decoder to use
			std::unique_ptr<Decoder> decoder;
			{
				std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);

				if(lock && !mDecoderQueue.empty()) {
					auto iter = std::begin(mDecoderQueue);
					decoder = std::move(*iter);
					mDecoderQueue.erase(iter);
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
				}
			}

			// Create the decoder state
			if(decoder) {
				decoderState = new DecoderStateData(std::move(decoder));
				decoderState->mTimeStamp = decoderCounter++;
			}
		}

		// ========================================
		// Ensure the decoder's format is compatible with the ring buffer
		if(decoderState) {
			const AudioFormat&		nextFormat			= decoderState->mDecoder->GetFormat();
			const ChannelLayout&	nextChannelLayout	= decoderState->mDecoder->GetChannelLayout();

			// The two files can be joined seamlessly only if they have the same formats, sample rates, and channel counts
			bool formatsMatch = true;

			if(nextFormat.mFormatID != mOutput->GetFormat().mFormatID) {
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Output format (" << mOutput->GetFormat().mFormatID << ") and decoder format (" << nextFormat.mFormatID << ") don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mSampleRate != mOutput->GetFormat().mSampleRate) {
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Output sample rate (" << mOutput->GetFormat().mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mChannelsPerFrame != mOutput->GetFormat().mChannelsPerFrame) {
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Output channel count (" << mOutput->GetFormat().mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
				formatsMatch = false;
			}

			// Enqueue the decoder if its channel layout matches the ring buffer's channel layout (so the channel map in the output AU will remain valid)
			if(nextChannelLayout != mOutput->GetChannelLayout()) {
				LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Gapless join failed: Output channel layout (" << mOutput->GetChannelLayout() << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
				formatsMatch = false;
			}

			// If the formats don't match, the decoder can't be used with the current ring buffer format
			if(!formatsMatch) {
				// Ensure output is muted before performing operations that aren't thread safe
				if(mOutput->IsRunning()) {
					mFlags.fetch_or(eAudioPlayerFlagFormatMismatch, std::memory_order_relaxed);

					// Wait for the currently rendering decoder to finish
					mach_timespec_t renderTimeout = {
						.tv_sec = 0,
						.tv_nsec = NSEC_PER_SEC / 100
					};

					// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
					while(eAudioPlayerFlagFormatMismatch & mFlags.load(std::memory_order_relaxed))
						mSemaphore.TimedWait(renderTimeout);
				}

				if(mFormatMismatchBlock)
					mFormatMismatchBlock(mOutput->GetFormat(), nextFormat);

				// Adjust the formats
				{
					std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
					if(!lock || !SetupOutputAndRingBufferForDecoder(*decoderState->mDecoder))
						delete decoderState, decoderState = nullptr;
				}

				// Clear the mute flag that was set in the rendering thread so output will resume
				mFlags.fetch_and(~eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
			}
		}

		// ========================================
		// Append the decoder state to the list of active decoders
		if(decoderState) {
			for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
				auto current = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

				if(nullptr != current)
					continue;

				if(mActiveDecoders[bufferIndex].compare_exchange_strong(current, decoderState))
					break;
				else
					LOGGER_WARNING("org.sbooth.AudioEngine.Player", "compare_exchange_strong() failed");
			}
		}

		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(decoderState) {
			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoding starting for \"" << decoderState->mDecoder->GetURL() << "\"");
			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoder format: " << decoderState->mDecoder->GetFormat());
			LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoder channel layout: " << decoderState->mDecoder->GetChannelLayout());

			const AudioFormat& decoderFormat = decoderState->mDecoder->GetFormat();

			// ========================================
			// Create the AudioConverter which will convert from the decoder's format to the output format (for PCM output)
			AudioConverterRef audioConverter = nullptr;
			BufferList bufferList;
			if(mOutput->GetFormat().IsPCM()) {
				auto outputFormat = mOutput->GetFormat();
				OSStatus result = AudioConverterNew(&decoderFormat, &outputFormat, &audioConverter);
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterNew failed: " << result);

					decoderState->mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished | eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);

					decoderState = nullptr;

					// If this happens, output will be impossible
					mCollectorSemaphore.Signal();
					
					continue;
				}

				// ========================================
				// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
				UInt32 inputBufferSize = mRingBufferWriteChunkSize * mOutput->GetFormat().mBytesPerFrame;
				UInt32 dataSize = sizeof(inputBufferSize);
				result = AudioConverterGetProperty(audioConverter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &inputBufferSize);
				if(noErr != result)
					LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);

				// ========================================
				// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
				decoderState->AllocateBufferList((UInt32)decoderFormat.ByteCountToFrameCount(inputBufferSize));
				bufferList.Allocate(mOutput->GetFormat(), mRingBufferWriteChunkSize);
			}
			else if(mOutput->GetFormat().IsDSD()) {
				UInt32 preferredSize = (UInt32)mOutput->GetPreferredBufferSize();
				decoderState->AllocateBufferList(preferredSize ?: 512);
			}


			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(!(eAudioPlayerFlagStopDecoding & mFlags.load(std::memory_order_relaxed)) && decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags.load(std::memory_order_relaxed))) {

				// Fill the ring buffer with as much data as possible
				for(;;) {

					// Reset the ring buffer if required
					if(eAudioPlayerFlagRingBufferNeedsReset & mFlags.load(std::memory_order_relaxed)) {

						mFlags.fetch_and(~eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

						// Ensure output is muted before performing operations that aren't thread safe
						if(mOutput->IsRunning()) {
							mFlags.fetch_or(eAudioPlayerFlagRequestMute, std::memory_order_relaxed);

							mach_timespec_t renderTimeout = {
								.tv_sec = 0,
								.tv_nsec = NSEC_PER_SEC / 100
							};

							// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
							while(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed))
								mSemaphore.TimedWait(renderTimeout);
						}
						else
							mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);

						// Reset the converter to flush any buffers
						if(audioConverter) {
							auto result = AudioConverterReset(audioConverter);
							if(noErr != result)
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterReset failed: " << result);
						}

						// Reset() is not thread safe but the rendering thread is outputting silence
						mRingBuffer->Reset();

						// Clear the mute flag
						mFlags.fetch_and(~eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
					}

					// Determine how many frames are available in the ring buffer
					size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();

					// Force writes to the ring buffer to be at least mRingBufferWriteChunkSize
					if(mRingBufferWriteChunkSize <= framesAvailableToWrite) {

						SInt64 frameToSeek = decoderState->mFrameToSeek.load(std::memory_order_relaxed);

						// Seek to the specified frame
						if(-1 != frameToSeek) {
							LOGGER_DEBUG("org.sbooth.AudioEngine.Player", "Seeking to frame " << frameToSeek);

							// Ensure output is muted before performing operations that aren't thread safe
							if(mOutput->IsRunning()) {
								mFlags.fetch_or(eAudioPlayerFlagRequestMute, std::memory_order_relaxed);

								mach_timespec_t renderTimeout = {
									.tv_sec = 0,
									.tv_nsec = NSEC_PER_SEC / 100
								};

								// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
								while(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed))
									mSemaphore.TimedWait(renderTimeout);
							}
							else
								mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);

							SInt64 newFrame = decoderState->mDecoder->SeekToFrame(frameToSeek);

							if(newFrame != frameToSeek)
								LOGGER_NOTICE("org.sbooth.AudioEngine.Player", "Inaccurate seek to frame  " << frameToSeek << ", got frame " << newFrame);

							// Update the seek request
							decoderState->mFrameToSeek.store(-1, std::memory_order_relaxed);

							// Update the counters accordingly
							if(-1 != newFrame) {
								decoderState->mFramesRendered.store(newFrame, std::memory_order_relaxed);
								mFramesDecoded.store(newFrame, std::memory_order_relaxed);
								mFramesRendered.store(newFrame, std::memory_order_relaxed);

								// Reset the converter to flush any buffers
								if(audioConverter) {
									auto result = AudioConverterReset(audioConverter);
									if(noErr != result)
										LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterReset failed: " << result);
								}

								// Reset the ring buffer
								mRingBuffer->Reset();
							}

							// Clear the mute flag
							mFlags.fetch_and(~eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
						}

						SInt64 startingFrameNumber = decoderState->mDecoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to determine starting frame number");
							break;
						}

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber && !(eDecoderStateDataFlagDecodingStarted & decoderState->mFlags.load(std::memory_order_relaxed))) {
							// Call the decoding started block
							if(mDecoderEventBlocks[0])
								mDecoderEventBlocks[0](*decoderState->mDecoder);
							decoderState->mFlags.fetch_or(eDecoderStateDataFlagDecodingStarted, std::memory_order_relaxed);
						}

						// Read the input chunk, converting from the decoder's format to the AUGraph's format
						UInt32 framesDecoded = mRingBufferWriteChunkSize;

						if(audioConverter) {
							auto result = AudioConverterFillComplexBuffer(audioConverter, myAudioConverterComplexInputDataProc, decoderState, &framesDecoded, bufferList, nullptr);
							if(noErr != result)
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "AudioConverterFillComplexBuffer failed: " << result);
						}
						else {
							framesDecoded = decoderState->ReadAudio(framesDecoded);

							// Bit swap if required
							auto outputFormat = mOutput->GetFormat();
							if(outputFormat.IsDSD() && (kAudioFormatFlagIsBigEndian & outputFormat.mFormatFlags) != (kAudioFormatFlagIsBigEndian & decoderState->mDecoder->GetFormat().mFormatFlags)) {
								for(UInt32 i = 0; i < decoderState->mBufferList->mNumberBuffers; ++i) {
									uint8_t *buf = (uint8_t *)decoderState->mBufferList->mBuffers[i].mData;
									auto bufsize = decoderState->mBufferList->mBuffers[i].mDataByteSize;

									while(bufsize--) {
										*buf = sBitReverseTable256[*buf];
										++buf;
									}
								}
							}
						}

						// Store the decoded audio
						if(0 != framesDecoded) {
							UInt32 framesWritten = (UInt32)mRingBuffer->WriteAudio(audioConverter ? bufferList : decoderState->mBufferList, framesDecoded);
							if(framesWritten != framesDecoded)
								LOGGER_ERR("org.sbooth.AudioEngine.Player", "RingBuffer::Store failed");

							mFramesDecoded.fetch_add(framesWritten, std::memory_order_relaxed);
						}

						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags.load(std::memory_order_relaxed))*/) {
							LOGGER_INFO("org.sbooth.AudioEngine.Player", "Decoding finished for \"" << decoderState->mDecoder->GetURL() << "\"");

							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							// Call the decoding finished block
							if(mDecoderEventBlocks[1])
								mDecoderEventBlocks[1](*decoderState->mDecoder);

							// Decoding is complete
							decoderState->mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished, std::memory_order_relaxed);
							decoderState = nullptr;

							break;
						}
					}
					// Not enough space remains in the ring buffer to write an entire decoded chunk
					else
						break;
				}

				// Start playback
				if(eAudioPlayerFlagStartPlayback & mFlags.load(std::memory_order_relaxed)) {
					mFlags.fetch_and(~eAudioPlayerFlagStartPlayback, std::memory_order_relaxed);

					if(!mOutput->IsRunning()) {
						// We don't want to start output in the middle of a buffer modification
						std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
						if(lock && !mOutput->Start())
							LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to start output");
					}
				}

				// Wait for the audio rendering thread to signal us that it could use more data, or for the timeout to happen
				mDecoderSemaphore.TimedWait(timeout);
			}

			// ========================================
			// Clean up
			// Set the appropriate flags for collection if decoding was stopped early
			if(decoderState) {
				decoderState->mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished, std::memory_order_relaxed);
				decoderState = nullptr;

				// If eAudioPlayerFlagMuteOutput is set SkipToNextTrack() is waiting for this decoder to finish
				if(eAudioPlayerFlagMuteOutput & mFlags)
					mSemaphore.Signal();
			}

			if(audioConverter) {
				auto result = AudioConverterDispose(audioConverter);
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

	while(!(eAudioPlayerFlagStopCollecting & mFlags.load(std::memory_order_relaxed))) {

		for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
			DecoderStateData *decoderState = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

			if(nullptr == decoderState)
				continue;

			auto flags = decoderState->mFlags.load(std::memory_order_relaxed);

			if(!(eDecoderStateDataFlagDecodingFinished & flags) || !(eDecoderStateDataFlagRenderingFinished & flags))
				continue;

			bool swapSucceeded = mActiveDecoders[bufferIndex].compare_exchange_strong(decoderState, nullptr);

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

#pragma mark Other Utilities

SFB::Audio::Player::DecoderStateData * SFB::Audio::Player::GetCurrentDecoderState() const
{
	DecoderStateData *result = nullptr;
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

		if(nullptr == decoderState)
			continue;

		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags.load(std::memory_order_relaxed))
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
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

		if(nullptr == decoderState)
			continue;

		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags.load(std::memory_order_relaxed))
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
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

		if(nullptr == decoderState)
			continue;

		decoderState->mFlags.fetch_or(eDecoderStateDataFlagStopDecoding, std::memory_order_relaxed);
	}

	mDecoderSemaphore.Signal();

	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex].load(std::memory_order_relaxed);

		if(nullptr == decoderState)
			continue;

		decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);
	}

	mCollectorSemaphore.Signal();
}

bool SFB::Audio::Player::SetupOutputAndRingBufferForDecoder(Decoder& decoder)
{
	// Open the decoder if necessary
	CFErrorRef error = nullptr;
	if(!decoder.IsOpen() && !decoder.Open(&error)) {
		if(error) {
			LOGGER_ERR("org.sbooth.AudioEngine.Player", "Error opening decoder: " << error);
			CFRelease(error), error = nullptr;
		}
		
		return false;
	}

	// Configure the output for decoder
	if(!mOutput->SetupForDecoder(decoder))
		return false;

	// Allocate enough space in the ring buffer for the new format
	if(!mRingBuffer->Allocate(mOutput->GetFormat(), mRingBufferCapacity)) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to allocate ring buffer");
		return false;
	}

	return true;
}

SFB::Audio::Output& SFB::Audio::Player::GetOutput() const
{
	return *mOutput;
}

bool SFB::Audio::Player::SetOutput(Output::unique_ptr output)
{
	if(!output)
		return false;

	Stop();

	if(!mOutput->Close())
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "Unable to close output");

	if(!output->Open()) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Player", "Unable to open output");
		return false;
	}

	output->SetPlayer(this);
	mOutput = std::move(output);

	return true;
}

UInt32 SFB::Audio::Player::ProvideAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// ========================================
	// Pre-rendering actions

	// Call the pre-render block
	if(mRenderEventBlocks[0])
		mRenderEventBlocks[0](bufferList, frameCount);

	// Mute output if requested
	if(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed)) {
		mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
		mFlags.fetch_and(~eAudioPlayerFlagRequestMute, std::memory_order_relaxed);

		mSemaphore.Signal();
	}


	// ========================================
	// Rendering
	size_t framesAvailableToRead = mRingBuffer->GetFramesAvailableToRead();

	// Output silence if muted or the ring buffer is empty
	auto outputFormat = mOutput->GetFormat();
	if(eAudioPlayerFlagMuteOutput & mFlags || 0 == framesAvailableToRead) {
		size_t byteCountToZero = outputFormat.FrameCountToByteCount(frameCount);
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
			memset(bufferList->mBuffers[bufferIndex].mData, outputFormat.IsDSD() ? 0xF : 0, byteCountToZero);
			bufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
		}

		return frameCount;
	}

	// Restrict reads to valid decoded audio
	size_t framesToRead = std::min((UInt32)framesAvailableToRead, frameCount);
	UInt32 framesRead = (UInt32)mRingBuffer->ReadAudio(bufferList, framesToRead);
	if(framesRead != framesToRead) {
		LOGGER_ERR("org.sbooth.AudioEngine.Player", "RingBuffer::ReadAudio failed: Requested " << framesToRead << " frames, got " << framesRead);
		return framesRead;
	}

	mFramesRendered.fetch_add(framesRead, std::memory_order_relaxed);

	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesRead != frameCount) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Player", "Insufficient audio in ring buffer: " << framesRead << " frames available, " << frameCount << " requested");

		size_t framesOfSilence = frameCount - framesRead;
		size_t byteCountToSkip = outputFormat.FrameCountToByteCount(framesRead);
		size_t byteCountToZero = outputFormat.FrameCountToByteCount(framesOfSilence);
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
			memset((int8_t *)bufferList->mBuffers[bufferIndex].mData + byteCountToSkip, outputFormat.IsDSD() ? 0xF : 0, byteCountToZero);
		}
	}

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();
	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
		mDecoderSemaphore.Signal();


	// ========================================
	// Post-rendering actions

	// Call the post-render block
	if(mRenderEventBlocks[1])
		mRenderEventBlocks[1](bufferList, frameCount);

	// There is nothing more to do if no frames were rendered
	if(0 == framesRead)
		return 0;

	// framesRead contains the number of valid frames that were rendered
	// However, these could have come from any number of decoders depending on the buffer sizes
	// So it is necessary to split them up here

	SInt64 framesRemainingToDistribute = framesRead;
	DecoderStateData *decoderState = GetCurrentDecoderState();

	// mActiveDecoders is not an ordered array, so to ensure that callbacks are performed
	// in the proper order multiple passes are made here
	while(nullptr != decoderState) {
		SInt64 timeStamp = decoderState->mTimeStamp;

		SInt64 decoderFramesRemaining = (-1 == decoderState->mTotalFrames ? framesRead : decoderState->mTotalFrames - decoderState->mFramesRendered);
		SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, (SInt64)framesRead);

		if(0 == decoderState->mFramesRendered && !(eDecoderStateDataFlagRenderingStarted & decoderState->mFlags.load(std::memory_order_relaxed))) {
			// Call the rendering started block
			if(mDecoderEventBlocks[2])
				mDecoderEventBlocks[2](*decoderState->mDecoder);
			decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingStarted, std::memory_order_relaxed);
		}

		decoderState->mFramesRendered.fetch_add(framesFromThisDecoder, std::memory_order_relaxed);

		if((eDecoderStateDataFlagDecodingFinished & decoderState->mFlags.load(std::memory_order_relaxed)) && decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags.load(std::memory_order_relaxed))*/) {
			// Call the rendering finished block
			if(mDecoderEventBlocks[3])
				mDecoderEventBlocks[3](*decoderState->mDecoder);

			decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);
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
			mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
			mFlags.fetch_and(~eAudioPlayerFlagFormatMismatch, std::memory_order_relaxed);
			mSemaphore.Signal();
		}
		// Calling ASIOStop() from within a callback causes a crash, at least with exaSound's ASIO driver
		else
			mOutput->RequestStop();
	}

	return framesRead;
}
