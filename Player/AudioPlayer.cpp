/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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
#include <mach/thread_act.h>
#include <mach/mach_error.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <Accelerate/Accelerate.h>
#include <stdexcept>
#include <new>
#include <algorithm>

#include "AudioEngineDefines.h"
#include "AudioPlayer.h"
#include "AudioDecoder.h"
#include "DecoderStateData.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "DeinterleavingFloatConverter.h"
#include "PCMConverter.h"

#include "CARingBuffer.h"

#if DEBUG
#  include "CAStreamBasicDescription.h"
#endif


// ========================================
// Macros
// ========================================
#define RING_BUFFER_SIZE_FRAMES					16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6


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
#if DEBUG
		mach_error(const_cast<char *>("Couldn't set thread's extended policy"), error);
#endif
		return false;
	}
	
	// Give the thread the specified importance
	thread_precedence_policy_data_t precedencePolicy = { importance };
	error = thread_policy_set(mach_thread_self(), 
							  THREAD_PRECEDENCE_POLICY, 
							  (thread_policy_t)&precedencePolicy, 
							  THREAD_PRECEDENCE_POLICY_COUNT);
	
	if (error != KERN_SUCCESS) {
#if DEBUG
		mach_error(const_cast<char *>("Couldn't set thread's precedence policy"), error);
#endif
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
	assert(NULL != inClientData);

	AudioPlayer *player = static_cast<AudioPlayer *>(inClientData);
	return player->Render(inDevice, inNow, inInputData, inInputTime, outOutputData, inOutputTime);
}

static OSStatus
myAudioObjectPropertyListenerProc(AudioObjectID							inObjectID,
								  UInt32								inNumberAddresses,
								  const AudioObjectPropertyAddress		inAddresses[],
								  void									*inClientData)
{
	assert(NULL != inClientData);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(inClientData);
	return player->AudioObjectPropertyChanged(inObjectID, inNumberAddresses, inAddresses);
}

// ========================================
// The decoder thread's entry point
// ========================================
static void *
decoderEntry(void *arg)
{
	assert(NULL != arg);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(arg);
	return player->DecoderThreadEntry();
}

// ========================================
// The collector thread's entry point
// ========================================
static void *
collectorEntry(void *arg)
{
	assert(NULL != arg);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(arg);
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
	assert(NULL != inUserData);
	assert(NULL != ioNumberDataPackets);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(inUserData);	
	return player->FillSampleRateConversionBuffer(inAudioConverter, ioNumberDataPackets, ioData, outDataPacketDescription);
}


#pragma mark Creation/Destruction


AudioPlayer::AudioPlayer()
	: mOutputDeviceID(kAudioDeviceUnknown), mOutputDeviceIOProcID(NULL), mOutputDeviceBufferFrameSize(0), mIsPlaying(false), mFlags(0), mDecoderQueue(NULL), mRingBuffer(NULL), mOutputConverters(NULL), mSampleRateConverter(NULL), mSampleRateConversionBuffer(NULL), mOutputBuffer(NULL), mFramesDecoded(0), mFramesRendered(0)
{
	mDecoderQueue = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
	
	if(NULL == mDecoderQueue)
		throw std::bad_alloc();

	mRingBuffer = new CARingBuffer();

	// ========================================
	// Create the semaphore and mutex to be used by the decoding and rendering threads
	kern_return_t result = semaphore_create(mach_task_self(), &mDecoderSemaphore, SYNC_POLICY_FIFO, 0);
	if(KERN_SUCCESS != result) {
#if DEBUG
		mach_error(const_cast<char *>("semaphore_create"), result);
#endif

		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		throw std::runtime_error("semaphore_create failed");
	}

	result = semaphore_create(mach_task_self(), &mCollectorSemaphore, SYNC_POLICY_FIFO, 0);
	if(KERN_SUCCESS != result) {
#if DEBUG
		mach_error(const_cast<char *>("semaphore_create"), result);
#endif
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif
		
		throw std::runtime_error("semaphore_create failed");
	}
	
	int success = pthread_mutex_init(&mMutex, NULL);
	if(0 != success) {
		ERR("pthread_mutex_init failed: %i", success);
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif

		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif

		throw std::runtime_error("pthread_mutex_init failed");
	}

	// ========================================
	// Initialize the decoder array
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex)
		mActiveDecoders[bufferIndex] = NULL;

	// ========================================
	// Launch the decoding thread
	mKeepDecoding = true;
	int creationResult = pthread_create(&mDecoderThread, NULL, decoderEntry, this);
	if(0 != creationResult) {
		ERR("pthread_create failed: %i", creationResult);
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif

		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif
		
		throw std::runtime_error("pthread_create failed");
	}
	
	// ========================================
	// Launch the collector thread
	mKeepCollecting = true;
	creationResult = pthread_create(&mCollectorThread, NULL, collectorEntry, this);
	if(0 != creationResult) {
		ERR("pthread_create failed: %i", creationResult);
		
		mKeepDecoding = false;
		semaphore_signal(mDecoderSemaphore);
		
		int joinResult = pthread_join(mDecoderThread, NULL);
		if(0 != joinResult)
			ERR("pthread_join failed: %i", joinResult);
		
		mDecoderThread = static_cast<pthread_t>(0);
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif
		
		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
#if DEBUG
		if(KERN_SUCCESS != result)
			mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif

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
		kAudioHardwarePropertyDefaultOutputDevice, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(mOutputDeviceID);

    OSStatus hwResult = AudioObjectGetPropertyData(kAudioObjectSystemObject,
												   &propertyAddress,
												   0,
												   NULL,
												   &dataSize,
												   &mOutputDeviceID);
	
	if(kAudioHardwareNoError != hwResult) {
		ERR("AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: %i", hwResult);
		throw std::runtime_error("AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed");
	}

	if(false == OpenOutput()) {
		ERR("OpenOutput failed");
		throw std::runtime_error("OpenOutput failed");
	}
}

