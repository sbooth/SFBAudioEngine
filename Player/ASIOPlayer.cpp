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

#include "AsioLibWrapper.h"

#include "ASIOPlayer.h"
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
	
	enum eMessageQueueEvents : uint32_t {
		eMessageQueueEventStopPlayback			= 'stop',
		eMessageQueueEventASIOResetNeeded		= 'rest',
		eMessageQueueEventASIOOverload			= 'ovld'
	};
}


// ========================================
// State data for decoders that are decoding and/or rendering
// ========================================
class SFB::Audio::ASIO::Player::DecoderStateData
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
			LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Couldn't set thread's extended policy: " << mach_error_string(error));
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
			LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Couldn't set thread's precedence policy: " << mach_error_string(error));
			return false;
		}

		return true;
	}
}

namespace {

	SFB::Audio::AudioFormat AudioFormatForASIOSampleType(ASIOSampleType sampleType)
	{
		SFB::Audio::AudioFormat result;

		switch (sampleType) {
				// 16 bit samples
			case ASIOSTInt16LSB:
			case ASIOSTInt16MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 16;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;
				

				// 24 bit samples
			case ASIOSTInt24LSB:
			case ASIOSTInt24MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 24;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 32 bit samples
			case ASIOSTInt32LSB:
			case ASIOSTInt32MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 32;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 32 bit float (float) samples
			case ASIOSTFloat32LSB:
			case ASIOSTFloat32MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 32;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 64 bit float (double) samples
			case ASIOSTFloat64LSB:
			case ASIOSTFloat64MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 64;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// other bit depths aligned in 32 bits
			case ASIOSTInt32LSB16:
			case ASIOSTInt32MSB16:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 16;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB18:
			case ASIOSTInt32MSB18:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 18;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB20:
			case ASIOSTInt32MSB20:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 20;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB24:
			case ASIOSTInt32MSB24:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 24;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// DSD
			case ASIOSTDSDInt8LSB1:
			case ASIOSTDSDInt8MSB1:
				result.mFormatID			= SFB::Audio::kAudioFormatDirectStreamDigital;
				result.mFormatFlags			= kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 1;
				result.mBytesPerPacket		= 1;
				result.mFramesPerPacket		= 8;
				result.mBytesPerFrame		= 0;
				break;


			case ASIOSTDSDInt8NER8:
				result.mFormatID			= SFB::Audio::kAudioFormatDirectStreamDigital;
				result.mFormatFlags			= kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 8;
				result.mBytesPerPacket		= 1;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= 1;
				break;
		}


		// Add big endian flag
		switch (sampleType) {
			case ASIOSTInt16MSB:
			case ASIOSTInt24MSB:
			case ASIOSTInt32MSB:
			case ASIOSTFloat32MSB:
			case ASIOSTFloat64MSB:
			case ASIOSTInt32MSB16:
			case ASIOSTInt32MSB18:
			case ASIOSTInt32MSB20:
			case ASIOSTInt32MSB24:
			case ASIOSTDSDInt8MSB1:
				result.mFormatFlags			|= kAudioFormatFlagIsBigEndian;
				break;
		}


		return result;
	}

	// ========================================
	// Information about an ASIO driver
	struct DriverInfo
	{
		ASIODriverInfo	mDriverInfo;

		long			mInputChannelCount;
		long			mOutputChannelCount;

		long			mMinimumBufferSize;
		long			mMaximumBufferSize;
		long			mPreferredBufferSize;
		long			mBufferGranularity;

		ASIOSampleType	mFormat;
		ASIOSampleRate	mSampleRate;

		bool			mPostOutput;

		long			mInputLatency;
		long			mOutputLatency;

		long			mInputBufferCount;	// becomes number of actual created input buffers
		long			mOutputBufferCount;	// becomes number of actual created output buffers

		ASIOBufferInfo	*mBufferInfo;
		ASIOChannelInfo	*mChannelInfo;
		// The above two arrays share the same indexing, as the data in them are linked together

		AudioBufferList *mBufferList;
		
		// Information from ASIOGetSamplePosition()
		// data is converted to double floats for easier use, however 64 bit integer can be used, too
		double			mNanoseconds;
		double			mSamples;
		double			mTCSamples;	// time code samples

		ASIOTime		mTInfo;			// time info state
		unsigned long	mSysRefTime;      // system reference time, when bufferSwitch() was called
	};

