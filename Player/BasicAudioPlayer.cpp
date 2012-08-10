/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <Accelerate/Accelerate.h>
#include <CoreServices/CoreServices.h>
#include <stdexcept>
#include <new>
#include <algorithm>
#include <iomanip>

#include "BasicAudioPlayer.h"
#include "DecoderStateData.h"
#include "Logger.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "ChannelLayoutsAreEqual.h"
#include "DeinterleavingFloatConverter.h"
#include "PCMConverter.h"
#include "CreateChannelLayout.h"

#include "CARingBuffer.h"

// ========================================
// Macros
// ========================================
#define RING_BUFFER_CAPACITY_FRAMES				16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6
#define SLEEP_TIME_USEC							1000

// ========================================
// Set the calling thread's timesharing and importance
// ========================================
static bool
setThreadPolicy(integer_t importance)
{
	// Turn off timesharing
	thread_extended_policy_data_t extendedPolicy = { 0 };
	kern_return_t error = thread_policy_set(mach_thread_self(),
											THREAD_EXTENDED_POLICY,
											(thread_policy_t)&extendedPolicy, 
											THREAD_EXTENDED_POLICY_COUNT);
	
	if(KERN_SUCCESS != error) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Couldn't set thread's extended policy: " << mach_error_string(error));
		return false;
	}
	
	// Give the thread the specified importance
	thread_precedence_policy_data_t precedencePolicy = { importance };
	error = thread_policy_set(mach_thread_self(), 
							  THREAD_PRECEDENCE_POLICY, 
							  (thread_policy_t)&precedencePolicy, 
							  THREAD_PRECEDENCE_POLICY_COUNT);
	
	if (error != KERN_SUCCESS) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Couldn't set thread's precedence policy: " << mach_error_string(error));
		return false;
	}
	
	return true;
}

// ========================================
// IOProc callbacks
// ========================================
static OSStatus 
myIOProc(AudioDeviceID				inDevice,
		 const AudioTimeStamp		*inNow,
		 const AudioBufferList		*inInputData,
		 const AudioTimeStamp		*inInputTime,
		 AudioBufferList			*outOutputData,
		 const AudioTimeStamp		*inOutputTime,
		 void						*inClientData)
{
	assert(nullptr != inClientData);

	BasicAudioPlayer *player = static_cast<BasicAudioPlayer *>(inClientData);
	return player->Render(inDevice, inNow, inInputData, inInputTime, outOutputData, inOutputTime);
}

static OSStatus
myAudioObjectPropertyListenerProc(AudioObjectID							inObjectID,
								  UInt32								inNumberAddresses,
								  const AudioObjectPropertyAddress		inAddresses[],
								  void									*inClientData)
{
	assert(nullptr != inClientData);
	
	BasicAudioPlayer *player = static_cast<BasicAudioPlayer *>(inClientData);
	return player->AudioObjectPropertyChanged(inObjectID, inNumberAddresses, inAddresses);
}

// ========================================
// The decoder thread's entry point
// ========================================
static void *
decoderEntry(void *arg)
{
	assert(nullptr != arg);
	
	BasicAudioPlayer *player = static_cast<BasicAudioPlayer *>(arg);
	return player->DecoderThreadEntry();
}

// ========================================
// The collector thread's entry point
// ========================================
static void *
collectorEntry(void *arg)
{
	assert(nullptr != arg);
	
	BasicAudioPlayer *player = static_cast<BasicAudioPlayer *>(arg);
	return player->CollectorThreadEntry();
}

// ========================================
// AudioConverter input callback
// ========================================
static OSStatus
mySampleRateConverterInputProc(AudioConverterRef				inAudioConverter,
							   UInt32							*ioNumberDataPackets,
							   AudioBufferList					*ioData,
							   AudioStreamPacketDescription		**outDataPacketDescription,
							   void								*inUserData)
{	
	assert(nullptr != inUserData);
	assert(nullptr != ioNumberDataPackets);
	
	BasicAudioPlayer *player = static_cast<BasicAudioPlayer *>(inUserData);	
	return player->FillSampleRateConversionBuffer(inAudioConverter, ioNumberDataPackets, ioData, outDataPacketDescription);
}


#pragma mark Creation/Destruction


BasicAudioPlayer::BasicAudioPlayer()
	: mOutputDeviceID(kAudioDeviceUnknown), mOutputDeviceIOProcID(nullptr), mOutputDeviceBufferFrameSize(0), mFlags(0), mDecoderQueue(nullptr), mRingBuffer(nullptr), mRingBufferChannelLayout(nullptr), mRingBufferCapacity(RING_BUFFER_CAPACITY_FRAMES), mRingBufferWriteChunkSize(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES), mOutputConverters(nullptr), mSampleRateConverter(nullptr), mSampleRateConversionBuffer(nullptr), mOutputBuffer(nullptr), mFramesDecoded(0), mFramesRendered(0), mDigitalVolume(1.0), mDigitalPreGain(1.0), mGuard(), mDecoderSemaphore(), mCollectorSemaphore()
{
	mDecoderQueue = CFArrayCreateMutable(kCFAllocatorDefault, 0, nullptr);
	
	if(nullptr == mDecoderQueue)
		throw std::bad_alloc();

	mRingBuffer = new CARingBuffer();

	// ========================================
	// Initialize the decoder array
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex)
		mActiveDecoders[bufferIndex] = nullptr;

	// ========================================
	// Launch the decoding thread
	mKeepDecoding = true;
	int creationResult = pthread_create(&mDecoderThread, nullptr, decoderEntry, this);
	if(0 != creationResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.BasicAudioPlayer", "pthread_create failed: " << strerror(creationResult));
		
		CFRelease(mDecoderQueue), mDecoderQueue = nullptr;
		delete mRingBuffer, mRingBuffer = nullptr;

		throw std::runtime_error("pthread_create failed");
	}
	
	// ========================================
	// Launch the collector thread
	mKeepCollecting = true;
	creationResult = pthread_create(&mCollectorThread, nullptr, collectorEntry, this);
	if(0 != creationResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.BasicAudioPlayer", "pthread_create failed: " << strerror(creationResult));
		
		mKeepDecoding = false;
		mDecoderSemaphore.Signal();
		
		int joinResult = pthread_join(mDecoderThread, nullptr);
		if(0 != joinResult)
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "pthread_join failed: " << strerror(joinResult));
		
		mDecoderThread = static_cast<pthread_t>(0);
		
		CFRelease(mDecoderQueue), mDecoderQueue = nullptr;
		delete mRingBuffer, mRingBuffer = nullptr;

		throw std::runtime_error("pthread_create failed");
	}
	
	// ========================================
	// The ring buffer will always contain deinterleaved 64-bit float audio
	mRingBufferFormat.mFormatID				= kAudioFormatLinearPCM;
	mRingBufferFormat.mFormatFlags			= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mRingBufferFormat.mSampleRate			= 0;
	mRingBufferFormat.mChannelsPerFrame		= 0;
	mRingBufferFormat.mBitsPerChannel		= 8 * sizeof(double);
	
	mRingBufferFormat.mBytesPerPacket		= (mRingBufferFormat.mBitsPerChannel / 8);
	mRingBufferFormat.mFramesPerPacket		= 1;
	mRingBufferFormat.mBytesPerFrame		= mRingBufferFormat.mBytesPerPacket * mRingBufferFormat.mFramesPerPacket;
	
	mRingBufferFormat.mReserved				= 0;

	// ========================================
	// Set up output
	
	// Use the default output device initially
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioHardwarePropertyDefaultOutputDevice, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(mOutputDeviceID);

    OSStatus hwResult = AudioObjectGetPropertyData(kAudioObjectSystemObject,
												   &propertyAddress,
												   0,
												   nullptr,
												   &dataSize,
												   &mOutputDeviceID);
	
	if(kAudioHardwareNoError != hwResult) {
		LOGGER_CRIT("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << hwResult);
		throw std::runtime_error("AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed");
	}

	if(!OpenOutput()) {
		LOGGER_CRIT("org.sbooth.AudioEngine.BasicAudioPlayer", "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

BasicAudioPlayer::~BasicAudioPlayer()
{
	Stop();

	// Stop the processing graph and reclaim its resources
	if(!CloseOutput())
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "CloseOutput() failed");

	// End the decoding thread
	mKeepDecoding = false;
	mDecoderSemaphore.Signal();

	int joinResult = pthread_join(mDecoderThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "pthread_join failed: " << strerror(joinResult));
	
	mDecoderThread = static_cast<pthread_t>(0);

	// End the collector thread
	mKeepCollecting = false;
	mCollectorSemaphore.Signal();
	
	joinResult = pthread_join(mCollectorThread, nullptr);
	if(0 != joinResult)
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "pthread_join failed: " << strerror(joinResult));
	
	mCollectorThread = static_cast<pthread_t>(0);

	// Force any decoders left hanging by the collector to end
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		if(nullptr != mActiveDecoders[bufferIndex])
			delete mActiveDecoders[bufferIndex], mActiveDecoders[bufferIndex] = nullptr;
	}
	
	// Clean up any queued decoders
	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>(const_cast<void *>(CFArrayGetValueAtIndex(mDecoderQueue, 0)));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}
	
	CFRelease(mDecoderQueue), mDecoderQueue = nullptr;

	// Clean up the ring buffer and associated resources
	if(mRingBuffer)
		delete mRingBuffer, mRingBuffer = nullptr;

	if(mRingBufferChannelLayout)
		free(mRingBufferChannelLayout), mRingBufferChannelLayout = nullptr;

	// Clean up the converters and conversion buffers
	if(mOutputConverters) {
		for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
			delete mOutputConverters[i], mOutputConverters[i] = nullptr;
		delete [] mOutputConverters, mOutputConverters = nullptr;
	}
	
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = nullptr;

		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterDispose failed: " << result);
	}

	if(mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);

	if(mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
}