AudioPlayer::~AudioPlayer()
{
	// Stop the processing graph and reclaim its resources
	if(false == CloseOutput())
		ERR("CloseOutput failed");

	// Dispose of all active decoders
	StopActiveDecoders();
	
	// End the decoding thread
	mKeepDecoding = false;
	semaphore_signal(mDecoderSemaphore);
	
	int joinResult = pthread_join(mDecoderThread, NULL);
	if(0 != joinResult)
		ERR("pthread_join failed: %i", joinResult);
	
	mDecoderThread = static_cast<pthread_t>(0);

	// End the collector thread
	mKeepCollecting = false;
	semaphore_signal(mCollectorSemaphore);
	
	joinResult = pthread_join(mCollectorThread, NULL);
	if(0 != joinResult)
		ERR("pthread_join failed: %i", joinResult);
	
	mCollectorThread = static_cast<pthread_t>(0);

	// Force any decoders left hanging by the collector to end
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		if(NULL != mActiveDecoders[bufferIndex])
			delete mActiveDecoders[bufferIndex], mActiveDecoders[bufferIndex] = NULL;
	}
	
	// Clean up any queued decoders
	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>(const_cast<void *>(CFArrayGetValueAtIndex(mDecoderQueue, 0)));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}
	
	CFRelease(mDecoderQueue), mDecoderQueue = NULL;

	// Clean up the ring buffer
	if(mRingBuffer)
		delete mRingBuffer, mRingBuffer = NULL;

	// Clean up the converters and conversion buffers
	if(mOutputConverters) {
		for(UInt32 i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
			delete mOutputConverters[i], mOutputConverters[i] = NULL;
		delete [] mOutputConverters, mOutputConverters = NULL;
	}
	
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = NULL;

		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
	}

	if(mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);

	if(mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
	
	// Destroy the decoder and collector semaphores
	kern_return_t result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
#if DEBUG
	if(KERN_SUCCESS != result)
		mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif

	result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
#if DEBUG
	if(KERN_SUCCESS != result)
		mach_error(const_cast<char *>("semaphore_destroy"), result);
#endif
	
	// Destroy the decoder mutex
	int success = pthread_mutex_destroy(&mMutex);
	if(0 != success)
		ERR("pthread_mutex_destroy failed: %i", success);
}


#pragma mark Playback Control


void AudioPlayer::Play()
{
	if(IsPlaying())
		return;

	mIsPlaying = StartOutput();
}

void AudioPlayer::Pause()
{
	if(false == IsPlaying())
		return;
	
	mIsPlaying = (false == StopOutput());
}

void AudioPlayer::Stop()
{
	Pause();
	
	StopActiveDecoders();
	
	ResetOutput();

	mFramesDecoded = 0;
	mFramesRendered = 0;
}

CFURLRef AudioPlayer::GetPlayingURL()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return NULL;
	
	return currentDecoderState->mDecoder->GetURL();
}


#pragma mark Playback Properties


SInt64 AudioPlayer::GetCurrentFrame()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
}

SInt64 AudioPlayer::GetTotalFrames()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return currentDecoderState->mTotalFrames;
}

CFTimeInterval AudioPlayer::GetCurrentTime()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return static_cast<CFTimeInterval>(GetCurrentFrame() / currentDecoderState->mDecoder->GetFormat().mSampleRate);
}

CFTimeInterval AudioPlayer::GetTotalTime()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return static_cast<CFTimeInterval>(currentDecoderState->mTotalFrames / currentDecoderState->mDecoder->GetFormat().mSampleRate);
}


#pragma mark Seeking


bool AudioPlayer::SeekForward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 desiredFrame		= GetCurrentFrame() + frameCount;
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::min(desiredFrame, totalFrames - 1));
}

bool AudioPlayer::SeekBackward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 currentFrame		= GetCurrentFrame();
	SInt64 desiredFrame		= currentFrame - frameCount;
	
	return SeekToFrame(std::max(0LL, desiredFrame));
}

bool AudioPlayer::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	SInt64 desiredFrame		= static_cast<SInt64>(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool AudioPlayer::SeekToFrame(SInt64 frame)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	if(false == currentDecoderState->mDecoder->SupportsSeeking())
		return false;
	
	if(0 > frame || frame >= currentDecoderState->mTotalFrames)
		return false;

	if(false == OSAtomicCompareAndSwap64Barrier(currentDecoderState->mFrameToSeek, frame, &currentDecoderState->mFrameToSeek))
		return false;
	
	semaphore_signal(mDecoderSemaphore);

	return true;	
}

bool AudioPlayer::SupportsSeeking()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}


#pragma mark Player Parameters


bool AudioPlayer::GetMasterVolume(Float32& volume)
{
	return GetVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::SetMasterVolume(Float32 volume)
{
	return SetVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::GetVolumeForChannel(UInt32 channel, Float32& volume)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyVolumeScalar, 
		kAudioDevicePropertyScopeOutput,
		channel 
	};
	
	if(false == AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOG("AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar [kAudioDevicePropertyScopeOutput, %i]) is false", channel);
		return false;
	}
	
	UInt32 dataSize = sizeof(volume);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &volume);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar [kAudioDevicePropertyScopeOutput, %i]) failed: %i", channel, result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetVolumeForChannel(UInt32 channel, Float32 volume)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyVolumeScalar, 
		kAudioDevicePropertyScopeOutput,
		channel 
	};
	
	if(false == AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOG("AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar [kAudioDevicePropertyScopeOutput, %i]) is false", channel);
		return false;
	}

	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 sizeof(volume),
												 &volume);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar [kAudioDevicePropertyScopeOutput, %i]) failed: %i", channel, result);
		return false;
	}
	
	return true;
}


#pragma mark Device Management


CFStringRef AudioPlayer::CreateOutputDeviceUID()
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyDeviceUID, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	CFStringRef deviceUID = NULL;
	UInt32 dataSize = sizeof(deviceUID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &deviceUID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: %i", result);
		return NULL;
	}
	
	return deviceUID;
}

bool AudioPlayer::SetOutputDeviceUID(CFStringRef deviceUID)
{
	AudioDeviceID		deviceID		= kAudioDeviceUnknown;
	UInt32				specifierSize	= 0;

	// If NULL was passed as the device UID, use the default output device
	if(NULL == deviceUID) {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioHardwarePropertyDefaultOutputDevice, 
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};
		
		specifierSize = sizeof(deviceID);
		
		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
													 &propertyAddress,
													 0,
													 NULL,
													 &specifierSize,
													 &deviceID);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: %i", result);
			return false;
		}
	}
	else {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioHardwarePropertyDeviceForUID, 
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};
		
		AudioValueTranslation translation = {
			&deviceUID, sizeof(deviceUID),
			&deviceID, sizeof(deviceID)
		};
		
		specifierSize = sizeof(translation);
		
		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
													 &propertyAddress,
													 0,
													 NULL,
													 &specifierSize,
													 &translation);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: %i", result);
			return false;
		}
	}
	
	// The device isn't connected or doesn't exist
	if(kAudioDeviceUnknown == deviceID)
		return false;

	return SetOutputDeviceID(deviceID);
}