	// ========================================
	// Callback prototypes
	void myASIOBufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	void myASIOSampleRateDidChange(ASIOSampleRate sRate);
	long myASIOMessage(long selector, long value, void *message, double *opt);
	ASIOTime * myASIOBufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);

	// ========================================
	// Sadly ASIO requires global state
	static SFB::Audio::ASIO::Player *sPlayer	= nullptr;
	static AsioDriver		*sASIO		= nullptr;
	static DriverInfo		sDriverInfo	= {{0}};
	static ASIOCallbacks	sCallbacks	= {
		.bufferSwitch			= myASIOBufferSwitch,
		.sampleRateDidChange	= myASIOSampleRateDidChange,
		.asioMessage			= myASIOMessage,
		.bufferSwitchTimeInfo	= myASIOBufferSwitchTimeInfo
	};

	// ========================================
	// Callbacks

	// Backdoor into myASIOBufferSwitchTimeInfo
	void myASIOBufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
	{
		ASIOTime timeInfo = {{0}};

		auto result = sASIO->getSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime);
		if(ASE_OK == result)
			timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

		myASIOBufferSwitchTimeInfo(&timeInfo, doubleBufferIndex, directProcess);
	}

	void myASIOSampleRateDidChange(ASIOSampleRate sRate)
	{
		LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "myASIOSampleRateDidChange: New sample rate " << sRate);
	}

	long myASIOMessage(long selector, long value, void *message, double *opt)
	{
		if(sPlayer)
			return sPlayer->HandleASIOMessage(selector, value, message, opt);
		return 0;
	}

	ASIOTime * myASIOBufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess)
	{
		if(sPlayer)
			sPlayer->FillASIOBuffer(doubleBufferIndex);
		return nullptr;
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

		auto decoderStateData = static_cast<SFB::Audio::ASIO::Player::DecoderStateData *>(inUserData);
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

//SFB::Audio::ASIO::Player * SFB::Audio::ASIO::Player::GetInstance()
//{
//	static Player *sPlayer = nullptr;
//	static dispatch_once_t onceToken;
//	dispatch_once(&onceToken, ^{
//		sPlayer = new Player;
//	});
//	return sPlayer;
//}

SFB::Audio::ASIO::Player::Player()
: mFlags(ATOMIC_VAR_INIT(0)), mRingBuffer(new RingBuffer), mRingBufferCapacity(ATOMIC_VAR_INIT(RING_BUFFER_CAPACITY_FRAMES)), mRingBufferWriteChunkSize(ATOMIC_VAR_INIT(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES)), mFramesDecoded(ATOMIC_VAR_INIT(0)), mFramesRendered(ATOMIC_VAR_INIT(0)), mFormatMismatchBlock(nullptr), mEventQueueTimer(), mEventQueue(new SFB::RingBuffer)
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
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIO.Player", "Unable to create decoder thread: " << e.what());

		throw;
	}

	// ========================================
	// Launch the collector thread
	try {
		mCollectorThread = std::thread(&Player::CollectorThreadEntry, this);
	}

	catch(const std::exception& e) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIO.Player", "Unable to create collector thread: " << e.what());

		mFlags.fetch_or(eAudioPlayerFlagStopDecoding, std::memory_order_relaxed);
		mDecoderSemaphore.Signal();

		try {
			mDecoderThread.join();
		}

		catch(const std::exception& e) {
			LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to join decoder thread: " << e.what());
		}

		throw;
	}

	// ========================================
	// Start the event dispatch timer

	mEventQueue->Allocate(1024);

	mEventQueueTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
	dispatch_source_set_timer(mEventQueueTimer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 5, NSEC_PER_SEC / 3);

	dispatch_source_set_event_handler(mEventQueueTimer, ^{

		// Process player events
		while(mEventQueue->GetBytesAvailableToRead()) {
			uint32_t eventCode;
			auto bytesRead = mEventQueue->Read(&eventCode, sizeof(eventCode));
			if(bytesRead != sizeof(eventCode)) {
				LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Error reading event from queue");
				break;
			}

			switch(eventCode) {
				case eMessageQueueEventStopPlayback:
					StopOutput();
					break;

				case eMessageQueueEventASIOResetNeeded:
					ResetOutput();
					break;

				case eMessageQueueEventASIOOverload:
					LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "ASIO overload");
					break;
			}
		}


	});

	// Start the timer
	dispatch_resume(mEventQueueTimer);

	// ========================================
	// Set up output
	if(!OpenOutput()) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIO.Player", "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

SFB::Audio::ASIO::Player::~Player()
{
	Stop();

	// Stop the processing graph and reclaim its resources
	if(!CloseOutput())
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "CloseOutput() failed");

	// End the decoding thread
	mFlags.fetch_or(eAudioPlayerFlagStopDecoding, std::memory_order_relaxed);
	mDecoderSemaphore.Signal();

	try {
		mDecoderThread.join();
	}

	catch(const std::exception& e) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to join decoder thread: " << e.what());
	}

	// End the collector thread
	mFlags.fetch_or(eAudioPlayerFlagStopCollecting, std::memory_order_relaxed);
	mCollectorSemaphore.Signal();

	try {
		mCollectorThread.join();
	}

	catch(const std::exception& e) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to join collector thread: " << e.what());
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

	dispatch_release(mEventQueueTimer);
}

#pragma mark Playback Control

bool SFB::Audio::ASIO::Player::Play()
{
	if(!OutputIsRunning())
		return StartOutput();

	return true;
}

bool SFB::Audio::ASIO::Player::Pause()
{
	if(OutputIsRunning())
		StopOutput();

	return true;
}

