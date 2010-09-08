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
myConverterInputProc(AudioConverterRef				inAudioConverter,
					 UInt32							*ioNumberDataPackets,
					 AudioBufferList				*ioData,
					 AudioStreamPacketDescription	**outDataPacketDescription,
					 void*							inUserData)
{
	
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)
	
	assert(NULL != inUserData);
	assert(NULL != ioNumberDataPackets);
	
	AudioPlayer *player = static_cast<AudioPlayer *>(inUserData);
	
	return player->FillConversionBuffer(inAudioConverter, ioNumberDataPackets, ioData, outDataPacketDescription);
}


#pragma mark Creation/Destruction


AudioPlayer::AudioPlayer()
	: mOutputDeviceID(kAudioDeviceUnknown), mOutputDeviceIOProcID(NULL), mOutputStreamID(kAudioStreamUnknown), mIsPlaying(false), mFlags(0), mDecoderQueue(NULL), mRingBuffer(NULL), mSampleRateConverter(NULL), mConversionBuffer(NULL), mOutputConverter(NULL), mFramesDecoded(0), mFramesRendered(0)
{
	mDecoderQueue = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
	
	if(NULL == mDecoderQueue)
		throw std::bad_alloc();

	mRingBuffer = new CARingBuffer();

	memset(&mRingBufferFormat, 0, sizeof(AudioStreamBasicDescription));
	memset(&mStreamVirtualFormat, 0, sizeof(AudioStreamBasicDescription));

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

	// Clean up the converter and conversion buffer
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		
		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
		
		mSampleRateConverter = NULL;
	}

	if(mConversionBuffer)
		mConversionBuffer = DeallocateABL(mConversionBuffer);
	
	if(mOutputConverter)
		delete mOutputConverter, mOutputConverter = NULL;
	
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

bool AudioPlayer::GetOutputDeviceSampleRate(Float64& sampleRate)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(sampleRate);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &sampleRate);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetOutputDeviceSampleRate(Float64 sampleRate)
{
	LOG("Setting device %#x sample rate to %.0f Hz", mOutputDeviceID, sampleRate);

	OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagSampleRateChanging */, &mFlags);
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 sizeof(sampleRate),
												 &sampleRate);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagSampleRateChanging */, &mFlags);
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
	
	// The device isn't hogged, so attempt to hog it
	if(hogPID == static_cast<pid_t>(-1)) {
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
	}
	else
		LOG("Device is already hogged by pid: %d", hogPID);
	
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

bool AudioPlayer::SetOutputStreamID(AudioStreamID streamID)
{
	assert(kAudioStreamUnknown != streamID);
	
	// Get rid of any unneeded property listeners
	if(kAudioStreamUnknown != mOutputStreamID) {
		AudioObjectPropertyAddress propertyAddress = { 
			kAudioStreamPropertyVirtualFormat, 
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};

		OSStatus result = AudioObjectRemovePropertyListener(mOutputStreamID, 
															&propertyAddress, 
															myAudioObjectPropertyListenerProc, 
															this);
		
		if(kAudioHardwareNoError != result)
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);

#if DEBUG
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectRemovePropertyListener(mOutputStreamID, 
												   &propertyAddress, 
												   myAudioObjectPropertyListenerProc, 
												   this);
		
		if(kAudioHardwareNoError != result)
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
#endif
	}
	
	mOutputStreamID = streamID;
	
	// Get the stream's virtual format
	memset(&mStreamVirtualFormat, 0, sizeof(AudioStreamBasicDescription));
	if(false == GetOutputStreamVirtualFormat(mStreamVirtualFormat))
		return false;
	
	// Listen for changes to the stream's formats
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyVirtualFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectAddPropertyListener(mOutputStreamID, 
													 &propertyAddress, 
													 myAudioObjectPropertyListenerProc, 
													 this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
		return false;
	}

#if DEBUG
	propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
	
	result = AudioObjectAddPropertyListener(mOutputStreamID, 
											&propertyAddress, 
											myAudioObjectPropertyListenerProc, 
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
		return false;
	}
#endif
	
	return true;
}

bool AudioPlayer::GetOutputStreamVirtualFormat(AudioStreamBasicDescription& virtualFormat)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyVirtualFormat,
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(virtualFormat);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputStreamID,
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