bool AudioPlayer::SetOutputDeviceID(AudioDeviceID deviceID)
{
	assert(kAudioDeviceUnknown != deviceID);
	
	if(deviceID == mOutputDeviceID)
		return true;

	if(false == CloseOutput())
		return false;
	
	mOutputDeviceID = deviceID;
	
	if(false == OpenOutput())
		return false;
	
	return true;
}

bool AudioPlayer::GetOutputDeviceSampleRate(Float64& deviceSampleRate)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(deviceSampleRate);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &deviceSampleRate);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetOutputDeviceSampleRate(Float64 deviceSampleRate)
{
	LOG("Setting device %#x sample rate to %.0f Hz", mOutputDeviceID, deviceSampleRate);

	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 sizeof(deviceSampleRate),
												 &deviceSampleRate);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}

	return true;
}

bool AudioPlayer::OutputDeviceIsHogged()
{
	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyHogMode, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %i", result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool AudioPlayer::StartHoggingOutputDevice()
{
	// Is it hogged already?
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyHogMode, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %i", result);
		return false;
	}
	
	// The device is already hogged
	if(hogPID != static_cast<pid_t>(-1)) {
		LOG("Device is already hogged by pid: %d", hogPID);
		return false;
	}

	// If IO is enabled, disable it while hog mode is acquired because the HAL
	// does not automatically restart IO after hog mode is taken
	bool wasPlaying = IsPlaying();
	if(wasPlaying)
		Pause();
			
	hogPID = getpid();
	
	result = AudioObjectSetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										NULL, 
										sizeof(hogPID), 
										&hogPID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: %i", result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(wasPlaying)
		Play();

	return true;
}

bool AudioPlayer::StopHoggingOutputDevice()
{
	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyHogMode, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	pid_t hogPID = static_cast<pid_t>(-1);
	UInt32 dataSize = sizeof(hogPID);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &hogPID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %i", result);
		return false;
	}
	
	// If we don't own hog mode we can't release it
	if(hogPID != getpid())
		return false;

	// Disable IO while hog mode is released
	bool wasPlaying = IsPlaying();
	if(wasPlaying)
		Pause();

	// Release hog mode.
	hogPID = static_cast<pid_t>(-1);

	result = AudioObjectSetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										NULL, 
										sizeof(hogPID), 
										&hogPID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: %i", result);
		return false;
	}
	
	if(wasPlaying)
		Play();
	
	return true;
}


#pragma mark Stream Management


bool AudioPlayer::GetOutputStreams(std::vector<AudioStreamID>& streams)
{
	streams.clear();

	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyStreams, 
		kAudioDevicePropertyScopeOutput, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(mOutputDeviceID, 
													 &propertyAddress, 
													 0,
													 NULL,
													 &dataSize);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}
	
	UInt32 streamCount = static_cast<UInt32>(dataSize / sizeof(AudioStreamID));
	AudioStreamID audioStreamIDs [streamCount];
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										NULL, 
										&dataSize, 
										audioStreamIDs);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}

	streams.reserve(streamCount);
	for(UInt32 i = 0; i < streamCount; ++i)
		streams.push_back(audioStreamIDs[i]);

	return true;
}

bool AudioPlayer::GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat)
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		ERR("Unknown AudioStreamID: %#x", streamID);
		return false;
	}

	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyVirtualFormat,
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(virtualFormat);
	
	OSStatus result = AudioObjectGetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &virtualFormat);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %i", result);
		return false;
	}
	
	return true;	
}

bool AudioPlayer::SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat)
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		ERR("Unknown AudioStreamID: %#x", streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyVirtualFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 NULL,
												 sizeof(virtualFormat),
												 &virtualFormat);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat)
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		ERR("Unknown AudioStreamID: %#x", streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyPhysicalFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(physicalFormat);
	
	OSStatus result = AudioObjectGetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &physicalFormat);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat)
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		ERR("Unknown AudioStreamID: %#x", streamID);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyPhysicalFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 NULL,
												 sizeof(physicalFormat),
												 &physicalFormat);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
		return false;
	}
	
	return true;
}


#pragma mark Playlist Management


bool AudioPlayer::Enqueue(CFURLRef url)
{
	assert(NULL != url);
	
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(url);
	
	if(NULL == decoder)
		return false;
	
	bool success = Enqueue(decoder);
	
	if(false == success)
		delete decoder;
	
	return success;
}

bool AudioPlayer::Enqueue(AudioDecoder *decoder)
{
	assert(NULL != decoder);
	
	int lockResult = pthread_mutex_lock(&mMutex);
	
	if(0 != lockResult) {
		ERR("pthread_mutex_lock failed: %i", lockResult);
		return false;
	}
	
	bool queueEmpty = (0 == CFArrayGetCount(mDecoderQueue));
		
	lockResult = pthread_mutex_unlock(&mMutex);
		
	if(0 != lockResult)
		ERR("pthread_mutex_unlock failed: %i", lockResult);
	
	// If there are no decoders in the queue, set up for playback
	if(NULL == GetCurrentDecoderState() && queueEmpty) {
		AudioStreamBasicDescription format = decoder->GetFormat();

		// The ring buffer contains deinterleaved floats at the decoder's sample rate and number of channels
		mRingBufferFormat.mSampleRate			= format.mSampleRate;
		mRingBufferFormat.mChannelsPerFrame		= format.mChannelsPerFrame;

		if(!CreateConvertersAndConversionBuffers())
			return false;
		
		// Allocate enough space in the ring buffer for the new format
		mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame,
							  mRingBufferFormat.mBytesPerFrame,
							  RING_BUFFER_SIZE_FRAMES);
	}
	// Otherwise, enqueue this decoder if the format matches
	else {
		AudioStreamBasicDescription		nextFormat			= decoder->GetFormat();
	//	AudioChannelLayout				nextChannelLayout	= decoder->GetChannelLayout();
		
		bool	formatsMatch			= (nextFormat.mSampleRate == mRingBufferFormat.mSampleRate && nextFormat.mChannelsPerFrame == mRingBufferFormat.mChannelsPerFrame);
	//	bool	channelLayoutsMatch		= ChannelLayoutsAreEqual(&nextChannelLayout, &mRingBufferChannelLayout);
		
		// The two files can be joined seamlessly only if they have the same formats and channel layouts
		if(false == formatsMatch /*|| false == channelLayoutsMatch*/)
			return false;
	}
	
	// Add the decoder to the queue
	lockResult = pthread_mutex_lock(&mMutex);
	
	if(0 != lockResult) {
		ERR("pthread_mutex_lock failed: %i", lockResult);
		return false;
	}
	
	CFArrayAppendValue(mDecoderQueue, decoder);
	
	lockResult = pthread_mutex_unlock(&mMutex);
	
	if(0 != lockResult)
		ERR("pthread_mutex_unlock failed: %i", lockResult);
	
	semaphore_signal(mDecoderSemaphore);
	
	return true;
}