bool SFB::Audio::ASIO::Player::Stop()
{
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	if(OutputIsRunning())
		StopOutput();

	StopActiveDecoders();

	if(!ResetOutput())
		return false;

	// Reset the ring buffer
	mFramesDecoded.store(0, std::memory_order_relaxed);
	mFramesRendered.store(0, std::memory_order_relaxed);

	mFlags.fetch_or(eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

	return true;
}

SFB::Audio::ASIO::Player::PlayerState SFB::Audio::ASIO::Player::GetPlayerState() const
{
	if(OutputIsRunning())
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

CFURLRef SFB::Audio::ASIO::Player::GetPlayingURL() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return nullptr;

	return currentDecoderState->mDecoder->GetURL();
}

void * SFB::Audio::ASIO::Player::GetPlayingRepresentedObject() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return nullptr;

	return currentDecoderState->mDecoder->GetRepresentedObject();
}

#pragma mark Block-based callback support

void SFB::Audio::ASIO::Player::SetDecodingStartedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[0])
		Block_release(mDecoderEventBlocks[0]), mDecoderEventBlocks[0] = nullptr;
	if(block)
		mDecoderEventBlocks[0] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetDecodingFinishedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[1])
		Block_release(mDecoderEventBlocks[1]), mDecoderEventBlocks[1] = nullptr;
	if(block)
		mDecoderEventBlocks[1] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetRenderingStartedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[2])
		Block_release(mDecoderEventBlocks[2]), mDecoderEventBlocks[2] = nullptr;
	if(block)
		mDecoderEventBlocks[2] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetRenderingFinishedBlock(AudioPlayerDecoderEventBlock block)
{
	if(mDecoderEventBlocks[3])
		Block_release(mDecoderEventBlocks[3]), mDecoderEventBlocks[3] = nullptr;
	if(block)
		mDecoderEventBlocks[3] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetPreRenderBlock(AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[0])
		Block_release(mRenderEventBlocks[0]), mRenderEventBlocks[0] = nullptr;
	if(block)
		mRenderEventBlocks[0] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetPostRenderBlock(AudioPlayerRenderEventBlock block)
{
	if(mRenderEventBlocks[1])
		Block_release(mRenderEventBlocks[1]), mRenderEventBlocks[1] = nullptr;
	if(block)
		mRenderEventBlocks[1] = Block_copy(block);
}

void SFB::Audio::ASIO::Player::SetFormatMismatchBlock(AudioPlayerFormatMismatchBlock block)
{
	if(mFormatMismatchBlock)
		Block_release(mFormatMismatchBlock), mFormatMismatchBlock = nullptr;
	if(block)
		mFormatMismatchBlock = Block_copy(block);
}

#pragma mark Playback Properties

bool SFB::Audio::ASIO::Player::GetCurrentFrame(SInt64& currentFrame) const
{
	SInt64 totalFrames;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool SFB::Audio::ASIO::Player::GetTotalFrames(SInt64& totalFrames) const
{
	SInt64 currentFrame;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool SFB::Audio::ASIO::Player::GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const
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

bool SFB::Audio::ASIO::Player::GetCurrentTime(CFTimeInterval& currentTime) const
{
	CFTimeInterval totalTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool SFB::Audio::ASIO::Player::GetTotalTime(CFTimeInterval& totalTime) const
{
	CFTimeInterval currentTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool SFB::Audio::ASIO::Player::GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool SFB::Audio::ASIO::Player::GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool SFB::Audio::ASIO::Player::SeekForward(CFTimeInterval secondsToSkip)
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

bool SFB::Audio::ASIO::Player::SeekBackward(CFTimeInterval secondsToSkip)
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

bool SFB::Audio::ASIO::Player::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	SInt64 desiredFrame		= (SInt64)(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;

	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool SFB::Audio::ASIO::Player::SeekToFrame(SInt64 frame)
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
	if(!OutputIsRunning())
		mFlags.fetch_or(eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

	mDecoderSemaphore.Signal();

	return true;
}

bool SFB::Audio::ASIO::Player::SupportsSeeking() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	return currentDecoderState->mDecoder->SupportsSeeking();
}

#pragma mark Device Management

bool SFB::Audio::ASIO::Player::GetOutputDeviceIOFormat(DeviceIOFormat& deviceIOFormat) const
{
	ASIOIoFormat asioFormat;
	auto result = sASIO->future(kAsioGetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to get ASIO format: " << result);
		return false;
	}

	switch(asioFormat.FormatType) {
		case kASIOPCMFormat:	deviceIOFormat = DeviceIOFormat::eDeviceIOFormatPCM;	break;
		case kASIODSDFormat:	deviceIOFormat = DeviceIOFormat::eDeviceIOFormatDSD;	break;

		case kASIOFormatInvalid:
		default:
			return false;
	}

	return true;
}

bool SFB::Audio::ASIO::Player::SetOutputDeviceIOFormat(const DeviceIOFormat& deviceIOFormat)
{
	ASIOIoFormat asioFormat = {
		.FormatType		= DeviceIOFormat::eDeviceIOFormatPCM == deviceIOFormat ? kASIOPCMFormat : kASIODSDFormat,
		.future			= {0}
	};

	auto result = sASIO->future(kAsioSetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to set ASIO format: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::ASIO::Player::GetOutputDeviceSampleRate(Float64& sampleRate) const
{
	auto result = sASIO->getSampleRate(&sampleRate);
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to get sample rate: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::ASIO::Player::SetOutputDeviceSampleRate(Float64 sampleRate)
{
	auto result = sASIO->canSampleRate(sampleRate);
	if(ASE_OK == result) {
		result = sASIO->setSampleRate(sampleRate);
		if(ASE_OK != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to set sample rate: " << result);
			return false;
		}
	}
	else {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Sample rate not supported: " << sampleRate);
		return false;
	}

	return true;
}

#pragma mark Playlist Management

bool SFB::Audio::ASIO::Player::Play(CFURLRef url)
{
	if(nullptr == url)
		return false;

	auto decoder = Decoder::CreateDecoderForURL(url);
	return Play(decoder);
}

bool SFB::Audio::ASIO::Player::Play(Decoder::unique_ptr& decoder)
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

bool SFB::Audio::ASIO::Player::Enqueue(CFURLRef url)
{
	if(nullptr == url)
		return false;

	auto decoder = Decoder::CreateDecoderForURL(url);
	return Enqueue(decoder);
}

bool SFB::Audio::ASIO::Player::Enqueue(Decoder::unique_ptr& decoder)
{
	if(!decoder)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Enqueuing \"" << decoder->GetURL() << "\"");

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

bool SFB::Audio::ASIO::Player::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Skipping \"" << currentDecoderState->mDecoder->GetURL() << "\"");

	if(OutputIsRunning()) {
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

bool SFB::Audio::ASIO::Player::ClearQueuedDecoders()
{
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	mDecoderQueue.clear();

	return true;
}

#pragma mark Ring Buffer Parameters

bool SFB::Audio::ASIO::Player::SetRingBufferCapacity(uint32_t bufferCapacity)
{
	if(0 == bufferCapacity || mRingBufferWriteChunkSize > bufferCapacity)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Setting ring buffer capacity to " << bufferCapacity);

	mRingBufferCapacity.store(bufferCapacity, std::memory_order_relaxed);
	return true;
}

bool SFB::Audio::ASIO::Player::SetRingBufferWriteChunkSize(uint32_t chunkSize)
{
	if(0 == chunkSize || mRingBufferCapacity < chunkSize)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Setting ring buffer write chunk size to " << chunkSize);

	mRingBufferWriteChunkSize.store(chunkSize, std::memory_order_relaxed);
	return true;
}

#pragma mark Callbacks

//OSStatus SFB::Audio::ASIO::Player::Render(AudioUnitRenderActionFlags		*ioActionFlags,
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
//		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "RingBuffer::ReadAudio failed: Requested " << framesToRead << " frames, got " << framesRead);
//		return 1;
//	}
//
//	mFramesRenderedLastPass = framesRead;
//	mFramesRendered.fetch_add(framesRead, std::memory_order_relaxed);
//
//	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
//	if(framesRead != inNumberFrames) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Insufficient audio in ring buffer: " << framesRead << " frames available, " << inNumberFrames << " requested");
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
//OSStatus SFB::Audio::ASIO::Player::RenderNotify(AudioUnitRenderActionFlags		*ioActionFlags,
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

void * SFB::Audio::ASIO::Player::DecoderThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Decoder");

	// ========================================
	// Make ourselves a high priority thread
	if(!setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Couldn't set decoder thread importance");

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
						LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Error opening decoder: " << error);
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

			if(nextFormat.mFormatID != mRingBufferFormat.mFormatID) {
				LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Gapless join failed: Ring buffer format (" << mRingBufferFormat.mFormatID << ") and decoder format (" << nextFormat.mFormatID << ") don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mSampleRate != mRingBufferFormat.mSampleRate) {
				LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Gapless join failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mChannelsPerFrame != mRingBufferFormat.mChannelsPerFrame) {
				LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Gapless join failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
				formatsMatch = false;
			}

			// Enqueue the decoder if its channel layout matches the ring buffer's channel layout (so the channel map in the output AU will remain valid)
			if(nextChannelLayout != mRingBufferChannelLayout) {
				LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Gapless join failed: Ring buffer channel layout (" << mRingBufferChannelLayout << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
				formatsMatch = false;
			}

			// If the formats don't match, the decoder can't be used with the current ring buffer format
			if(!formatsMatch) {
				// Ensure output is muted before performing operations that aren't thread safe
				if(OutputIsRunning()) {
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
					mFormatMismatchBlock(mRingBufferFormat, nextFormat);

				// Adjust the formats
				{
					std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
					if(lock)
						SetupOutputAndRingBufferForDecoder(*decoderState->mDecoder);
					else
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
					LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "compare_exchange_strong() failed");
			}
		}

		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(decoderState) {
			LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Decoding starting for \"" << decoderState->mDecoder->GetURL() << "\"");
			LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Decoder format: " << decoderState->mDecoder->GetFormat());
			LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Decoder channel layout: " << decoderState->mDecoder->GetChannelLayout());

			const AudioFormat& decoderFormat = decoderState->mDecoder->GetFormat();

			// ========================================
			// Create the AudioConverter which will convert from the decoder's format to the ring buffer format (for PCM output)
			AudioConverterRef audioConverter = nullptr;
			BufferList bufferList;
			if(mRingBufferFormat.IsPCM()) {
				OSStatus result = AudioConverterNew(&decoderFormat, &mRingBufferFormat, &audioConverter);
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterNew failed: " << result);

					decoderState->mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished | eDecoderStateDataFlagRenderingFinished, std::memory_order_relaxed);

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
					LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);

				// ========================================
				// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
				decoderState->AllocateBufferList((UInt32)decoderFormat.ByteCountToFrameCount(inputBufferSize));
				bufferList.Allocate(mRingBufferFormat, mRingBufferWriteChunkSize);
			}
			else if(kAudioFormatDirectStreamDigital == mRingBufferFormat.mFormatID)
				decoderState->AllocateBufferList((UInt32)sDriverInfo.mPreferredBufferSize);


			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(!(eAudioPlayerFlagStopDecoding & mFlags.load(std::memory_order_relaxed)) && decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags.load(std::memory_order_relaxed))) {

				// Fill the ring buffer with as much data as possible
				for(;;) {

					// Reset the ring buffer if required
					if(eAudioPlayerFlagRingBufferNeedsReset & mFlags.load(std::memory_order_relaxed)) {

						mFlags.fetch_and(~eAudioPlayerFlagRingBufferNeedsReset, std::memory_order_relaxed);

						// Ensure output is muted before performing operations that aren't thread safe
						if(OutputIsRunning()) {
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
								LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterReset failed: " << result);
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
							LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "Seeking to frame " << frameToSeek);

							// Ensure output is muted before performing operations that aren't thread safe
							if(OutputIsRunning()) {
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
								LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Error seeking to frame  " << frameToSeek);

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
										LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterReset failed: " << result);
								}

								// Reset the ring buffer
								mRingBuffer->Reset();
							}

							// Clear the mute flag
							mFlags.fetch_and(~eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
						}

						SInt64 startingFrameNumber = decoderState->mDecoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to determine starting frame number");
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
								LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterFillComplexBuffer failed: " << result);
						}
						else {
							framesDecoded = decoderState->ReadAudio(framesDecoded);

							// Bit swap if required
							if(mRingBufferFormat.IsDSD() && (kAudioFormatFlagIsBigEndian & mRingBufferFormat.mFormatFlags) != (kAudioFormatFlagIsBigEndian & decoderState->mDecoder->GetFormat().mFormatFlags)) {
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
								LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "RingBuffer::Store failed");

							mFramesDecoded.fetch_add(framesWritten, std::memory_order_relaxed);
						}

						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags.load(std::memory_order_relaxed))*/) {
							LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Decoding finished for \"" << decoderState->mDecoder->GetURL() << "\"");

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

					if(!OutputIsRunning() && !StartOutput())
						LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to start output");
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
					LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "AudioConverterDispose failed: " << result);
				audioConverter = nullptr;
			}
		}

		// Wait for another thread to wake us, or for the timeout to happen
		mDecoderSemaphore.TimedWait(timeout);
	}

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Decoding thread terminating");

	return nullptr;
}

void * SFB::Audio::ASIO::Player::CollectorThreadEntry()
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
				LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "Collecting decoder: \"" << decoderState->mDecoder->GetURL() << "\"");
				delete decoderState, decoderState = nullptr;
			}
		}

		// Wait for any thread to signal us to try and collect finished decoders
		mCollectorSemaphore.TimedWait(timeout);
	}

	LOGGER_INFO("org.sbooth.AudioEngine.ASIO.Player", "Collecting thread terminating");

	return nullptr;
}