#pragma mark Playback Control

bool BasicAudioPlayer::Play()
{
	Mutex::Locker lock(mGuard);

	if(!IsPlaying())
		return StartOutput();

	return true;
}

bool BasicAudioPlayer::Pause()
{
	Mutex::Locker lock(mGuard);

	if(IsPlaying())
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);

	return true;
}

bool BasicAudioPlayer::Stop()
{
	Guard::Locker lock(mGuard);

	if(IsPlaying()) {
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
		// Wait for output to stop
		lock.Wait();
	}

	StopActiveDecoders();
	
	ResetOutput();

	mFramesDecoded = 0;
	mFramesRendered = 0;

	return true;
}

BasicAudioPlayer::PlayerState BasicAudioPlayer::GetPlayerState() const
{
	if(eAudioPlayerFlagIsPlaying & mFlags)
		return BasicAudioPlayer::ePlaying;

	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return BasicAudioPlayer::eStopped;

	if(eDecoderStateDataFlagRenderingStarted & currentDecoderState->mFlags)
		return BasicAudioPlayer::ePaused;

	if(eDecoderStateDataFlagDecodingStarted & currentDecoderState->mFlags)
		return BasicAudioPlayer::ePending;

	return BasicAudioPlayer::eStopped;
}

CFURLRef BasicAudioPlayer::GetPlayingURL() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return nullptr;
	
	return currentDecoderState->mDecoder->GetURL();
}

#pragma mark Playback Properties

bool BasicAudioPlayer::GetCurrentFrame(SInt64& currentFrame) const
{
	SInt64 totalFrames;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool BasicAudioPlayer::GetTotalFrames(SInt64& totalFrames) const
{
	SInt64 currentFrame;
	return GetPlaybackPosition(currentFrame, totalFrames);
}

bool BasicAudioPlayer::GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	currentFrame	= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	totalFrames		= currentDecoderState->mTotalFrames;

	return true;
}

bool BasicAudioPlayer::GetCurrentTime(CFTimeInterval& currentTime) const
{
	CFTimeInterval totalTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool BasicAudioPlayer::GetTotalTime(CFTimeInterval& totalTime) const
{
	CFTimeInterval currentTime;
	return GetPlaybackTime(currentTime, totalTime);
}

bool BasicAudioPlayer::GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool BasicAudioPlayer::GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const
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

bool BasicAudioPlayer::SeekForward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);
	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 desiredFrame		= currentFrame + frameCount;
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::min(desiredFrame, totalFrames - 1));
}

bool BasicAudioPlayer::SeekBackward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 currentFrame		= (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
	SInt64 desiredFrame		= currentFrame - frameCount;
	
	return SeekToFrame(std::max(0LL, desiredFrame));
}

bool BasicAudioPlayer::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	SInt64 desiredFrame		= static_cast<SInt64>(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool BasicAudioPlayer::SeekToFrame(SInt64 frame)
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

bool BasicAudioPlayer::SupportsSeeking() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(nullptr == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}

#pragma mark Player Parameters

bool BasicAudioPlayer::GetVolume(double& volume) const
{
	volume = mDigitalVolume;
	return true;
}

bool BasicAudioPlayer::SetVolume(double volume)
{
	if(0 > volume || 1 < volume)
		return false;

	mDigitalVolume = std::min(1.0, std::max(0.0, volume));

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Digital volume set to " << mDigitalVolume);

	return true;
}

bool BasicAudioPlayer::GetPreGain(double& preGain) const
{
	preGain = mDigitalPreGain;
	return true;
}

bool BasicAudioPlayer::SetPreGain(double preGain)
{
	if(0 > preGain || 1 < preGain)
		return false;

	mDigitalPreGain = std::min(1.0, std::max(0.0, preGain));

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Digital pregain set to " << mDigitalPreGain);

	return true;
}

bool BasicAudioPlayer::SetSampleRateConverterQuality(UInt32 srcQuality)
{
	if(nullptr == mSampleRateConverter)
		return false;

	Guard::Locker lock(mGuard);

	bool restartIO = IsPlaying();
	if(restartIO) {
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
		// Wait for output to stop
		lock.Wait();
	}

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting sample rate converter quality to " << srcQuality);

	OSStatus result = AudioConverterSetProperty(mSampleRateConverter, 
												kAudioConverterSampleRateConverterQuality, 
												sizeof(srcQuality), 
												&srcQuality);

	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterSetProperty (kAudioConverterSampleRateConverterQuality) failed: " << result);
		return false;
	}

	if(!ReallocateSampleRateConversionBuffer())
		return false;

	if(restartIO)
		return StartOutput();

	return true;
}

bool BasicAudioPlayer::SetSampleRateConverterComplexity(OSType srcComplexity)
{
	if(nullptr == mSampleRateConverter)
		return false;

	Guard::Locker lock(mGuard);

	bool restartIO = IsPlaying();
	if(restartIO) {
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
		// Wait for output to stop
		lock.Wait();
	}

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting sample rate converter complexity to " << srcComplexity);

	OSStatus result = AudioConverterSetProperty(mSampleRateConverter, 
												kAudioConverterSampleRateConverterComplexity, 
												sizeof(srcComplexity), 
												&srcComplexity);

	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterSetProperty (kAudioConverterSampleRateConverterComplexity) failed: " << result);
		return false;
	}

	if(!ReallocateSampleRateConversionBuffer())
		return false;

	if(restartIO)
		return StartOutput();

	return true;
}

#pragma mark Hog Mode

bool BasicAudioPlayer::OutputDeviceIsHogged() const
{
	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool BasicAudioPlayer::StartHoggingOutputDevice()
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Taking hog mode for device 0x" << std::hex << mOutputDeviceID);

	// Is it hogged already?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}
	
	// The device is already hogged
	if(hogPID != static_cast<pid_t>(-1)) {
		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Device is already hogged by pid: " << hogPID);
		return false;
	}

	bool restartIO = false;
	{
		Guard::Locker lock(mGuard);

		// If IO is enabled, disable it while hog mode is acquired because the HAL
		// does not automatically restart IO after hog mode is taken
		restartIO = IsPlaying();
		if(restartIO) {
			OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
			// Wait for output to stop
			lock.Wait();
		}
	}

	hogPID = getpid();
	
	result = AudioObjectSetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										nullptr, 
										sizeof(hogPID), 
										&hogPID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(restartIO && !OutputIsRunning())
		StartOutput();

	return true;
}

bool BasicAudioPlayer::StopHoggingOutputDevice()
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Releasing hog mode for device 0x" << std::hex << mOutputDeviceID);

	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyHogMode, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}
	
	// If we don't own hog mode we can't release it
	if(hogPID != getpid())
		return false;

	bool restartIO = false;
	{
		Guard::Locker lock(mGuard);

		// Disable IO while hog mode is released
		restartIO = IsPlaying();
		if(restartIO) {
			OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
			// Wait for output to stop
			lock.Wait();
		}
	}

	// Release hog mode.
	hogPID = static_cast<pid_t>(-1);

	result = AudioObjectSetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										nullptr, 
										sizeof(hogPID), 
										&hogPID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}
	
	if(restartIO && !OutputIsRunning())
		StartOutput();
	
	return true;
}