bool AudioPlayer::ClearQueuedDecoders()
{
	int lockResult = pthread_mutex_lock(&mMutex);
	
	if(0 != lockResult) {
		ERR("pthread_mutex_lock failed: %i", lockResult);
		return false;
	}
	
	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>(const_cast<void *>(CFArrayGetValueAtIndex(mDecoderQueue, 0)));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}
	
	lockResult = pthread_mutex_unlock(&mMutex);
	
	if(0 != lockResult)
		ERR("pthread_mutex_unlock failed: %i", lockResult);
	
	return true;	
}


#pragma mark IOProc


OSStatus AudioPlayer::Render(AudioDeviceID			inDevice,
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
	assert(NULL != outOutputData);

	// ========================================
	// RENDERING

	// Mute functionality
	if(eAudioPlayerFlagMuteOutput & mFlags)
		return kAudioHardwareNoError;

	// Don't render during seeks
	if(eAudioPlayerFlagIsSeeking & mFlags)
		return kAudioHardwareNoError;
	
	// If the ring buffer doesn't contain any valid audio, skip some work
	UInt32 framesAvailableToRead = static_cast<UInt32>(mFramesDecoded - mFramesRendered);

	if(0 == framesAvailableToRead) {
		// If there are no decoders in the queue, stop IO
		if(NULL == GetCurrentDecoderState())
			Stop();
		
		return kAudioHardwareNoError;
	}

	// Reset state
	mFramesRenderedLastPass = 0;

	// Determine how many frames to read
	UInt32 framesToRead = std::min(framesAvailableToRead, mOutputDeviceBufferFrameSize);

	// Convert to the stream's sample rate, if required
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterFillComplexBuffer(mSampleRateConverter, 
														  mySampleRateConverterInputProc,
														  this,
														  &framesToRead, 
														  mOutputBuffer,
														  NULL);
		
		if(noErr != result) {
			ERR("AudioConverterFillComplexBuffer failed: %i", result);
			return result;
		}
	}
	// Otherwise fetch the output from the ring buffer
	else {
		CARingBufferError result = mRingBuffer->Fetch(mOutputBuffer, framesToRead, mFramesRendered, false);
		
		if(kCARingBufferError_OK != result) {
			ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
			return ioErr;
		}
		
		OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
		
		mFramesRenderedLastPass += framesToRead;
	}

	// Iterate through each stream and render output in the stream's format
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(NULL == mOutputConverters[i])
			continue;

		// Convert to the output device's format
		UInt32 framesConverted = mOutputConverters[i]->Convert(mOutputBuffer, outOutputData, framesToRead);
		
		if(framesConverted != framesToRead)
			ERR("Conversion to output format failed; all frames may not be rendered");
	}
	
	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = static_cast<UInt32>(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));

	if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite)
		semaphore_signal(mDecoderSemaphore);

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
	while(NULL != decoderState) {
		SInt64 timeStamp = decoderState->mTimeStamp;
		
		SInt64 decoderFramesRemaining = decoderState->mTotalFrames - decoderState->mFramesRendered;
		SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, static_cast<SInt64>(mFramesRenderedLastPass));
		
		if(0 == decoderState->mFramesRendered)
			decoderState->mDecoder->PerformRenderingStartedCallback();
		
		OSAtomicAdd64Barrier(framesFromThisDecoder, &decoderState->mFramesRendered);
		
		if(decoderState->mFramesRendered == decoderState->mTotalFrames) {
			decoderState->mDecoder->PerformRenderingFinishedCallback();			

			OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);

			// Since rendering is finished, signal the collector to clean up this decoder
			semaphore_signal(mCollectorSemaphore);
		}
		
		framesRemainingToDistribute -= framesFromThisDecoder;
		
		if(0 == framesRemainingToDistribute)
			break;
		
		decoderState = GetDecoderStateStartingAfterTimeStamp(timeStamp);
	}
	
	return kAudioHardwareNoError;
}

OSStatus AudioPlayer::AudioObjectPropertyChanged(AudioObjectID						inObjectID,
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
#if DEBUG
				case kAudioDevicePropertyDeviceIsRunning:
				{
					UInt32 isRunning = 0;
					UInt32 dataSize = sizeof(isRunning);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 NULL, 
																 &dataSize,
																 &isRunning);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);
						continue;
					}

					LOG("-> kAudioDevicePropertyDeviceIsRunning [%#x]: %s", inObjectID, isRunning ? "True" : "False");

					break;
				}

				case kAudioDevicePropertyNominalSampleRate:
				{
					Float64 deviceSampleRate = 0;
					UInt32 dataSize = sizeof(deviceSampleRate);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 NULL, 
																 &dataSize,
																 &deviceSampleRate);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
						continue;
					}
					
					LOG("-> kAudioDevicePropertyNominalSampleRate [%#x]: %.0f Hz", inObjectID, deviceSampleRate);
					
					break;
				}
					