bool AudioPlayer::SetOutputStreamVirtualFormat(const AudioStreamBasicDescription& virtualFormat)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyVirtualFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(mOutputStreamID,
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

bool AudioPlayer::GetOutputStreamPhysicalFormat(AudioStreamBasicDescription& physicalFormat)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyPhysicalFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(physicalFormat);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputStreamID,
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

bool AudioPlayer::SetOutputStreamPhysicalFormat(const AudioStreamBasicDescription& physicalFormat)
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyPhysicalFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectSetPropertyData(mOutputStreamID,
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

		if(!CreateConverterAndConversionBuffer())
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
	//	bool	channelLayoutsMatch		= ChannelLayoutsAreEqual(&nextChannelLayout, &mChannelLayout);
		
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

	// Stop output during sample rate changes, to prevent glitching
	if(eAudioPlayerFlagSampleRateChanging & mFlags) {
		StopOutput();
		return kAudioHardwareNoError;		
	}

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

	// If an output converter doesn't exist, return silence
	if(NULL == mOutputConverter)
		return kAudioHardwareNoError;

	UInt32 desiredFrames = outOutputData->mBuffers[0].mDataByteSize / mStreamVirtualFormat.mBytesPerFrame;

	// Reset state
	mFramesRenderedLastPass = 0;
	
	UInt32 framesToRead = std::min(framesAvailableToRead, desiredFrames);

	// Convert the sample rate, if required
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterFillComplexBuffer(mSampleRateConverter, 
														  myConverterInputProc,
														  this,
														  &framesToRead, 
														  mConversionBuffer,
														  NULL);
		
		if(noErr != result)
			ERR("AudioConverterFillComplexBuffer failed: %i", result);		
	}
	else {
		CARingBufferError result = mRingBuffer->Fetch(mConversionBuffer, framesToRead, mFramesRendered, false);
		
		if(kCARingBufferError_OK != result) {
			ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
			return ioErr;
		}

		OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
		
		mFramesRenderedLastPass += framesToRead;
	}

	// Convert to the output device's format
	UInt32 framesConverted = mOutputConverter->Convert(mConversionBuffer, outOutputData, framesToRead);
	
	if(framesConverted != framesToRead)
		ERR("Conversion to output format failed; all frames may not be rendered");
	
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
#endif

				case kAudioDevicePropertyStreams:
				{
					OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					if(OutputIsRunning())
						StopOutput();

					// If the streams changed, the virtual format may have changed as well					
					UInt32 dataSize = sizeof(currentAddress);
					
					OSStatus result = AudioObjectGetPropertyDataSize(inObjectID, 
																	 &currentAddress, 
																	 0,
																	 NULL,
																	 &dataSize);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: %i", result);
						continue;
					}
					
					UInt32 streamCount = static_cast<UInt32>(dataSize / sizeof(AudioStreamID));
					AudioStreamID audioStreams [streamCount];
					
					if(1 != streamCount)
						LOG("Found %i AudioStream(s) on device %#x", streamCount, mOutputDeviceID);
					
					result = AudioObjectGetPropertyData(inObjectID, 
														&currentAddress, 
														0, 
														NULL, 
														&dataSize, 
														audioStreams);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: %i", result);
						continue;
					}
					
					// For now, use the first stream
					if(false == SetOutputStreamID(audioStreams[0])) 
						ERR("Unable to set output stream ID");

					if(false == CreateConverterAndConversionBuffer())
						ERR("Couldn't create AudioConverter");

					if(IsPlaying() && !OutputIsRunning() && !(eAudioPlayerFlagSampleRateChanging & mFlags))
						StartOutput();

					OSAtomicTestAndClearBarrier(5 /* eAudioPlayerFlagMuteOutput */, &mFlags);
					
					LOG("-> kAudioDevicePropertyStreams [%#x] changed, using %#x", inObjectID, audioStreams[0]);

					break;
				}

				case kAudioDevicePropertyNominalSampleRate:
				{
					Float64 sampleRate = 0;
					UInt32 dataSize = sizeof(sampleRate);
					
					OSStatus result = AudioObjectGetPropertyData(inObjectID, 
																 &currentAddress, 
																 0,
																 NULL, 
																 &dataSize,
																 &sampleRate);
					
					if(kAudioHardwareNoError != result) {
						ERR("AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
						continue;
					}
					
					LOG("-> kAudioDevicePropertyNominalSampleRate [%#x]: %.0f Hz", inObjectID, sampleRate);

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagSampleRateChanging */, &mFlags);
					
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
	else if(inObjectID == mOutputStreamID) {
		for(UInt32 addressIndex = 0; addressIndex < inNumberAddresses; ++addressIndex) {
			AudioObjectPropertyAddress currentAddress = inAddresses[addressIndex];
			
			switch(currentAddress.mSelector) {
				case kAudioStreamPropertyVirtualFormat:
				{
					OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					if(OutputIsRunning())
						StopOutput();

					// Get the new virtual format
					memset(&mStreamVirtualFormat, 0, sizeof(AudioStreamBasicDescription));
					if(false == GetOutputStreamVirtualFormat(mStreamVirtualFormat))
						ERR("Couldn't get stream virtual format");

#if DEBUG
					else {
						CAStreamBasicDescription virtualFormat(mStreamVirtualFormat);
						fprintf(stderr, "-> kAudioStreamPropertyVirtualFormat [%#x]: ", inObjectID);
						virtualFormat.Print(stderr);
					}
#endif

					if(false == CreateConverterAndConversionBuffer())
						ERR("Couldn't create AudioConverter");

					if(IsPlaying() && !OutputIsRunning() && !(eAudioPlayerFlagSampleRateChanging & mFlags))
						StartOutput();

					OSAtomicTestAndClearBarrier(5 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					break;
				}
					
#if DEBUG
				case kAudioStreamPropertyPhysicalFormat:
				{
					// Get the new physical format
					CAStreamBasicDescription physicalFormat;
					if(false == GetOutputStreamPhysicalFormat(physicalFormat))
						ERR("Couldn't get stream physical format");
					else {
						fprintf(stderr, "-> kAudioStreamPropertyPhysicalFormat [%#x]: ", inObjectID);
						physicalFormat.Print(stderr);
					}
					break;
				}
#endif
			}
		}
	}
	
	return kAudioHardwareNoError;			
}

OSStatus AudioPlayer::FillConversionBuffer(AudioConverterRef			inAudioConverter,
										   UInt32						*ioNumberDataPackets,
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

	CARingBufferError result = mRingBuffer->Fetch(mConversionBuffer, framesToRead, mFramesRendered, false);

	if(kCARingBufferError_OK != result) {
		ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
		*ioNumberDataPackets = 0;
		return ioErr;
	}
	
	OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
	
	// This may be called multiple times from AudioConverterFillComplexBuffer, so keep an additive tally
	// of how many frames were rendered
	mFramesRenderedLastPass += framesToRead;

	// Point ioData at our decoded audio
	ioData->mNumberBuffers = mConversionBuffer->mNumberBuffers;
	for(UInt32 bufferIndex = 0; bufferIndex < mConversionBuffer->mNumberBuffers; ++bufferIndex)
		ioData->mBuffers[bufferIndex] = mConversionBuffer->mBuffers[bufferIndex];
	
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
							double minValue = -1;
							double maxValue = ((1u << (bitsPerChannel - 1)) - 1) / (1u << (bitsPerChannel - 1));
							
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
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDeviceProcessorOverload, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 dataSize = sizeof(mOutputDeviceID);
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectAddPropertyListener (kAudioDeviceProcessorOverload) failed: %i", result);

#if DEBUG
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);
#endif

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}