#pragma mark ASIO Utilities

bool SFB::Audio::ASIO::Player::OpenOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "OpenOutput");

	int count = AsioLibWrapper::GetAsioLibraryList(nullptr, 0);
	if(0 == count) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to load ASIO library list");
		return false;
	}

	AsioLibInfo buffer [count];
	count = AsioLibWrapper::GetAsioLibraryList(buffer, (unsigned int)count);
	if(0 == count) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to load ASIO library list");
		return false;
	}

	// FIXME: Select the appropriate driver
	// Only 0 or 2 seems to work
	unsigned int libIndex = 0;

	if(!AsioLibWrapper::LoadLib(buffer[libIndex])) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to load ASIO library");
		return false;
	}

	if(AsioLibWrapper::CreateInstance(buffer[libIndex].Number, &sASIO)) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to instantiate ASIO driver");
		return false;
	}

	sDriverInfo.mDriverInfo = {
		.asioVersion = 2,
		.sysRef = nullptr
	};

	if(!sASIO->init(&sDriverInfo.mDriverInfo)){
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to init ASIO driver: " << sDriverInfo.mDriverInfo.errorMessage);
		return false;
	}

	// Determine whether to post output notifications
	if(ASE_OK == sASIO->outputReady())
		sDriverInfo.mPostOutput = true;

	return true;
}