#endif

				case kAudioDevicePropertyStreams:
				{
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					bool restartIO = false;
					if(OutputIsRunning())
						restartIO = StopOutput();

					// Stop observing properties on the defunct streams
					if(!RemoveVirtualFormatPropertyListeners())
						ERR("RemoveVirtualFormatPropertyListeners failed");

					// Update our list of cached streams
					if(!GetOutputStreams(mOutputDeviceStreamIDs)) 
						continue;

					// Populate the virtual formats and observe the new streams for changes
					if(!BuildVirtualFormatsCache())
						ERR("BuildVirtualFormatsCache failed");

					if(!AddVirtualFormatPropertyListeners())
						ERR("AddVirtualFormatPropertyListeners failed");
					
					if(!CreateConvertersAndConversionBuffers())
						ERR("CreateConvertersAndConversionBuffers failed");

					if(restartIO)
						StartOutput();

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
					
					LOG("-> kAudioDevicePropertyStreams [%#x] changed", inObjectID);

					break;
				}

				case kAudioDevicePropertyBufferFrameSize:
				{
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

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
																 NULL, 
																 &dataSize,
																 &mOutputDeviceBufferFrameSize);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: %i", result);
						continue;
					}
					
					AudioStreamBasicDescription outputBufferFormat = mRingBufferFormat;
					
					// Recalculate the sample rate conversion buffer size
					if(NULL != mSampleRateConverter) {
						// Get the SRC's output format (input is mRingBufferFormat)
						dataSize = sizeof(outputBufferFormat);
						
						result = AudioConverterGetProperty(mSampleRateConverter, 
														   kAudioConverterCurrentOutputStreamDescription, 
														   &dataSize, 
														   &outputBufferFormat);
						
						if(noErr != result) {
							ERR("AudioConverterGetProperty (kAudioConverterCurrentOutputStreamDescription) failed: %i", result);
							continue;
						}
						
						// Calculate how large the sample rate conversion buffer must be
						UInt32 bufferSizeBytes = mOutputDeviceBufferFrameSize * outputBufferFormat.mBytesPerFrame;
						dataSize = sizeof(bufferSizeBytes);
						
						result = AudioConverterGetProperty(mSampleRateConverter, 
														   kAudioConverterPropertyCalculateInputBufferSize, 
														   &dataSize, 
														   &bufferSizeBytes);
						
						if(noErr != result) {
							ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
							continue;
						}
						
						// Allocate the sample rate conversion buffer (data is at the ring buffer's sample rate)
						mSampleRateConversionBuffer = AllocateABL(mRingBufferFormat, bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);
					}
					
					// Allocate the output buffer (data is at the device's sample rate)
					mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					LOG("-> kAudioDevicePropertyBufferFrameSize [%#x]: %d", inObjectID, mOutputDeviceBufferFrameSize);

					break;
				}

				case kAudioDeviceProcessorOverload:
					LOG("-> kAudioDeviceProcessorOverload [%#x]: Unable to meet IOProc time constraints", inObjectID);
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
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					bool restartIO = false;
					if(OutputIsRunning())
						restartIO = StopOutput();

					// Get the new virtual format
					AudioStreamBasicDescription virtualFormat;
					UInt32 dataSize = sizeof(virtualFormat);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 NULL, 
																 &dataSize,
																 &virtualFormat);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %i", result);
						continue;
					}

#if DEBUG
					CAStreamBasicDescription streamVirtualFormat(virtualFormat);
					fprintf(stderr, "-> kAudioStreamPropertyVirtualFormat [%#x]: ", inObjectID);
					streamVirtualFormat.Print(stderr);
#endif

					mStreamVirtualFormats[inObjectID] = virtualFormat;

					if(false == CreateConvertersAndConversionBuffers())
						ERR("CreateConvertersAndConversionBuffers failed");

					if(restartIO)
						StartOutput();

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					break;
				}
					
#if DEBUG
				case kAudioStreamPropertyPhysicalFormat:
				{
					// Get the new physical format
					CAStreamBasicDescription physicalFormat;
					UInt32 dataSize = sizeof(physicalFormat);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 NULL, 
																 &dataSize,
																 &physicalFormat);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
						continue;
					}

					fprintf(stderr, "-> kAudioStreamPropertyPhysicalFormat [%#x]: ", inObjectID);
					physicalFormat.Print(stderr);

					break;
				}
#endif
			}
		}
	}
	
	return kAudioHardwareNoError;			
}

OSStatus AudioPlayer::FillSampleRateConversionBuffer(AudioConverterRef				inAudioConverter,
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
		return ioErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min(framesAvailableToRead, *ioNumberDataPackets);

	CARingBufferError result = mRingBuffer->Fetch(mSampleRateConversionBuffer, framesToRead, mFramesRendered, false);
	
	if(kCARingBufferError_OK != result) {
		ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
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


void * AudioPlayer::DecoderThreadEntry()
{
	// ========================================
	// Make ourselves a high priority thread
	if(false == setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		ERR("Couldn't set decoder thread importance");
	
	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };

	while(mKeepDecoding) {
		AudioDecoder *decoder = NULL;
		
		// ========================================
		// Lock the queue and remove the head element, which contains the next decoder to use
		int lockResult = pthread_mutex_lock(&mMutex);
		
		if(0 != lockResult) {
			ERR("pthread_mutex_lock failed: %i", lockResult);
			
			// Stop now, to avoid risking data corruption
			continue;
		}

		if(0 < CFArrayGetCount(mDecoderQueue)) {
			decoder = (AudioDecoder *)CFArrayGetValueAtIndex(mDecoderQueue, 0);
			CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		}
		
		lockResult = pthread_mutex_unlock(&mMutex);
		
		if(0 != lockResult)
			ERR("pthread_mutex_unlock failed: %i", lockResult);
		
		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(NULL != decoder) {

#if DEBUG
			fprintf(stderr, "Starting decoder for: ");
			CFShow(decoder->GetURL());
			CAStreamBasicDescription decoderASBD = decoder->GetFormat();
			fprintf(stderr, "Decoder format: ");
			decoderASBD.Print(stderr);
#endif
			
			// ========================================
			// Create the decoder state and append to the list of active decoders
			DecoderStateData *decoderState = new DecoderStateData(decoder);
			decoderState->mTimeStamp = mFramesDecoded;
			
			for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
				if(NULL != mActiveDecoders[bufferIndex])
					continue;
				
				if(OSAtomicCompareAndSwapPtrBarrier(NULL, decoderState, reinterpret_cast<void **>(&mActiveDecoders[bufferIndex])))
					break;
				else
					ERR("OSAtomicCompareAndSwapPtrBarrier failed");
			}
			
			SInt64 startTime = decoderState->mTimeStamp;

			AudioStreamBasicDescription decoderFormat = decoder->GetFormat();

			DeinterleavingFloatConverter *converter = new DeinterleavingFloatConverter(decoderFormat);

			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer			
			decoderState->AllocateBufferList(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES);

			AudioBufferList *bufferList = AllocateABL(mRingBufferFormat, RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES);
			
			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(decoderState && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)) {
				
				// Fill the ring buffer with as much data as possible
				while(decoderState) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = static_cast<UInt32>(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));
					
					// Force writes to the ring buffer to be at least RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES
					if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite) {
						
						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagIsSeeking */, &mFlags);
							
							SInt64 currentFrameBeforeSeeking = decoder->GetCurrentFrame();
							
							SInt64 newFrame = decoder->SeekToFrame(decoderState->mFrameToSeek);
							
							if(newFrame != decoderState->mFrameToSeek)
								ERR("Error seeking to frame %lld", decoderState->mFrameToSeek);
							
							// Update the seek request
							if(false == OSAtomicCompareAndSwap64Barrier(decoderState->mFrameToSeek, -1, &decoderState->mFrameToSeek))
								ERR("OSAtomicCompareAndSwap64Barrier failed");
							
							// If the seek failed do not update the counters
							if(-1 != newFrame) {
								SInt64 framesSkipped = newFrame - currentFrameBeforeSeeking;
								
								// Treat the skipped frames as if they were rendered, and update the counters accordingly
								if(false == OSAtomicCompareAndSwap64Barrier(decoderState->mFramesRendered, newFrame, &decoderState->mFramesRendered))
									ERR("OSAtomicCompareAndSwap64Barrier failed");
								
								OSAtomicAdd64Barrier(framesSkipped, &mFramesDecoded);
								if(false == OSAtomicCompareAndSwap64Barrier(mFramesRendered, mFramesDecoded, &mFramesRendered))
									ERR("OSAtomicCompareAndSwap64Barrier failed");
								
								// This is safe to call at this point, because eAudioPlayerFlagIsSeeking is set so
								// no rendering is being performed
								ResetOutput();
							}

							OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagIsSeeking */, &mFlags);
						}
						
						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						if(-1 == startingFrameNumber)
							break;

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber)
							decoder->PerformDecodingStartedCallback();
						
						// Read the input chunk
						UInt32 framesDecoded = decoderState->ReadAudio(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES);
						
						// Convert and store the decoded audio
						if(0 != framesDecoded) {
							
							UInt32 framesConverted = converter->Convert(decoderState->mBufferList, bufferList, framesDecoded);
							
							if(framesConverted != framesDecoded)
								ERR("Incomplete conversion: %d/%d frames", framesConverted, framesDecoded);

#if 0
							// Clip the samples to [-1, +1)
							UInt32 bitsPerChannel = decoder->GetSourceFormat().mBitsPerChannel;
							
							// For compressed formats, pretend the samples are 24-bit
							if(0 == bitsPerChannel)
								bitsPerChannel = 24;
							
							// The maximum allowable sample value
							double minValue = -1.;
							double maxValue = static_cast<double>((1u << (bitsPerChannel - 1)) - 1) / static_cast<double>(1u << (bitsPerChannel - 1));

							for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
								double *buffer = static_cast<double *>(bufferList->mBuffers[bufferIndex].mData);
								vDSP_vclipD(buffer, 1, &minValue, &maxValue, buffer, 1, framesConverted);
							}