#pragma mark Device Parameters

bool BasicAudioPlayer::GetDeviceMasterVolume(Float32& volume) const
{
	return GetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool BasicAudioPlayer::SetDeviceMasterVolume(Float32 volume)
{
	return SetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool BasicAudioPlayer::GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyVolumeScalar, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= channel 
	};

	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize, &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool BasicAudioPlayer::SetDeviceVolumeForChannel(UInt32 channel, Float32 volume)
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting output device 0x" << std::hex << mOutputDeviceID << " channel " << channel << " volume to " << volume);

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyVolumeScalar, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= channel 
	};

	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, sizeof(volume), &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool BasicAudioPlayer::GetDeviceChannelCount(UInt32& channelCount) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyStreamConfiguration, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) is false");
		return false;
	}

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);

	if(nullptr == bufferList) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Unable to allocate << " << dataSize << " bytes");
		return false;
	}

	result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize, bufferList);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		free(bufferList), bufferList = nullptr;
		return false;
	}

	channelCount = 0;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;

	free(bufferList), bufferList = nullptr;
	return true;
}

bool BasicAudioPlayer::GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyPreferredChannelsForStereo, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed is false");
		return false;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize, &preferredChannels);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	preferredStereoChannels.first = preferredChannels[0];
	preferredStereoChannels.second = preferredChannels[1];

	return true;
}

#pragma mark Device Management

bool BasicAudioPlayer::CreateOutputDeviceUID(CFStringRef& deviceUID) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyDeviceUID, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(deviceUID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &deviceUID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::SetOutputDeviceUID(CFStringRef deviceUID)
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting output device UID to " << deviceUID);

	AudioDeviceID		deviceID		= kAudioDeviceUnknown;
	UInt32				specifierSize	= 0;

	// If nullptr was passed as the device UID, use the default output device
	if(nullptr == deviceUID) {
		AudioObjectPropertyAddress propertyAddress = { 
			.mSelector	= kAudioHardwarePropertyDefaultOutputDevice, 
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster 
		};
		
		specifierSize = sizeof(deviceID);
		
		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
													 &propertyAddress,
													 0,
													 nullptr,
													 &specifierSize,
													 &deviceID);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << result);
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
		
		specifierSize = sizeof(translation);
		
		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
													 &propertyAddress,
													 0,
													 nullptr,
													 &specifierSize,
													 &translation);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: " << result);
			return false;
		}
	}
	
	// The device isn't connected or doesn't exist
	if(kAudioDeviceUnknown == deviceID)
		return false;

	return SetOutputDeviceID(deviceID);
}

bool BasicAudioPlayer::GetOutputDeviceID(AudioDeviceID& deviceID) const
{
	deviceID = mOutputDeviceID;
	return true;
}

bool BasicAudioPlayer::SetOutputDeviceID(AudioDeviceID deviceID)
{
	if(kAudioDeviceUnknown == deviceID)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting output device ID to 0x" << std::hex << deviceID);
	
	if(deviceID == mOutputDeviceID)
		return true;

	if(!CloseOutput())
		return false;
	
	mOutputDeviceID = deviceID;
	
	if(!OpenOutput())
		return false;
	
	return true;
}

bool BasicAudioPlayer::GetOutputDeviceSampleRate(Float64& deviceSampleRate) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyNominalSampleRate, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(deviceSampleRate);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &deviceSampleRate);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::SetOutputDeviceSampleRate(Float64 deviceSampleRate)
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting device 0x" << std::hex << mOutputDeviceID << " sample rate to " << deviceSampleRate << " Hz");

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyNominalSampleRate, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 sizeof(deviceSampleRate),
												 &deviceSampleRate);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

#pragma mark Stream Management

bool BasicAudioPlayer::GetOutputStreams(std::vector<AudioStreamID>& streams) const
{
	streams.clear();

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyStreams, 
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(mOutputDeviceID, 
													 &propertyAddress, 
													 0,
													 nullptr,
													 &dataSize);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: " << result);
		return false;
	}
	
	UInt32 streamCount = static_cast<UInt32>(dataSize / sizeof(AudioStreamID));
	AudioStreamID audioStreamIDs [streamCount];
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										nullptr, 
										&dataSize, 
										audioStreamIDs);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: " << result);
		return false;
	}

	streams.reserve(streamCount);
	for(UInt32 i = 0; i < streamCount; ++i)
		streams.push_back(audioStreamIDs[i]);

	return true;
}

bool BasicAudioPlayer::GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat) const
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}

	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioStreamPropertyVirtualFormat, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(virtualFormat);
	
	OSStatus result = AudioObjectGetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &virtualFormat);	
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
		return false;
	}
	
	return true;	
}

bool BasicAudioPlayer::SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat)
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting stream 0x" << std::hex << streamID << " virtual format to: " << virtualFormat);

	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioStreamPropertyVirtualFormat, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 sizeof(virtualFormat),
												 &virtualFormat);	
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat) const
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioStreamPropertyPhysicalFormat, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(physicalFormat);
	
	OSStatus result = AudioObjectGetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &physicalFormat);	
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat)
{
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting stream 0x" << std::hex << streamID << " physical format to: " << physicalFormat);

	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioStreamPropertyPhysicalFormat, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 sizeof(physicalFormat),
												 &physicalFormat);	
	
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
		return false;
	}
	
	return true;
}

#pragma mark Playlist Management

bool BasicAudioPlayer::Enqueue(CFURLRef url)
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

bool BasicAudioPlayer::Enqueue(AudioDecoder *decoder)
{
	if(nullptr == decoder)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Enqueuing \"" << decoder->GetURL() << "\"");

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
		if(mRingBufferChannelLayout)
			free(mRingBufferChannelLayout), mRingBufferChannelLayout = nullptr;

		// Open the decoder if necessary
		CFErrorRef error = nullptr;
		if(!decoder->IsOpen() && !decoder->Open(&error)) {
			if(error) {
				LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error opening decoder: " << error);
				CFRelease(error), error = nullptr;
			}

			return false;
		}

		AudioStreamBasicDescription format = decoder->GetFormat();

		// The ring buffer contains deinterleaved floats at the decoder's sample rate and channel layout
		mRingBufferFormat.mSampleRate			= format.mSampleRate;
		mRingBufferFormat.mChannelsPerFrame		= format.mChannelsPerFrame;
		mRingBufferChannelLayout				= CopyChannelLayout(decoder->GetChannelLayout());

		// Assign a default channel layout to the ring buffer if the decoder has an unknown layout
		if(nullptr == mRingBufferChannelLayout)
			mRingBufferChannelLayout = CreateDefaultAudioChannelLayout(mRingBufferFormat.mChannelsPerFrame);

		if(!CreateConvertersAndSRCBuffer()) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "CreateConvertersAndSRCBuffer failed");
			return false;
		}

		// Allocate enough space in the ring buffer for the new format
		mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame, mRingBufferFormat.mBytesPerFrame, mRingBufferCapacity);
	}
	// Otherwise, enqueue this decoder if the format matches
	else if(decoder->IsOpen()) {
		AudioStreamBasicDescription		nextFormat			= decoder->GetFormat();
		AudioChannelLayout				*nextChannelLayout	= decoder->GetChannelLayout();
		
		// The two files can be joined seamlessly only if they have the same sample rates and channel counts
		if(nextFormat.mSampleRate != mRingBufferFormat.mSampleRate) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Enqueue failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
			return false;
		}
		else if(nextFormat.mChannelsPerFrame != mRingBufferFormat.mChannelsPerFrame) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Enqueue failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
			return false;
		}

		// If the decoder has an explicit channel layout, enqueue it if it matches the ring buffer's channel layout
		if(nullptr != nextChannelLayout && !ChannelLayoutsAreEqual(nextChannelLayout, mRingBufferChannelLayout)) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Enqueue failed: Ring buffer channel layout (" << mRingBufferChannelLayout << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
			return false;
		}
		// If the decoder doesn't have an explicit channel layout, enqueue it if the default layout matches
		else if(nullptr == nextChannelLayout) {
			AudioChannelLayout *defaultLayout = CreateDefaultAudioChannelLayout(nextFormat.mChannelsPerFrame);
			bool layoutsMatch = ChannelLayoutsAreEqual(defaultLayout, mRingBufferChannelLayout);
			free(defaultLayout), defaultLayout = nullptr;

			if(!layoutsMatch) {
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Enqueue failed: Decoder has no channel layout and ring buffer channel layout (" << mRingBufferChannelLayout << ") isn't the default for " << nextFormat.mChannelsPerFrame << " channels");
				return false;
			}
		}
	}
	// If the decoder isn't open the format isn't yet known.  Enqueue it and hope things work out for the best
	
	// Add the decoder to the queue
	CFArrayAppendValue(mDecoderQueue, decoder);

	mDecoderSemaphore.Signal();
	
	return true;
}