#if DEBUG
	propertyAddress.mSelector = kAudioObjectPropertyName;

	CFStringRef deviceName = NULL;
	dataSize = sizeof(deviceName);
	
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

    result = AudioObjectGetPropertyDataSize(mOutputDeviceID, 
											&propertyAddress, 
											0,
											NULL,
											&dataSize);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}
	
	UInt32 streamCount = static_cast<UInt32>(dataSize / sizeof(AudioStreamID));
	AudioStreamID audioStreams [streamCount];
	
	if(1 != streamCount)
		LOG("Found %i AudioStream(s) on device %x", streamCount, mOutputDeviceID);
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, 
										&propertyAddress, 
										0, 
										NULL, 
										&dataSize, 
										audioStreams);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}
	
	// For now, use the first stream
	if(false == SetOutputStreamID(audioStreams[0]))
		return false;
	
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
#endif

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyStreams) failed: %i", result);

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

bool AudioPlayer::CreateConverterAndConversionBuffer()
{
	// Clean up
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		
		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
		
		mSampleRateConverter = NULL;
	}
	
	if(mConversionBuffer)
		mConversionBuffer = DeallocateABL(mConversionBuffer);
	
	if(mOutputConverter)
		delete mOutputConverter, mOutputConverter = NULL;

	// Get the output buffer size for the device
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyBufferFrameSize,
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	UInt32 bufferSizeFrames = 0;
	UInt32 dataSize = sizeof(bufferSizeFrames);
	
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID,
												 &propertyAddress,
												 0,
												 NULL,
												 &dataSize,
												 &bufferSizeFrames);	
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: %i", result);
		return false;
	}

	AudioStreamBasicDescription conversionBufferFormat = mRingBufferFormat;

	// Create a sample rate converter if necessary
	if(conversionBufferFormat.mSampleRate != mStreamVirtualFormat.mSampleRate) {
		conversionBufferFormat.mSampleRate = mStreamVirtualFormat.mSampleRate;

		result = AudioConverterNew(&mRingBufferFormat, &conversionBufferFormat, &mSampleRateConverter);
		
		if(noErr != result) {
			ERR("AudioConverterNew failed: %i", result);
			return false;
		}
		
#if DEBUG
		fprintf(stderr, "Using sample rate converter: ");
		CAShow(mSampleRateConverter);
#endif
		
		// Calculate how large the conversion buffer must be
		UInt32 bufferSizeBytes = bufferSizeFrames * conversionBufferFormat.mBytesPerFrame;
		dataSize = sizeof(bufferSizeBytes);
		
		result = AudioConverterGetProperty(mSampleRateConverter, 
										   kAudioConverterPropertyCalculateInputBufferSize, 
										   &dataSize, 
										   &bufferSizeBytes);
		
		if(noErr != result) {
			ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
			return false;
		}
		
		bufferSizeFrames = std::max(bufferSizeFrames, bufferSizeBytes / conversionBufferFormat.mBytesPerFrame);
	}

	// Allocate the conversion buffer (data is read from the ring buffer into this buffer before conversion for output)
	mConversionBuffer = AllocateABL(conversionBufferFormat, bufferSizeFrames);
	mOutputConverter = new PCMConverter(conversionBufferFormat, mStreamVirtualFormat);

	return true;
}