#endif

							CARingBufferError result = mRingBuffer->Store(bufferList, 
																		  framesConverted, 
																		  startingFrameNumber + startTime);
							
							if(kCARingBufferError_OK != result)
								ERR("CARingBuffer::Store() failed: %i", result);
							
							OSAtomicAdd64Barrier(framesConverted, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded) {
							decoder->PerformDecodingFinishedCallback();
							
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update 
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							// Decoding is complete
							OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);

							decoderState = NULL;

							break;
						}
					}
					// Not enough space remains in the ring buffer to write an entire decoded chunk
					else
						break;
				}
				
				// Wait for the audio rendering thread to signal us that it could use more data, or for the timeout to happen
				semaphore_timedwait(mDecoderSemaphore, timeout);
			}
			
			// ========================================
			// Clean up
			if(NULL != bufferList)
				bufferList = DeallocateABL(bufferList);
			
			if(NULL != converter)
				delete converter, converter = NULL;
		}
		
		// Wait for the audio rendering thread to wake us, or for the timeout to happen
		semaphore_timedwait(mDecoderSemaphore, timeout);
	}
	
	return NULL;
}

void * AudioPlayer::CollectorThreadEntry()
{
	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };

	while(mKeepCollecting) {
		
		for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
			DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
			
			if(NULL == decoderState)
				continue;

			if(!(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) || !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags))
				continue;

			bool swapSucceeded = OSAtomicCompareAndSwapPtrBarrier(decoderState, NULL, reinterpret_cast<void **>(&mActiveDecoders[bufferIndex]));

			if(swapSucceeded)
				delete decoderState, decoderState = NULL;
		}
		
		// Wait for any thread to signal us to try and collect finished decoders
		semaphore_timedwait(mCollectorSemaphore, timeout);
	}
	
	return NULL;
}


#pragma mark AudioHardware Utilities


bool AudioPlayer::OpenOutput()
{
	// Create the IOProc which will feed audio to the device
	OSStatus result = AudioDeviceCreateIOProcID(mOutputDeviceID, 
												myIOProc, 
												this, 
												&mOutputDeviceIOProcID);
	
	if(noErr != result) {
		ERR("AudioDeviceCreateIOProcID failed: %i", result);
		return false;
	}

	// Register device property listeners
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDeviceProcessorOverload, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectAddPropertyListener (kAudioDeviceProcessorOverload) failed: %i", result);

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyBufferFrameSize) failed: %i", result);
		return false;
	}
		
#if DEBUG
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}
	
	propertyAddress.mSelector = kAudioObjectPropertyName;

	CFStringRef deviceName = NULL;
	UInt32 dataSize = sizeof(deviceName);
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										NULL, 
										&dataSize, 
										&deviceName);

	if(kAudioHardwareNoError == result) {
		CFRange range = CFRangeMake(0, CFStringGetLength(deviceName));
		CFIndex count;
		
		// Determine the length of the string in UTF-8
		CFStringGetBytes(deviceName, range, kCFStringEncodingUTF8, 0, false, NULL, 0, &count);
		
		char buf [count + 1];
		
		// Convert it
		CFIndex used;
		CFIndex converted = CFStringGetBytes(deviceName, range, kCFStringEncodingUTF8, 0, false, reinterpret_cast<UInt8 *>(buf), count, &used);
		
		if(CFStringGetLength(deviceName) != converted)
			LOG("CFStringGetBytes failed: converted %ld of %ld characters", converted, CFStringGetLength(deviceName));
		
		// Add terminator
		buf[used] = '\0';
		
		LOG("Opening output for device %#x (%s)", mOutputDeviceID, buf);
	}
	else
		ERR("AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: %i", result);

	if(deviceName)
		CFRelease(deviceName), deviceName = NULL;