bool SFB::Audio::ASIO::Player::CloseOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "CloseOutput");

	if(nullptr == sASIO)
		return false;

	sASIO->disposeBuffers();
	delete sASIO, sASIO = nullptr;
	sDriverInfo = {{0}};

	return true;
}

bool SFB::Audio::ASIO::Player::StartOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "StartOutput");

	if(OutputIsRunning())
		return true;

	// We don't want to start output in the middle of a buffer modification
	std::unique_lock<std::mutex> lock(mMutex, std::try_to_lock);
	if(!lock)
		return false;

	if(nullptr == sASIO || nullptr != sPlayer)
		return false;

	auto result = sASIO->start();
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "start() failed: " << result);
		return false;
	}

	sPlayer = this;

	return true;
}

bool SFB::Audio::ASIO::Player::StopOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "StopOutput");

	if(!OutputIsRunning())
		return true;

	if(nullptr == sASIO)
		return false;

	auto result = sASIO->stop();
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "stop() failed: " << result);
		return false;
	}

	sPlayer = nullptr;

	return true;
}

bool SFB::Audio::ASIO::Player::OutputIsRunning() const
{
	return nullptr != sPlayer;
}

bool SFB::Audio::ASIO::Player::ResetOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.ASIO.Player", "ResetOutput");

	if(!StopOutput())
		return false;

	if(nullptr == sASIO)
		return false;

	sASIO->disposeBuffers();

	if(!sASIO->init(&sDriverInfo.mDriverInfo)){
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to init ASIO driver: " << sDriverInfo.mDriverInfo.errorMessage);
		return false;
	}

	if(ASE_OK == sASIO->outputReady())
		sDriverInfo.mPostOutput = true;

	return true;
}