bool BasicAudioPlayer::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(nullptr == currentDecoderState)
		return false;

	OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &currentDecoderState->mFlags);

	// Signal the decoding thread that decoding is finished (inner loop)
	mDecoderSemaphore.Signal();

	// Wait for decoding to finish or a SIGSEGV could occur if the collector collects an active decoder
	while(!(eDecoderStateDataFlagDecodingFinished & currentDecoderState->mFlags)) {
		int result = usleep(SLEEP_TIME_USEC);
		if(0 != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Couldn't wait for decoding to finish: " << strerror(errno));
		}
	}

	OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &currentDecoderState->mFlags);

	// Effect a flush of the ring buffer
	mFramesDecoded = 0;
	mFramesRendered = 0;
	
	// Signal the decoding thread to start the next decoder (outer loop)
	mDecoderSemaphore.Signal();

	OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	return true;
}

bool BasicAudioPlayer::ClearQueuedDecoders()
{
	Mutex::Tryer lock(mGuard);
	if(!lock)
		return false;

	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>(const_cast<void *>(CFArrayGetValueAtIndex(mDecoderQueue, 0)));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}

	return true;	
}

#pragma mark Ring Buffer Parameters

bool BasicAudioPlayer::SetRingBufferCapacity(uint32_t bufferCapacity)
{
	if(0 == bufferCapacity || mRingBufferWriteChunkSize > bufferCapacity)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting ring buffer capacity to " << bufferCapacity);

	return OSAtomicCompareAndSwap32Barrier(mRingBufferCapacity, bufferCapacity, reinterpret_cast<int32_t *>(&mRingBufferCapacity));
}

bool BasicAudioPlayer::SetRingBufferWriteChunkSize(uint32_t chunkSize)
{
	if(0 == chunkSize || mRingBufferCapacity < chunkSize)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Setting ring buffer write chunk size to " << chunkSize);

	return OSAtomicCompareAndSwap32Barrier(mRingBufferWriteChunkSize, chunkSize, reinterpret_cast<int32_t *>(&mRingBufferWriteChunkSize));
}

#pragma mark IOProc

OSStatus BasicAudioPlayer::Render(AudioDeviceID			inDevice,
							 const AudioTimeStamp	*inNow,
							 const AudioBufferList	*inInputData,
							 const AudioTimeStamp	*inInputTime,
							 AudioBufferList		*outOutputData,
							 const AudioTimeStamp	*inOutputTime)
{

#pragma unused(inNow)
#pragma unused(inInputData)
#pragma unused(inInputTime)
#pragma unused(inOutputTime)

	assert(inDevice == mOutputDeviceID);
	assert(nullptr != outOutputData);

	// ========================================
	// RENDERING

	// Stop output if requested
	if(eAudioPlayerFlagStopRequested & mFlags) {
		OSAtomicTestAndClearBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
		StopOutput();
		return kAudioHardwareNoError;
	}

	// Reset output, if requested
	if(eAudioPlayerFlagResetNeeded & mFlags) {
		OSAtomicTestAndClearBarrier(2 /* eAudioPlayerFlagResetNeeded */, &mFlags);
		ResetOutput();
	}

	// Mute functionality
	if(eAudioPlayerFlagMuteOutput & mFlags)
		return kAudioHardwareNoError;

	// If the ring buffer doesn't contain any valid audio, skip some work
	if(mFramesDecoded == mFramesRendered) {
		DecoderStateData *decoderState = GetCurrentDecoderState();

		// If there is a valid decoder but the ring buffer is empty, verify that the rendering finished callbacks
		// were performed.  It is possible that decoding is actually finished, but that the last time we checked was in between
		// the time decoderState->mFramesDecoded was updated and the time eDecoderStateDataFlagDecodingFinished was set
		// so the callback wasn't performed
		if(decoderState) {
			
			// mActiveDecoders is not an ordered array, so to ensure that callbacks are performed
			// in the proper order multiple passes are made here
			while(nullptr != decoderState) {
				SInt64 timeStamp = decoderState->mTimeStamp;

				if((eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) && decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)*/) {
					decoderState->mDecoder->PerformRenderingFinishedCallback();			
					
					OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
					decoderState = nullptr;
					
					// Since rendering is finished, signal the collector to clean up this decoder
					mCollectorSemaphore.Signal();
				}

				decoderState = GetDecoderStateStartingAfterTimeStamp(timeStamp);
			}
		}
		// If there are no decoders in the queue, stop IO
		else
			StopOutput();

		return kAudioHardwareNoError;
	}

	// Reset state
	mFramesRenderedLastPass = 0;

	// The format of mOutputBuffer is the same as mRingBufferFormat except possibly mSampleRate
	for(UInt32 i = 0; i < mOutputBuffer->mNumberBuffers; ++i)
		mOutputBuffer->mBuffers[i].mDataByteSize = mRingBufferFormat.mBytesPerFrame * mOutputDeviceBufferFrameSize;

	// The number of frames to read, at the output device's sample rate
	UInt32 framesToRead = 0;
	
	// Convert to the stream's sample rate, if required
	if(mSampleRateConverter) {
		// The number of frames read will be limited to valid decoded frames in the converter callback
		framesToRead = mOutputDeviceBufferFrameSize;

		OSStatus result = AudioConverterFillComplexBuffer(mSampleRateConverter, 
														  mySampleRateConverterInputProc,
														  this,
														  &framesToRead, 
														  mOutputBuffer,
														  nullptr);
		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterFillComplexBuffer failed: " << result);
			return result;
		}
	}
	// Otherwise fetch the output from the ring buffer
	else {
		UInt32 framesAvailableToRead = static_cast<UInt32>(mFramesDecoded - mFramesRendered);
		framesToRead = std::min(framesAvailableToRead, mOutputDeviceBufferFrameSize);

		if(framesToRead != mOutputDeviceBufferFrameSize) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Insufficient audio in ring buffer: " << framesToRead << " frames available, " << mOutputDeviceBufferFrameSize << " requested");

			// TODO: Perform AudioBufferRanDry() callback ??
		}

		CARingBufferError result = mRingBuffer->Fetch(mOutputBuffer, framesToRead, mFramesRendered);
		
		if(kCARingBufferError_OK != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "CARingBuffer::Fetch failed: " << result << ", requested " << framesToRead << " frames from " << mFramesRendered);
			return ioErr;
		}

		OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
		
		mFramesRenderedLastPass += framesToRead;
	}

	// Apply digital volume
	if(1 != mDigitalVolume) {
		for(UInt32 bufferIndex = 0; bufferIndex < mOutputBuffer->mNumberBuffers; ++bufferIndex) {
			double *buffer = static_cast<double *>(mOutputBuffer->mBuffers[bufferIndex].mData);
			vDSP_vsmulD(buffer, 1, &mDigitalVolume, buffer, 1, framesToRead);
		}
	}

	// Iterate through each stream and render output in the stream's format
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(nullptr == mOutputConverters[i])
			continue;

		// Convert to the output device's format
		UInt32 framesConverted = mOutputConverters[i]->Convert(mOutputBuffer, outOutputData, framesToRead);
		
		if(framesConverted != framesToRead) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Conversion to output format failed; all frames may not be rendered");
		}
	}

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = static_cast<UInt32>(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));

	if(mRingBufferWriteChunkSize <= framesAvailableToWrite)
		mDecoderSemaphore.Signal();

	// ========================================
	// POST-RENDERING HOUSEKEEPING
	
	// There is nothing more to do if no frames were rendered
	if(0 == mFramesRenderedLastPass)
		return kAudioHardwareNoError;
	
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
		SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, static_cast<SInt64>(mFramesRenderedLastPass));
		
		if(0 == decoderState->mFramesRendered && !(eDecoderStateDataFlagRenderingStarted & decoderState->mFlags)) {
			OSAtomicTestAndSetBarrier(5 /* eDecoderStateDataFlagRenderingStarted */, &decoderState->mFlags);
			decoderState->mDecoder->PerformRenderingStartedCallback();
		}
		
		OSAtomicAdd64Barrier(framesFromThisDecoder, &decoderState->mFramesRendered);
		
		if((eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) && decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)*/) {
			decoderState->mDecoder->PerformRenderingFinishedCallback();			

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
	
	return kAudioHardwareNoError;
}