#endif

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyStreams) failed: %i", result);

	// Get the device's stream information
	if(!GetOutputStreams(mOutputDeviceStreamIDs))
		return false;

	// Populate the virtual formats
	if(!BuildVirtualFormatsCache())
		return false;
	
	if(!AddVirtualFormatPropertyListeners())
		return false;

	mOutputConverters = new PCMConverter * [mOutputDeviceStreamIDs.size()];
	memset(mOutputConverters, 0, sizeof(mOutputConverters));

	return true;
}

bool AudioPlayer::CloseOutput()
{
	LOG("Closing output for device %#x", mOutputDeviceID);

	OSStatus result = AudioDeviceDestroyIOProcID(mOutputDeviceID, 
												 mOutputDeviceIOProcID);

	if(noErr != result) {
		ERR("AudioDeviceDestroyIOProcID failed: %i", result);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDeviceProcessorOverload, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};

	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectRemovePropertyListener (kAudioDeviceProcessorOverload) failed: %i", result);

#if DEBUG
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}
#endif

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyBufferFrameSize) failed: %i", result);
		return false;
	}
	
	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyStreams) failed: %i", result);

	RemoveVirtualFormatPropertyListeners();

	mOutputDeviceStreamIDs.clear();
	mStreamVirtualFormats.clear();

	delete [] mOutputConverters, mOutputConverters = NULL;
	
	return true;
}

bool AudioPlayer::StartOutput()
{
	LOG("Starting device %#x", mOutputDeviceID);

	OSStatus result = AudioDeviceStart(mOutputDeviceID, 
									   mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioDeviceStart failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::StopOutput()
{
	LOG("Stopping device %#x", mOutputDeviceID);

	OSStatus result = AudioDeviceStop(mOutputDeviceID, 
									  mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioDeviceStop failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::OutputIsRunning()
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyDeviceIsRunning, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};

	UInt32 isRunning = 0;
	UInt32 dataSize = sizeof(isRunning);

	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, 
												 &propertyAddress, 
												 0,
												 NULL, 
												 &dataSize,
												 &isRunning);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);
		return false;
	}
	
	return isRunning;
}

// NOT thread safe
bool AudioPlayer::ResetOutput()
{
	LOG("Resetting output");

	if(NULL != mSampleRateConverter) {
		OSStatus result = AudioConverterReset(mSampleRateConverter);
		
		if(noErr != result) {
			ERR("AudioConverterReset failed: %d", result);
			return false;
		}
	}

	return true;
}


#pragma mark Other Utilities


DecoderStateData * AudioPlayer::GetCurrentDecoderState()
{
	DecoderStateData *result = NULL;
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(NULL == decoderState)
			continue;
		
		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)
			continue;
		
		if(decoderState->mTotalFrames == decoderState->mFramesRendered)
			continue;
		
		if(NULL == result)
			result = decoderState;
		else if(decoderState->mTimeStamp < result->mTimeStamp)
			result = decoderState;
	}
	
	return result;
}

DecoderStateData * AudioPlayer::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp)
{
	DecoderStateData *result = NULL;
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(NULL == decoderState)
			continue;
		
		if(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)
			continue;

		if(NULL == result && decoderState->mTimeStamp > timeStamp)
			result = decoderState;
		else if(decoderState->mTimeStamp > timeStamp && decoderState->mTimeStamp < result->mTimeStamp)
			result = decoderState;
	}
	
	return result;
}

void AudioPlayer::StopActiveDecoders()
{
	// End any still-active decoders
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(NULL == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
		OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
	}
	
	// Signal the collector to collect 
	semaphore_signal(mDecoderSemaphore);
	semaphore_signal(mCollectorSemaphore);
}

bool AudioPlayer::CreateConvertersAndConversionBuffers()
{
	// Clean up
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(NULL != mOutputConverters[i])
			delete mOutputConverters[i], mOutputConverters[i] = NULL;
	}
	
	if(NULL != mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = NULL;
			
		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
	}
	
	if(NULL != mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);
	
	if(NULL != mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
	
	// Get the output buffer size for the device
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyBufferFrameSize,
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(mOutputDeviceBufferFrameSize);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &mOutputDeviceBufferFrameSize);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: %i", result);
		return false;
	}

	// FIXME: Handle devices with variable output buffer sizes
	
	AudioStreamBasicDescription outputBufferFormat = mRingBufferFormat;

	// Create a sample rate converter if required
	Float64 deviceSampleRate;
	if(!GetOutputDeviceSampleRate(deviceSampleRate)) {
		ERR("Unable to determine output device sample rate");
		return false;
	}
	
	if(deviceSampleRate != mRingBufferFormat.mSampleRate) {
		outputBufferFormat.mSampleRate = deviceSampleRate;
		
		result = AudioConverterNew(&mRingBufferFormat, &outputBufferFormat, &mSampleRateConverter);
		
		if(noErr != result) {
			ERR("AudioConverterNew failed: %i", result);
			return false;
		}
		
#if DEBUG
		fprintf(stderr, "Using sample rate converter: ");
		CAShow(mSampleRateConverter);
#endif
		
		// Calculate how large the sample rate conversion buffer must be
		UInt32 bufferSizeBytes = mOutputDeviceBufferFrameSize * outputBufferFormat.mBytesPerFrame;
		dataSize = sizeof(bufferSizeBytes);
		
		result = AudioConverterGetProperty(mSampleRateConverter, 
										   kAudioConverterPropertyCalculateInputBufferSize, 
										   &dataSize, 
										   &bufferSizeBytes);
		
		if(noErr != result) {
			ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
			return false;
		}
		
		// Allocate the sample rate conversion buffer (data is at the ring buffer's sample rate)
		mSampleRateConversionBuffer = AllocateABL(mRingBufferFormat, bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);
	}

	// Allocate the output buffer (data is at the device's sample rate)
	mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);
	
	// Determine the device's preferred stereo channels for output mapping
	propertyAddress.mSelector = kAudioDevicePropertyPreferredChannelsForStereo;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	
	UInt32 preferredStereoChannels [2] = { 1, 2 };
	if(AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		dataSize = sizeof(preferredStereoChannels);
		
		result = AudioObjectGetPropertyData(mOutputDeviceID,
											&propertyAddress,
											0,
											NULL,
											&dataSize,
											&preferredStereoChannels);	
		
		if(kAudioHardwareNoError != result)
			ERR("AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo) failed: %i", result);
	}

	LOG("Device preferred stereo channels: %d %d", preferredStereoChannels[0], preferredStereoChannels[1]);

	// For efficiency disable streams that aren't needed
	size_t streamUsageSize = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) + (sizeof(UInt32) * mOutputDeviceStreamIDs.size());
	AudioHardwareIOProcStreamUsage *streamUsage = static_cast<AudioHardwareIOProcStreamUsage *>(calloc(1, streamUsageSize));
	
	streamUsage->mIOProc = reinterpret_cast<void *>(mOutputDeviceIOProcID);
	streamUsage->mNumberStreams = static_cast<UInt32>(mOutputDeviceStreamIDs.size());

	// Create the output converter for each stream as required
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		AudioStreamID streamID = mOutputDeviceStreamIDs[i];

		LOG("Stream %#x information:", streamID);

		std::map<AudioStreamID, AudioStreamBasicDescription>::const_iterator virtualFormatIterator = mStreamVirtualFormats.find(streamID);
		if(mStreamVirtualFormats.end() == virtualFormatIterator) {
			ERR("Unknown virtual format for AudioStreamID %#x", streamID);
			return false;
		}

		AudioStreamBasicDescription virtualFormat = virtualFormatIterator->second;