#pragma mark Other Utilities

SFB::Audio::ASIO::Player::DecoderStateData * SFB::Audio::ASIO::Player::GetCurrentDecoderState() const
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

SFB::Audio::ASIO::Player::DecoderStateData * SFB::Audio::ASIO::Player::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const
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

void SFB::Audio::ASIO::Player::StopActiveDecoders()
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

bool SFB::Audio::ASIO::Player::SetupOutputAndRingBufferForDecoder(Decoder& decoder)
{
	// Open the decoder if necessary
	CFErrorRef error = nullptr;
	if(!decoder.IsOpen() && !decoder.Open(&error)) {
		if(error) {
			LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Error opening decoder: " << error);
			CFRelease(error), error = nullptr;
		}
		
		return false;
	}
	
	const AudioFormat& format = decoder.GetFormat();
	if(!format.IsPCM() && !format.IsDSD()) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "ASIO driver unsupported format: " << format);
		return false;
	}

	// Clean up existing state
	sASIO->disposeBuffers();

	sDriverInfo.mInputBufferCount = 0;
	sDriverInfo.mOutputBufferCount = 0;

	if(sDriverInfo.mBufferInfo)
		delete [] sDriverInfo.mBufferInfo, sDriverInfo.mBufferInfo = nullptr;

	if(sDriverInfo.mChannelInfo)
		delete [] sDriverInfo.mChannelInfo, sDriverInfo.mChannelInfo = nullptr;

	if(sDriverInfo.mBufferList)
		free(sDriverInfo.mBufferList), sDriverInfo.mBufferList = nullptr;

	// Configure the ASIO driver with the decoder's format
	ASIOIoFormat asioFormat = {
		.FormatType		= format.IsPCM() ? kASIOPCMFormat : format.IsDSD() ? kASIODSDFormat : kASIOFormatInvalid,
		.future			= {0}
	};

	auto result = sASIO->future(kAsioSetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to set ASIO format: " << result);
		return false;
	}

	// Set the sample rate if supported
	SetOutputDeviceSampleRate(format.mSampleRate);


	// Store the ASIO driver format
	asioFormat = {0};
	result = sASIO->future(kAsioGetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to get ASIO format: " << result);
		return false;
	}

	sDriverInfo.mFormat = asioFormat.FormatType;
//	if(asioFormat.FormatType != format.streamType) {
//		return false;
//	}

	if(!GetOutputDeviceSampleRate(sDriverInfo.mSampleRate))
		return false;


	// Query available channels
	result = sASIO->getChannels(&sDriverInfo.mInputChannelCount, &sDriverInfo.mOutputChannelCount);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to obtain ASIO channel count: " << result);
		return false;
	}