OSStatus BasicAudioPlayer::AudioObjectPropertyChanged(AudioObjectID						inObjectID,
												 UInt32								inNumberAddresses,
												 const AudioObjectPropertyAddress	inAddresses[])
{
	// The HAL automatically stops output before this is called, and restarts output afterward if necessary

	// ========================================
	// AudioDevice properties
	if(inObjectID == mOutputDeviceID) {
		for(UInt32 addressIndex = 0; addressIndex < inNumberAddresses; ++addressIndex) {
			AudioObjectPropertyAddress currentAddress = inAddresses[addressIndex];

			switch(currentAddress.mSelector) {
				case kAudioDevicePropertyDeviceIsRunning:
				{
					UInt32 isRunning = 0;
					UInt32 dataSize = sizeof(isRunning);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 nullptr, 
																 &dataSize,
																 &isRunning);
					
					if(kAudioHardwareNoError != result) {
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: " << result);
						continue;
					}

					if(isRunning)
						OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagIsPlaying */, &mFlags);
					else {
						OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagIsPlaying */, &mFlags);
						mGuard.Signal();
					}

					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioDevicePropertyDeviceIsRunning [0x" << std::hex << inObjectID << "]: " << (isRunning ? "True" : "False"));

					break;
				}

				case kAudioDevicePropertyNominalSampleRate:
				{
					Float64 deviceSampleRate = 0;
					UInt32 dataSize = sizeof(deviceSampleRate);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 nullptr, 
																 &dataSize,
																 &deviceSampleRate);
					
					if(kAudioHardwareNoError != result) {
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
						continue;
					}
					
					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioDevicePropertyNominalSampleRate [0x" << std::hex << inObjectID << "]: " << deviceSampleRate << " Hz");
					
					break;
				}

				case kAudioDevicePropertyStreams:
				{
					Guard::Locker lock(mGuard);

					bool restartIO = OutputIsRunning();
					if(restartIO) {
						OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
						// Wait for output to stop
						lock.Wait();
					}

					// Stop observing properties on the defunct streams
					if(!RemoveVirtualFormatPropertyListeners())
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "RemoveVirtualFormatPropertyListeners failed");

					for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
						if(nullptr != mOutputConverters[i])
							delete mOutputConverters[i], mOutputConverters[i] = nullptr;
					}

					delete [] mOutputConverters, mOutputConverters = nullptr;

					mOutputDeviceStreamIDs.clear();

					// Update our list of cached streams
					if(!GetOutputStreams(mOutputDeviceStreamIDs)) 
						continue;

					// Observe the new streams for changes
					if(!AddVirtualFormatPropertyListeners())
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AddVirtualFormatPropertyListeners failed");

					mOutputConverters = new PCMConverter * [mOutputDeviceStreamIDs.size()];
					for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
						mOutputConverters[i] = nullptr;

					if(!CreateConvertersAndSRCBuffer())
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "CreateConvertersAndSRCBuffer failed");

					if(restartIO)
						StartOutput();

					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioDevicePropertyStreams [0x" << std::hex << inObjectID << "]");

					break;
				}

				case kAudioDevicePropertyBufferFrameSize:
				{
					Guard::Locker lock(mGuard);
					
					bool restartIO = OutputIsRunning();
					if(restartIO) {
						OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
						// Wait for output to stop
						lock.Wait();
					}

					// Clean up
					if(mSampleRateConversionBuffer)
						mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);

					if(mOutputBuffer)
						mOutputBuffer = DeallocateABL(mOutputBuffer);

					// Get the new buffer size
					UInt32 dataSize = sizeof(mOutputDeviceBufferFrameSize);

					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 nullptr, 
																 &dataSize,
																 &mOutputDeviceBufferFrameSize);

					if(kAudioHardwareNoError != result) {
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: " << result);
						continue;
					}

					AudioStreamBasicDescription outputBufferFormat = mRingBufferFormat;

					// Recalculate the sample rate conversion buffer size
					if(mSampleRateConverter && !ReallocateSampleRateConversionBuffer())
						continue;

					// Allocate the output buffer (data is at the device's sample rate)
					mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);

					if(restartIO)
						StartOutput();

					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioDevicePropertyBufferFrameSize [0x" << std::hex << inObjectID << "]: " << mOutputDeviceBufferFrameSize);

					break;
				}

				case kAudioDeviceProcessorOverload:
					LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioDeviceProcessorOverload [0x" << std::hex << inObjectID << "]: Unable to meet IOProc time constraints");
					break;
			}
			
		}
	}
	// ========================================
	// AudioStream properties
	else if(mOutputDeviceStreamIDs.end() != std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), inObjectID)) {
		for(UInt32 addressIndex = 0; addressIndex < inNumberAddresses; ++addressIndex) {
			AudioObjectPropertyAddress currentAddress = inAddresses[addressIndex];
			
			switch(currentAddress.mSelector) {
				case kAudioStreamPropertyVirtualFormat:
				{
					Guard::Locker lock(mGuard);

					bool restartIO = OutputIsRunning();
					if(restartIO) {
						OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
						// Wait for output to stop
						lock.Wait();
					}

					// Get the new virtual format
					AudioStreamBasicDescription virtualFormat;
					UInt32 dataSize = sizeof(virtualFormat);

					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 nullptr, 
																 &dataSize,
																 &virtualFormat);

					if(kAudioHardwareNoError != result) {
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
						continue;
					}

					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioStreamPropertyVirtualFormat [0x" << std::hex << inObjectID << "]: " << virtualFormat);

					if(!CreateConvertersAndSRCBuffer())
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "CreateConvertersAndSRCBuffer failed");

					if(restartIO)
						StartOutput();

					break;
				}

				case kAudioStreamPropertyPhysicalFormat:
				{
					// Get the new physical format
					AudioStreamBasicDescription physicalFormat;
					UInt32 dataSize = sizeof(physicalFormat);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 nullptr, 
																 &dataSize,
																 &physicalFormat);
					
					if(kAudioHardwareNoError != result) {
						LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
						continue;
					}

					LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "-> kAudioStreamPropertyPhysicalFormat [0x" << std::hex << inObjectID << "]: " << physicalFormat);

					break;
				}
			}
		}
	}
	
	return kAudioHardwareNoError;			
}

OSStatus BasicAudioPlayer::FillSampleRateConversionBuffer(AudioConverterRef				inAudioConverter,
													 UInt32							*ioNumberDataPackets,
													 AudioBufferList				*ioData,
													 AudioStreamPacketDescription	**outDataPacketDescription)
{
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

	UInt32 framesAvailableToRead = static_cast<UInt32>(mFramesDecoded - mFramesRendered);

	// Nothing to read
	if(0 == framesAvailableToRead) {
		*ioNumberDataPackets = 0;
		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min(framesAvailableToRead, *ioNumberDataPackets);

	CARingBufferError result = mRingBuffer->Fetch(mSampleRateConversionBuffer, framesToRead, mFramesRendered);
	
	if(kCARingBufferError_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "CARingBuffer::Fetch failed: " << result << ", requested " << framesToRead << " frames from " << mFramesRendered);
		*ioNumberDataPackets = 0;
		return ioErr;
	}
	
	OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
	
	// This may be called multiple times from AudioConverterFillComplexBuffer, so keep an additive tally
	// of how many frames were rendered
	mFramesRenderedLastPass += framesToRead;
	
	// Point ioData at our converted audio
	ioData->mNumberBuffers = mSampleRateConversionBuffer->mNumberBuffers;
	for(UInt32 bufferIndex = 0; bufferIndex < mSampleRateConversionBuffer->mNumberBuffers; ++bufferIndex)
		ioData->mBuffers[bufferIndex] = mSampleRateConversionBuffer->mBuffers[bufferIndex];
	
	*ioNumberDataPackets = framesToRead;
	
	return noErr;
}