#if DEBUG
		CAStreamBasicDescription streamVirtualFormat(virtualFormat);
		fprintf(stderr, "  Virtual format: ");
		streamVirtualFormat.Print(stderr);
#endif

		// Set up the channel mapping to determine if this stream is needed
		propertyAddress.mSelector = kAudioStreamPropertyStartingChannel;
		propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
		
		UInt32 startingChannel;
		dataSize = sizeof(startingChannel);
		
		result = AudioObjectGetPropertyData(streamID,
											&propertyAddress,
											0,
											NULL,
											&dataSize,
											&startingChannel);	

		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectGetPropertyData (kAudioStreamPropertyStartingChannel) failed: %i", result);
			return false;
		}
		
		LOG("  Starting channel: %d", startingChannel);
		
		UInt32 endingChannel = startingChannel + virtualFormat.mChannelsPerFrame;

		std::map<int, int> channelMap;

		// TODO: Handle files with non-standard channel layouts

		// Map mono to stereo using the preferred stereo channels
		if(1 == outputBufferFormat.mChannelsPerFrame)  {
			if(preferredStereoChannels[0] >= startingChannel && preferredStereoChannels[0] < endingChannel)
				channelMap[preferredStereoChannels[0] - 1] = 0;
			if(preferredStereoChannels[1] >= startingChannel && preferredStereoChannels[1] < endingChannel)
				channelMap[preferredStereoChannels[1] - 1] = 0;
		}
		// Stereo
		else if(2 == outputBufferFormat.mChannelsPerFrame) {
			if(preferredStereoChannels[0] >= startingChannel && preferredStereoChannels[0] < endingChannel)
				channelMap[preferredStereoChannels[0] - 1] = 0;
			if(preferredStereoChannels[1] >= startingChannel && preferredStereoChannels[1] < endingChannel)
				channelMap[preferredStereoChannels[1] - 1] = 1;
		}
		// Multichannel
		else {
			UInt32 channelCount = std::min(outputBufferFormat.mChannelsPerFrame, virtualFormat.mChannelsPerFrame);
			for(UInt32 channel = 1; channel <= channelCount; ++channel) {
				if(channel >= startingChannel && channel < endingChannel)
					channelMap[channel - 1] = channel - 1;
			}
		}

		// If the channel map isn't empty, the stream is used and an output converter is necessary
		if(!channelMap.empty()) {
			mOutputConverters[i] = new PCMConverter(outputBufferFormat, virtualFormat);			
			mOutputConverters[i]->SetChannelMap(channelMap);

#if DEBUG
			fprintf(stderr, "  Channel map: ");
			for(std::map<int, int>::const_iterator mapIterator = channelMap.begin(); mapIterator != channelMap.end(); ++mapIterator)
				fprintf(stderr, "%d -> %d  ", mapIterator->first, mapIterator->second);
			fputc('\n', stderr);
#endif

			streamUsage->mStreamIsOn[i] = true;
		}
	}

	// Disable the unneeded streams
	propertyAddress.mSelector = kAudioDevicePropertyIOProcStreamUsage;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;

	result = AudioObjectSetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, static_cast<UInt32>(streamUsageSize), streamUsage);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyIOProcStreamUsage) failed: %i", result);
		return false;
	}

	return true;
}

bool AudioPlayer::BuildVirtualFormatsCache()
{
	mStreamVirtualFormats.clear();

	for(std::vector<AudioStreamID>::const_iterator iter = mOutputDeviceStreamIDs.begin(); iter != mOutputDeviceStreamIDs.end(); ++iter) {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioStreamPropertyVirtualFormat,
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};
		
		AudioStreamBasicDescription virtualFormat;
		UInt32 dataSize = sizeof(virtualFormat);
		OSStatus result = AudioObjectGetPropertyData(*iter, 
													 &propertyAddress, 
													 0, 
													 NULL, 
													 &dataSize, 
													 &virtualFormat);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			return false;
		}
		
		mStreamVirtualFormats[*iter] = virtualFormat;		
	}
	
	return true;
}

bool AudioPlayer::AddVirtualFormatPropertyListeners()
{
	for(std::vector<AudioStreamID>::const_iterator iter = mOutputDeviceStreamIDs.begin(); iter != mOutputDeviceStreamIDs.end(); ++iter) {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioStreamPropertyVirtualFormat,
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};
		
		// Observe virtual format changes for the streams
		OSStatus result = AudioObjectAddPropertyListener(*iter,
														 &propertyAddress,
														 myAudioObjectPropertyListenerProc,
														 this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			return false;
		}
		
#if DEBUG
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectAddPropertyListener(*iter,
												&propertyAddress,
												myAudioObjectPropertyListenerProc,
												this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			return false;
		}
#endif
	}

	return true;
}

bool AudioPlayer::RemoveVirtualFormatPropertyListeners()
{
	for(std::vector<AudioStreamID>::const_iterator iter = mOutputDeviceStreamIDs.begin(); iter != mOutputDeviceStreamIDs.end(); ++iter) {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioStreamPropertyVirtualFormat,
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};
		
		OSStatus result = AudioObjectRemovePropertyListener(*iter,
															&propertyAddress,
															myAudioObjectPropertyListenerProc,
															this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			continue;
		}
		
#if DEBUG
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectRemovePropertyListener(*iter,
												   &propertyAddress,
												   myAudioObjectPropertyListenerProc,
												   this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			continue;
		}
#endif
	}
	
	return true;
}