//	if(0 == sDriverInfo.mOutputChannelCount) {
//		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "No available output channels");
//		return false;
//	}

	// Get the preferred buffer size
	result = sASIO->getBufferSize(&sDriverInfo.mMinimumBufferSize, &sDriverInfo.mMaximumBufferSize, &sDriverInfo.mPreferredBufferSize, &sDriverInfo.mBufferGranularity);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to obtain ASIO buffer size: " << result);
		return false;
	}

	// Prepare ASIO buffers

	sDriverInfo.mInputBufferCount = std::min(sDriverInfo.mInputChannelCount, 0L);
	sDriverInfo.mOutputBufferCount = std::min(sDriverInfo.mOutputChannelCount, (long)format.mChannelsPerFrame);

	sDriverInfo.mBufferInfo = new ASIOBufferInfo [sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount];
	sDriverInfo.mChannelInfo = new ASIOChannelInfo [sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount];

	for(long channelIndex = 0; channelIndex < sDriverInfo.mInputBufferCount; ++channelIndex) {
		sDriverInfo.mBufferInfo[channelIndex].isInput = ASIOTrue;
		sDriverInfo.mBufferInfo[channelIndex].channelNum = channelIndex;
		sDriverInfo.mBufferInfo[channelIndex].buffers[0] = sDriverInfo.mBufferInfo[channelIndex].buffers[1] = nullptr;
	}

	for(long channelIndex = sDriverInfo.mInputBufferCount; channelIndex < sDriverInfo.mOutputBufferCount; ++channelIndex) {
		sDriverInfo.mBufferInfo[channelIndex].isInput = ASIOFalse;
		sDriverInfo.mBufferInfo[channelIndex].channelNum = channelIndex;
		sDriverInfo.mBufferInfo[channelIndex].buffers[0] = sDriverInfo.mBufferInfo[channelIndex].buffers[1] = nullptr;
	}

	// Create the buffers
	result = sASIO->createBuffers(sDriverInfo.mBufferInfo, sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount, sDriverInfo.mPreferredBufferSize, &sCallbacks);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to create ASIO buffers: " << result);
		return false;
	}

	// Get the buffer details, sample word length, name, word clock group and activation
	for(long i = 0; i < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++i) {
		sDriverInfo.mChannelInfo[i].channel = sDriverInfo.mBufferInfo[i].channelNum;
		sDriverInfo.mChannelInfo[i].isInput = sDriverInfo.mBufferInfo[i].isInput;

		result = sASIO->getChannelInfo(&sDriverInfo.mChannelInfo[i]);
		if(ASE_OK != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.ASIOPlayer", "Unable to get ASIO channel information: " << result);
			break;
		}
	}

	// Allocate a shell ABL to point to the ASIO buffers
	sDriverInfo.mBufferList = (AudioBufferList *)malloc(offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * (size_t)sDriverInfo.mOutputBufferCount));
	sDriverInfo.mBufferList->mNumberBuffers = (UInt32)sDriverInfo.mOutputBufferCount;


	// Get input and output latencies
	if(ASE_OK == result) {
		// Latencies often are only valid after ASIOCreateBuffers()
		//  (input latency is the age of the first sample in the currently returned audio block)
		//  (output latency is the time the first sample in the currently returned audio block requires to get to the output)
		result = sASIO->getLatencies(&sDriverInfo.mInputLatency, &sDriverInfo.mOutputLatency);
		if(ASE_OK != result)
			LOGGER_ERR("org.sbooth.AudioEngine.ASIOPlayer", "Unable to get ASIO latencies: " << result);
	}


	// Set the ring buffer format to the first output channel
	// FIXME: Can each channel have a separate format?
	for(long i = 0; i < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++i) {
		if(!sDriverInfo.mChannelInfo[i].isInput) {
			mRingBufferFormat = AudioFormatForASIOSampleType(sDriverInfo.mChannelInfo[i].type);
			mRingBufferFormat.mSampleRate = sDriverInfo.mSampleRate;
			mRingBufferFormat.mChannelsPerFrame = (UInt32)sDriverInfo.mOutputBufferCount;

			LOGGER_INFO("org.sbooth.AudioEngine.ASIOPlayer", "Ring buffer format: " << mRingBufferFormat);
			break;
		}
	}

	// Attempt to set the output audio unit's channel map
	const ChannelLayout& channelLayout = decoder.GetChannelLayout();
//	if(!SetOutputUnitChannelMap(channelLayout))
//		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to set output unit channel map");

	// The decoder's channel layout becomes the ring buffer's channel layout
	mRingBufferChannelLayout = channelLayout;

	// Ensure the ring buffer is large enough
	if(4 * sDriverInfo.mPreferredBufferSize > mRingBufferCapacity)
		mRingBufferCapacity = (UInt32)(4 * sDriverInfo.mPreferredBufferSize);

	// Allocate enough space in the ring buffer for the new format
	if(!mRingBuffer->Allocate(mRingBufferFormat, mRingBufferCapacity)) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "Unable to allocate ring buffer");
		return false;
	}

	return true;
}