#pragma mark Thread Entry Points

void * BasicAudioPlayer::DecoderThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Decoder");

	// ========================================
	// Make ourselves a high priority thread
	if(!setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Couldn't set decoder thread importance");
	
	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };

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
					LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error opening decoder: " << error);
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
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Gapless join failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
				formatsMatch = false;
			}
			else if(nextFormat.mChannelsPerFrame != mRingBufferFormat.mChannelsPerFrame) {
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Gapless join failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
				formatsMatch = false;
			}

			// If the decoder has an explicit channel layout, enqueue it if it matches the ring buffer's channel layout
			if(nextChannelLayout && !ChannelLayoutsAreEqual(nextChannelLayout, mRingBufferChannelLayout)) {
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Gapless join failed: Ring buffer channel layout (" << mRingBufferChannelLayout << ") and decoder channel layout (" << nextChannelLayout << ") don't match");
				formatsMatch = false;
			}
			// If the decoder doesn't have an explicit channel layout, enqueue it if the default layout matches
			else if(nullptr == nextChannelLayout) {
				AudioChannelLayout *defaultLayout = CreateDefaultAudioChannelLayout(nextFormat.mChannelsPerFrame);
				bool layoutsMatch = ChannelLayoutsAreEqual(defaultLayout, mRingBufferChannelLayout);
				free(defaultLayout), defaultLayout = nullptr;

				if(!layoutsMatch) {
					LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Gapless join failed: Decoder has no channel layout and ring buffer channel layout (" << mRingBufferChannelLayout << ") isn't the default for " << nextFormat.mChannelsPerFrame << " channels");
					formatsMatch = false;
				}
			}

			// If the formats don't match, the decoder can't be used with the current ring buffer format
			if(!formatsMatch)
				delete decoderState, decoderState = nullptr;
		}

		// ========================================
		// Append the decoder state to the list of active decoders
		if(decoderState) {
			for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
				if(nullptr != mActiveDecoders[bufferIndex])
					continue;
				
				if(OSAtomicCompareAndSwapPtrBarrier(nullptr, decoderState, reinterpret_cast<void **>(&mActiveDecoders[bufferIndex])))
					break;
				else
					LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "OSAtomicCompareAndSwapPtrBarrier() failed");
			}
		}
		
		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(decoderState) {
			AudioDecoder *decoder = decoderState->mDecoder;

			LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Decoding starting for \"" << decoder->GetURL() << "\"");
			LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Decoder format: " << decoder->GetFormat());
			LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Decoder channel layout: " << decoder->GetChannelLayout());
			
			SInt64 startTime = decoderState->mTimeStamp;

			// ========================================
			// Create the deinterleaver that will convert from the decoder's format to deinterleaved, normalized 64-bit floats
			DeinterleavingFloatConverter *converter = nullptr;
			try {
				converter = new DeinterleavingFloatConverter(decoder->GetFormat());
			}
			
			catch(const std::exception& e) {
				LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error creating DeinterleavingFloatConverter: " << e.what());
				OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &decoderState->mFlags);
			}

			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer			
			decoderState->AllocateBufferList(mRingBufferWriteChunkSize);

			AudioBufferList *bufferList = AllocateABL(mRingBufferFormat, mRingBufferWriteChunkSize);
			
			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(mKeepDecoding && decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags)) {

				// Fill the ring buffer with as much data as possible
				for(;;) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = static_cast<UInt32>(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));
					
					// Force writes to the ring buffer to be at least mRingBufferWriteChunkSize
					if(mRingBufferWriteChunkSize <= framesAvailableToWrite) {
						
						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Seeking to frame " << decoderState->mFrameToSeek);

							OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
							
							SInt64 currentFrameBeforeSeeking = decoder->GetCurrentFrame();
							
							SInt64 newFrame = decoder->SeekToFrame(decoderState->mFrameToSeek);
							
							if(newFrame != decoderState->mFrameToSeek)
								LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error seeking to frame  " << decoderState->mFrameToSeek);
							
							// Update the seek request
							if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFrameToSeek, -1, &decoderState->mFrameToSeek))
								LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");
							
							// If the seek failed do not update the counters
							if(-1 != newFrame) {
								SInt64 framesSkipped = newFrame - currentFrameBeforeSeeking;
								
								// Treat the skipped frames as if they were rendered, and update the counters accordingly
								if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFramesRendered, newFrame, &decoderState->mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");
								
								OSAtomicAdd64Barrier(framesSkipped, &mFramesDecoded);
								if(!OSAtomicCompareAndSwap64Barrier(mFramesRendered, mFramesDecoded, &mFramesRendered))
									LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "OSAtomicCompareAndSwap64Barrier() failed ");

								// If sample rate conversion is being performed, ResetOutput() needs to be called to flush any
								// state the AudioConverter may have.  In the future, if ResetOutput() does anything other than
								// reset the AudioConverter state the if(mSampleRateConverter) will need to be removed
								if(mSampleRateConverter) {
									// ResetOutput() is not safe to call when the device is running, because the player
									// could be in the middle of a render callback
									if(OutputIsRunning())
										OSAtomicTestAndSetBarrier(2 /* eAudioPlayerFlagResetNeeded */, &mFlags);
									// Even if the device isn't running, AudioConverters are not thread-safe
									else {
										Mutex::Locker lock(mGuard);
										ResetOutput();
									}
								}
							}

							OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
						}
						
						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Unable to determine starting frame number ");
							break;
						}

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber && !(eDecoderStateDataFlagDecodingStarted & decoderState->mFlags)) {
							OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingStarted */, &decoderState->mFlags);
							decoder->PerformDecodingStartedCallback();
						}

						// Read the input chunk
						UInt32 framesDecoded = decoderState->ReadAudio(mRingBufferWriteChunkSize);
						
						// Convert and store the decoded audio
						if(0 != framesDecoded) {
							UInt32 framesConverted = 0;
							try {
								framesConverted = converter->Convert(decoderState->mBufferList, bufferList, framesDecoded);
							}

							catch(const std::exception& e) {
								LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error converting input to float: " << e.what());
							}
							
							if(framesConverted != framesDecoded)
								LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Incomplete conversion:  " << framesConverted <<  "/" << framesDecoded << " frames");

							// Apply digital pre-gain
							if(1 != mDigitalPreGain) {
								for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
									double *buffer = static_cast<double *>(bufferList->mBuffers[bufferIndex].mData);
									vDSP_vsmulD(buffer, 1, &mDigitalPreGain, buffer, 1, framesConverted);
								}
							}

							CARingBufferError result = mRingBuffer->Store(bufferList, 
																		  framesConverted, 
																		  startingFrameNumber + startTime);
							
							if(kCARingBufferError_OK != result)
								LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "CARingBuffer::Store failed: " << result);

							OSAtomicAdd64Barrier(framesConverted, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)*/) {
							LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Decoding finished for \"" << decoder->GetURL() << "\"");

							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update 
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							decoder->PerformDecodingFinishedCallback();
							
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
			}

			if(bufferList)
				DeallocateABL(bufferList), bufferList = nullptr;
			
			if(converter)
				delete converter, converter = nullptr;
		}

		// Wait for another thread to wake us, or for the timeout to happen
		mDecoderSemaphore.TimedWait(timeout);
	}
	
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Decoding thread terminating");

	return nullptr;
}

void * BasicAudioPlayer::CollectorThreadEntry()
{
	pthread_setname_np("org.sbooth.AudioEngine.Collector");

	// The collector should be signaled when there is cleanup to be done, so there is no need for a short timeout
	mach_timespec_t timeout = { 30, 0 };

	while(mKeepCollecting) {
		
		for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
			DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
			
			if(nullptr == decoderState)
				continue;

			if(!(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) || !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags))
				continue;

			bool swapSucceeded = OSAtomicCompareAndSwapPtrBarrier(decoderState, nullptr, reinterpret_cast<void **>(&mActiveDecoders[bufferIndex]));

			if(swapSucceeded)
				delete decoderState, decoderState = nullptr;
		}
		
		// Wait for any thread to signal us to try and collect finished decoders
		mCollectorSemaphore.TimedWait(timeout);
	}
	
	LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Collecting thread terminating");
	
	return nullptr;
}