long SFB::Audio::ASIO::Player::HandleASIOMessage(long selector, long value, void *message, double *opt)
{
	switch(selector) {
		case kAsioSelectorSupported:
			if(value == kAsioResetRequest || value == kAsioEngineVersion || value == kAsioResyncRequest || value == kAsioLatenciesChanged || value == kAsioSupportsTimeInfo || value == kAsioSupportsTimeCode || value == kAsioSupportsInputMonitor)
				return 1;
			break;

		case kAsioResetRequest:
		{
			uint32_t event = eMessageQueueEventASIOResetNeeded;
			mEventQueue->Write(&event, sizeof(event));
			return 1;
		}

		case kAsioOverload:
		{
			uint32_t event = eMessageQueueEventASIOOverload;
			mEventQueue->Write(&event, sizeof(event));
			return 1;
		}

		case kAsioResyncRequest:
		case kAsioLatenciesChanged:
		case kAsioSupportsTimeInfo:
			return 1;

		case kAsioEngineVersion:
			return 2;

	}

	return 0;
}

void SFB::Audio::ASIO::Player::FillASIOBuffer(long doubleBufferIndex)
{
	// Pre-rendering actions
	// Call the pre-render block
//	if(mRenderEventBlocks[0])
//		mRenderEventBlocks[0](ioData, inNumberFrames);

	// Mute output if requested
	if(eAudioPlayerFlagRequestMute & mFlags.load(std::memory_order_relaxed)) {
		mFlags.fetch_or(eAudioPlayerFlagMuteOutput, std::memory_order_relaxed);
		mFlags.fetch_and(~eAudioPlayerFlagRequestMute, std::memory_order_relaxed);

		mSemaphore.Signal();
	}


	// Rendering
	size_t frameCount = (size_t)sDriverInfo.mPreferredBufferSize;
	size_t framesAvailableToRead = mRingBuffer->GetFramesAvailableToRead();

	// Output silence if muted or the ring buffer is empty
	if(eAudioPlayerFlagMuteOutput & mFlags || 0 == framesAvailableToRead) {
		for(long bufferIndex = 0; bufferIndex < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++bufferIndex) {
			if(!sDriverInfo.mBufferInfo[bufferIndex].isInput)
				memset(sDriverInfo.mBufferInfo[bufferIndex].buffers[doubleBufferIndex], 0, mRingBufferFormat.FrameCountToByteCount(frameCount));
		}

		return;
	}

	for(long bufferIndex = 0, ablIndex = 0; bufferIndex < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++bufferIndex) {
		if(!sDriverInfo.mBufferInfo[bufferIndex].isInput) {
			sDriverInfo.mBufferList->mBuffers[ablIndex].mData = sDriverInfo.mBufferInfo[bufferIndex].buffers[doubleBufferIndex];
			sDriverInfo.mBufferList->mBuffers[ablIndex].mDataByteSize = (UInt32)mRingBufferFormat.FrameCountToByteCount(frameCount);
			sDriverInfo.mBufferList->mBuffers[ablIndex].mNumberChannels = 1;
			++ablIndex;
		}
	}

	// Restrict reads to valid decoded audio
	size_t framesToRead = std::min(framesAvailableToRead, frameCount);
	UInt32 framesRead = (UInt32)mRingBuffer->ReadAudio(sDriverInfo.mBufferList, framesToRead);
	if(framesRead != framesToRead) {
		LOGGER_ERR("org.sbooth.AudioEngine.ASIO.Player", "RingBuffer::ReadAudio failed: Requested " << framesToRead << " frames, got " << framesRead);
		return;
	}

	mFramesRendered.fetch_add(framesRead, std::memory_order_relaxed);

	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesRead != frameCount) {
		LOGGER_WARNING("org.sbooth.AudioEngine.ASIO.Player", "Insufficient audio in ring buffer: " << framesRead << " frames available, " << frameCount << " requested");

		size_t framesOfSilence = frameCount - framesRead;
		for(long bufferIndex = 0; bufferIndex < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++bufferIndex) {
			if(!sDriverInfo.mBufferInfo[bufferIndex].isInput) {
				size_t byteCountToSkip = mRingBufferFormat.FrameCountToByteCount(framesRead);
				size_t byteCountToZero = mRingBufferFormat.FrameCountToByteCount(framesOfSilence);
				memset((int8_t *)sDriverInfo.mBufferInfo[bufferIndex].buffers[doubleBufferIndex] + byteCountToSkip, 0, byteCountToZero);
			}
		}
	}

	// If the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if(sDriverInfo.mPostOutput)
		sASIO->outputReady();

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	size_t framesAvailableToWrite = mRingBuffer->GetFramesAvailableToWrite();
	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
		mDecoderSemaphore.Signal();


	// Post-rendering actions
	// Call the post-render block
//	if(mRenderEventBlocks[1])
//		mRenderEventBlocks[1](ioData, inNumberFrames);

	// There is nothing more to do if no frames were rendered
	if(0 == framesRead)
		return;

	// mFramesRenderedLastPass contains the number of valid frames that were rendered
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
		else {
			uint32_t event = eMessageQueueEventStopPlayback;
			mEventQueue->Write(&event, sizeof(event));
		}
	}
}