#pragma mark AudioHardware Utilities

bool BasicAudioPlayer::OpenOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Opening output for device 0x" << std::hex << mOutputDeviceID);
	
	// Create the IOProc which will feed audio to the device
	OSStatus result = AudioDeviceCreateIOProcID(mOutputDeviceID, 
												myIOProc, 
												this, 
												&mOutputDeviceIOProcID);
	
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioDeviceCreateIOProcID failed: " << result);
		return false;
	}

	// Register device property listeners
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDeviceProcessorOverload, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioDeviceProcessorOverload) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioDevicePropertyBufferFrameSize) failed: " << result);
		return false;
	}
		
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}
	
	propertyAddress.mSelector = kAudioObjectPropertyName;

	CFStringRef deviceName = nullptr;
	UInt32 dataSize = sizeof(deviceName);
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										nullptr, 
										&dataSize, 
										&deviceName);

	if(kAudioHardwareNoError == result) {
		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Opening output for device 0x" << std::hex << mOutputDeviceID << " (" << deviceName << ")");
	}
	else
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: " << result);

	if(deviceName)
		CFRelease(deviceName), deviceName = nullptr;

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioDevicePropertyStreams) failed: " << result);

	// Get the device's stream information
	if(!GetOutputStreams(mOutputDeviceStreamIDs))
		return false;

	if(!AddVirtualFormatPropertyListeners())
		return false;

	mOutputConverters = new PCMConverter * [mOutputDeviceStreamIDs.size()];
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
		mOutputConverters[i] = nullptr;

	return true;
}

bool BasicAudioPlayer::CloseOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Closing output for device 0x" << std::hex << mOutputDeviceID);

	OSStatus result = AudioDeviceDestroyIOProcID(mOutputDeviceID, 
												 mOutputDeviceIOProcID);

	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioDeviceDestroyIOProcID failed: " << result);
	
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDeviceProcessorOverload, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioDeviceProcessorOverload) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioDevicePropertyBufferFrameSize) failed: " << result);
	
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioDevicePropertyNominalSampleRate) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioDevicePropertyStreams) failed: " << result);

	if(!RemoveVirtualFormatPropertyListeners())
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "RemoveVirtualFormatPropertyListeners failed");

	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(nullptr != mOutputConverters[i])
			delete mOutputConverters[i], mOutputConverters[i] = nullptr;
	}
	
	delete [] mOutputConverters, mOutputConverters = nullptr;
	
	mOutputDeviceStreamIDs.clear();

	return true;
}

bool BasicAudioPlayer::StartOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Starting device 0x" << std::hex << mOutputDeviceID);

	// We don't want to start output in the middle of a buffer modification
	Mutex::Locker lock(mGuard);

	OSStatus result = AudioDeviceStart(mOutputDeviceID, 
									   mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioDeviceStart failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::StopOutput()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Stopping device 0x" << std::hex << mOutputDeviceID);

	OSStatus result = AudioDeviceStop(mOutputDeviceID, 
									  mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioDeviceStop failed: " << result);
		return false;
	}
	
	return true;
}

bool BasicAudioPlayer::OutputIsRunning() const
{
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyDeviceIsRunning, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};

	UInt32 isRunning = 0;
	UInt32 dataSize = sizeof(isRunning);

	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, 
												 &propertyAddress, 
												 0,
												 nullptr, 
												 &dataSize,
												 &isRunning);
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: " << result);
		return false;
	}
	
	return isRunning;
}

bool BasicAudioPlayer::ResetOutput()
{
	// Since this can be called from the IOProc, don't log informational messages in non-debug builds
#if DEBUG
	LOGGER_DEBUG("org.sbooth.AudioEngine.BasicAudioPlayer", "Resetting output");
#endif

	if(nullptr != mSampleRateConverter) {
		OSStatus result = AudioConverterReset(mSampleRateConverter);
		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterReset failed: " << result);
			return false;
		}
	}

	return true;
}

#pragma mark Other Utilities

DecoderStateData * BasicAudioPlayer::GetCurrentDecoderState() const
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

DecoderStateData * BasicAudioPlayer::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const
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

void BasicAudioPlayer::StopActiveDecoders()
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

bool BasicAudioPlayer::CreateConvertersAndSRCBuffer()
{
	// Clean up
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(nullptr != mOutputConverters[i])
			delete mOutputConverters[i], mOutputConverters[i] = nullptr;
	}
	
	if(nullptr != mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = nullptr;
			
		if(noErr != result)
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterDispose failed: " << result);
	}
	
	if(nullptr != mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);
	
	if(nullptr != mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
	
	// If the ring buffer does not yet have a format, no buffers can be allocated
	if(0 == mRingBufferFormat.mChannelsPerFrame || 0 == mRingBufferFormat.mSampleRate) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "Ring buffer has invalid format");
		return false;
	}

	// Get the output buffer size for the device
	AudioObjectPropertyAddress propertyAddress = { 
		.mSelector	= kAudioDevicePropertyBufferFrameSize, 
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(mOutputDeviceBufferFrameSize);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &mOutputDeviceBufferFrameSize);	
	
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: " << result);
		return false;
	}

	// FIXME: Handle devices with variable output buffer sizes
	propertyAddress.mSelector = kAudioDevicePropertyUsesVariableBufferFrameSizes;
	if(AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Devices with variable buffer sizes not supported");
		return false;
	}

	AudioStreamBasicDescription outputBufferFormat = mRingBufferFormat;

	// Create a sample rate converter if required
	Float64 deviceSampleRate;
	if(!GetOutputDeviceSampleRate(deviceSampleRate)) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Unable to determine output device sample rate");
		return false;
	}
	
	if(deviceSampleRate != mRingBufferFormat.mSampleRate) {
		outputBufferFormat.mSampleRate = deviceSampleRate;
		
		result = AudioConverterNew(&mRingBufferFormat, &outputBufferFormat, &mSampleRateConverter);
		
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterNew failed: " << result);
			return false;
		}
		
		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Using sample rate converter for " << mRingBufferFormat.mSampleRate << " Hz to " << deviceSampleRate << " Hz conversion");

		if(!ReallocateSampleRateConversionBuffer())
			return false;
	}

	// Allocate the output buffer (data is at the device's sample rate)
	mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);

	// Determine the channel map to use when mapping channels to the device for output
	UInt32 deviceChannelCount = 0;
	if(!GetDeviceChannelCount(deviceChannelCount)) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Unable to determine the total number of channels");
		return false;
	}

	// The default channel map is silence
	SInt32 deviceChannelMap [deviceChannelCount];
	for(UInt32 i = 0; i < deviceChannelCount; ++i)
		deviceChannelMap[i] = -1;
	
	// Determine the device's preferred stereo channels for output mapping
	if(1 == outputBufferFormat.mChannelsPerFrame || 2 == outputBufferFormat.mChannelsPerFrame) {
		propertyAddress.mSelector = kAudioDevicePropertyPreferredChannelsForStereo;
		propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
		
		UInt32 preferredStereoChannels [2] = { 1, 2 };
		if(AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
			dataSize = sizeof(preferredStereoChannels);
			
			result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize, &preferredStereoChannels);	
			
			if(kAudioHardwareNoError != result)
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo) failed: " << result);
		}
		
		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Device preferred stereo channels: " << preferredStereoChannels[0] << " " << preferredStereoChannels[1]);

		AudioChannelLayout stereoLayout;	
		stereoLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
		
		const AudioChannelLayout *specifier [2] = { mRingBufferChannelLayout, &stereoLayout };
		
		SInt32 stereoChannelMap [2] = { 1, 2 };
		dataSize = sizeof(stereoChannelMap);
		result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(specifier), specifier, &dataSize, stereoChannelMap);
		
		if(noErr == result) {
			deviceChannelMap[preferredStereoChannels[0] - 1] = stereoChannelMap[0];
			deviceChannelMap[preferredStereoChannels[1] - 1] = stereoChannelMap[1];
		}
		else {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);
			
			// Just use a channel map that makes sense
			deviceChannelMap[preferredStereoChannels[0] - 1] = 0;
			deviceChannelMap[preferredStereoChannels[1] - 1] = 1;
		}
	}
	// Determine the device's preferred multichannel layout
	else {
		propertyAddress.mSelector = kAudioDevicePropertyPreferredChannelLayout;
		propertyAddress.mScope = kAudioDevicePropertyScopeOutput;

		if(AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
			result = AudioObjectGetPropertyDataSize(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize);
			
			if(kAudioHardwareNoError != result)
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyPreferredChannelLayout) failed: " << result);

			AudioChannelLayout *preferredChannelLayout = static_cast<AudioChannelLayout *>(malloc(dataSize));
			
			result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, &dataSize, preferredChannelLayout);	
			
			if(kAudioHardwareNoError != result)
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelLayout) failed: " << result);

			LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Device preferred channel layout: " << preferredChannelLayout);
			
			const AudioChannelLayout *specifier [2] = { mRingBufferChannelLayout, preferredChannelLayout };

			// Not all channel layouts can be mapped, so handle failure with a generic mapping
			dataSize = (UInt32)sizeof(deviceChannelMap);
			result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(specifier), specifier, &dataSize, deviceChannelMap);
				
			if(noErr != result) {
				LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);

				// Just use a channel map that makes sense
				for(UInt32 i = 0; i < std::min(outputBufferFormat.mChannelsPerFrame, deviceChannelCount); ++i)
					deviceChannelMap[i] = i;
			}
			
			free(preferredChannelLayout), preferredChannelLayout = nullptr;		
		}
		else {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "No preferred multichannel layout");
			
			// Just use a channel map that makes sense
			for(UInt32 i = 0; i < deviceChannelCount; ++i)
				deviceChannelMap[i] = i;
		}
	}

	// For efficiency disable streams that aren't needed
	size_t streamUsageSize = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) + (sizeof(UInt32) * mOutputDeviceStreamIDs.size());
	AudioHardwareIOProcStreamUsage *streamUsage = static_cast<AudioHardwareIOProcStreamUsage *>(calloc(1, streamUsageSize));
	
	streamUsage->mIOProc = reinterpret_cast<void *>(mOutputDeviceIOProcID);
	streamUsage->mNumberStreams = static_cast<UInt32>(mOutputDeviceStreamIDs.size());

	// Create the output converter for each stream as required
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		AudioStreamID streamID = mOutputDeviceStreamIDs[i];

		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "Stream 0x" << std::hex << streamID << " information: ");

		AudioStreamBasicDescription virtualFormat;
		if(!GetOutputStreamVirtualFormat(streamID, virtualFormat)) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Unknown virtual format for AudioStreamID 0x" << std::hex << streamID);
			free(streamUsage), streamUsage = nullptr;
			return false;
		}

		// In some cases when this function is called from Enqueue() immediately after a device sample rate change, the device's
		// nominal sample rate has changed but the virtual formats have not
		if(deviceSampleRate != virtualFormat.mSampleRate) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Internal inconsistency: device sample rate (" << deviceSampleRate << " Hz) and virtual format sample rate (" << virtualFormat.mSampleRate << " Hz) don't match");
			free(streamUsage), streamUsage = nullptr;
			return false;
		}

		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "  Virtual format: " << virtualFormat);

		// Set up the channel mapping to determine if this stream is needed
		propertyAddress.mSelector = kAudioStreamPropertyStartingChannel;
		propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
		
		UInt32 startingChannel;
		dataSize = sizeof(startingChannel);
		
		result = AudioObjectGetPropertyData(streamID, &propertyAddress, 0, nullptr, &dataSize, &startingChannel);	

		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectGetPropertyData (kAudioStreamPropertyStartingChannel) failed: " << result);
			free(streamUsage), streamUsage = nullptr;
			return false;
		}
		
		LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "  Starting channel: " << startingChannel);
		
		UInt32 endingChannel = startingChannel + virtualFormat.mChannelsPerFrame;

		std::map<int, int> channelMap;
		for(UInt32 channel = startingChannel; channel < endingChannel; ++channel) {
			if(-1 != deviceChannelMap[channel - 1])
				channelMap[channel - 1] = deviceChannelMap[channel - 1];
		}

		// If the channel map isn't empty, the stream is used and an output converter is necessary
		if(!channelMap.empty()) {
			try {
				mOutputConverters[i] = new PCMConverter(outputBufferFormat, virtualFormat);			
			}

			catch(const std::exception& e) {
				LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "Error creating PCMConverter: " << e.what());
				free(streamUsage), streamUsage = nullptr;
				return false;
			}

			mOutputConverters[i]->SetChannelMap(channelMap);

			LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "  Channel map: ");
			for(std::map<int, int>::const_iterator mapIterator = channelMap.begin(); mapIterator != channelMap.end(); ++mapIterator)
				LOGGER_INFO("org.sbooth.AudioEngine.BasicAudioPlayer", "    " << mapIterator->first << " -> " << mapIterator->second);

			streamUsage->mStreamIsOn[i] = true;
		}
	}

	// Disable the unneeded streams
	propertyAddress.mSelector = kAudioDevicePropertyIOProcStreamUsage;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;

	result = AudioObjectSetPropertyData(mOutputDeviceID, &propertyAddress, 0, nullptr, static_cast<UInt32>(streamUsageSize), streamUsage);
	
	free(streamUsage), streamUsage = nullptr;

	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectSetPropertyData (kAudioDevicePropertyIOProcStreamUsage) failed: " << result);
		return false;
	}

	return true;
}

bool BasicAudioPlayer::AddVirtualFormatPropertyListeners()
{
	for(std::vector<AudioStreamID>::const_iterator iter = mOutputDeviceStreamIDs.begin(); iter != mOutputDeviceStreamIDs.end(); ++iter) {
		AudioObjectPropertyAddress propertyAddress = { 
			.mSelector	= kAudioStreamPropertyVirtualFormat, 
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster 
		};
		
		// Observe virtual format changes for the streams
		OSStatus result = AudioObjectAddPropertyListener(*iter,
														 &propertyAddress,
														 myAudioObjectPropertyListenerProc,
														 this);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: " << result);
			return false;
		}
		
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectAddPropertyListener(*iter,
												&propertyAddress,
												myAudioObjectPropertyListenerProc,
												this);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectAddPropertyListener (kAudioStreamPropertyPhysicalFormat) failed: " << result);
			return false;
		}
	}

	return true;
}

bool BasicAudioPlayer::RemoveVirtualFormatPropertyListeners()
{
	for(std::vector<AudioStreamID>::const_iterator iter = mOutputDeviceStreamIDs.begin(); iter != mOutputDeviceStreamIDs.end(); ++iter) {
		AudioObjectPropertyAddress propertyAddress = { 
			.mSelector	= kAudioStreamPropertyVirtualFormat, 
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster 
		};
		
		OSStatus result = AudioObjectRemovePropertyListener(*iter,
															&propertyAddress,
															myAudioObjectPropertyListenerProc,
															this);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: " << result);
			continue;
		}
		
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectRemovePropertyListener(*iter,
												   &propertyAddress,
												   myAudioObjectPropertyListenerProc,
												   this);
		
		if(kAudioHardwareNoError != result) {
			LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioObjectRemovePropertyListener (kAudioStreamPropertyPhysicalFormat) failed: " << result);
			continue;
		}
	}
	
	return true;
}

bool BasicAudioPlayer::ReallocateSampleRateConversionBuffer()
{
	if(nullptr == mSampleRateConverter)
		return false;

	// Get the SRC's output format
	AudioStreamBasicDescription outputBufferFormat;
	UInt32 dataSize = sizeof(outputBufferFormat);

	OSStatus result = AudioConverterGetProperty(mSampleRateConverter, 
												kAudioConverterCurrentOutputStreamDescription, 
												&dataSize, 
												&outputBufferFormat);

	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterGetProperty (kAudioConverterCurrentOutputStreamDescription) failed: " << result);
		return false;
	}

	// Calculate how large the sample rate conversion buffer must be
	UInt32 bufferSizeBytes = mOutputDeviceBufferFrameSize * outputBufferFormat.mBytesPerFrame;
	dataSize = sizeof(bufferSizeBytes);

	result = AudioConverterGetProperty(mSampleRateConverter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &bufferSizeBytes);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.BasicAudioPlayer", "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);
		return false;
	}

	if(mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);

	// Allocate the sample rate conversion buffer (data is at the ring buffer's sample rate)
	mSampleRateConversionBuffer = AllocateABL(mRingBufferFormat, bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);

	return true;
}
