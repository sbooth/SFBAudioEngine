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
#include <stdexcept>
#include <new>

#include "AudioEngineDefines.h"
#include "AudioPlayer.h"
#include "AudioDecoder.h"
#include "DecoderStateData.h"
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
// Utility functions
// ========================================
static AudioBufferList *
allocateBufferList(UInt32 channelsPerFrame, UInt32 bytesPerFrame, bool interleaved, UInt32 capacityFrames)
{
	AudioBufferList *bufferList = NULL;

	UInt32 numBuffers = interleaved ? 1 : channelsPerFrame;
	UInt32 channelsPerBuffer = interleaved ? channelsPerFrame : 1;
	
	bufferList = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * numBuffers)));
	
	bufferList->mNumberBuffers = numBuffers;
	
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
		bufferList->mBuffers[bufferIndex].mData = static_cast<void *>(calloc(capacityFrames, bytesPerFrame));
		bufferList->mBuffers[bufferIndex].mDataByteSize = capacityFrames * bytesPerFrame;
		bufferList->mBuffers[bufferIndex].mNumberChannels = channelsPerBuffer;
	}
	
	return bufferList;
}

static void
deallocateBufferList(AudioBufferList *bufferList)
{
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		free(bufferList->mBuffers[bufferIndex].mData), bufferList->mBuffers[bufferIndex].mData = NULL;
	
	free(bufferList), bufferList = NULL;
}

static bool
channelLayoutsAreEqual(AudioChannelLayout *lhs,
					   AudioChannelLayout *rhs)
{
	assert(NULL != lhs);
	assert(NULL != rhs);
	
	// First check if the tags are equal
	if(lhs->mChannelLayoutTag != rhs->mChannelLayoutTag)
		return false;
	
	// If the tags are equal, check for special values
	if(kAudioChannelLayoutTag_UseChannelBitmap == lhs->mChannelLayoutTag)
		return (lhs->mChannelBitmap == rhs->mChannelBitmap);
	
	if(kAudioChannelLayoutTag_UseChannelDescriptions == lhs->mChannelLayoutTag) {
		if(lhs->mNumberChannelDescriptions != rhs->mNumberChannelDescriptions)
			return false;
		
		size_t bytesToCompare = lhs->mNumberChannelDescriptions * sizeof(AudioChannelDescription);
		return (0 == memcmp(&lhs->mChannelDescriptions, &rhs->mChannelDescriptions, bytesToCompare));
	}
	
	return true;
}

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

// ========================================
// AudioConverter input callback
// ========================================
static OSStatus
myDeinterleaverInputProc(AudioConverterRef				inAudioConverter,
						 UInt32							*ioNumberDataPackets,
						 AudioBufferList				*ioData,
						 AudioStreamPacketDescription	**outDataPacketDescription,
						 void*							inUserData)
{
	
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)
	
	assert(NULL != inUserData);
	assert(NULL != ioNumberDataPackets);
	
	DecoderStateData *decoderStateData = static_cast<DecoderStateData *>(inUserData);
	
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
	: mOutputDeviceID(kAudioDeviceUnknown), mOutputDeviceIOProcID(NULL), mOutputStreamID(kAudioStreamUnknown), mIsPlaying(false), mFlags(0), mDecoderQueue(NULL), mRingBuffer(NULL), mConverter(NULL), mPCMConverter(NULL), mConversionBuffer(NULL), mFramesDecoded(0), mFramesRendered(0)
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
	if(mConverter) {
		OSStatus result = AudioConverterDispose(mConverter);
		
		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
		
		mConverter = NULL;
	}
	
	if(mPCMConverter)
		delete mPCMConverter, mPCMConverter = NULL;
	
	if(mConversionBuffer)
		deallocateBufferList(mConversionBuffer), mConversionBuffer = NULL;
	
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
	assert(0 <= frame);

	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	if(false == currentDecoderState->mDecoder->SupportsSeeking())
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
			kAudioStreamPropertyPhysicalFormat, 
			kAudioObjectPropertyScopeGlobal, 
			kAudioObjectPropertyElementMaster 
		};

		OSStatus result = AudioObjectRemovePropertyListener(mOutputStreamID, 
															&propertyAddress, 
															myAudioObjectPropertyListenerProc, 
															this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
			return false;
		}

		propertyAddress.mSelector = kAudioStreamPropertyVirtualFormat;
		
		result = AudioObjectRemovePropertyListener(mOutputStreamID, 
												   &propertyAddress, 
												   myAudioObjectPropertyListenerProc, 
												   this);
		
		if(kAudioHardwareNoError != result) {
			ERR("AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
			return false;
		}
	}
	
	mOutputStreamID = streamID;
	
	// Get the stream's virtual format
	if(false == GetOutputStreamVirtualFormat(mStreamVirtualFormat))
		return false;
	
	// Listen for changes to the stream's physical format
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioStreamPropertyPhysicalFormat, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};
	
	OSStatus result = AudioObjectAddPropertyListener(mOutputStreamID, 
													 &propertyAddress, 
													 myAudioObjectPropertyListenerProc, 
													 this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyPhysicalFormat) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioStreamPropertyVirtualFormat;
	
	result = AudioObjectAddPropertyListener(mOutputStreamID, 
											&propertyAddress, 
											myAudioObjectPropertyListenerProc, 
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: %i", result);
		return false;
	}
	
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
		// The ring buffer's format will be the same as the decoder's, except
		// the ring buffer will contain deinterleaved samples
		mRingBufferFormat = decoder->GetFormat();
		
		if(!(kAudioFormatFlagIsNonInterleaved & mRingBufferFormat.mFormatFlags)) {
			mRingBufferFormat.mFormatFlags		|= kAudioFormatFlagIsNonInterleaved;			
			mRingBufferFormat.mBytesPerFrame	/= mRingBufferFormat.mChannelsPerFrame;
			mRingBufferFormat.mBytesPerPacket	/= mRingBufferFormat.mChannelsPerFrame;
		}

		CreateConverterAndConversionBuffer();
		
		// Allocate enough space in the ring buffer for the new format
		mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame,
							  mRingBufferFormat.mBytesPerFrame,
							  RING_BUFFER_SIZE_FRAMES);
	}
	// Otherwise, enqueue this decoder if the format matches
	else {
		AudioStreamBasicDescription		nextFormat			= decoder->GetFormat();
	//	AudioChannelLayout				nextChannelLayout	= decoder->GetChannelLayout();
		
		// The channels will be deinterleaved after decoding and before storage in the ring buffer
		if(!(kAudioFormatFlagIsNonInterleaved & nextFormat.mFormatFlags)) {
			nextFormat.mFormatFlags		|= kAudioFormatFlagIsNonInterleaved;			
			nextFormat.mBytesPerFrame	/= nextFormat.mChannelsPerFrame;
			nextFormat.mBytesPerPacket	/= nextFormat.mChannelsPerFrame;
		}
		
		bool	formatsMatch			= (0 == memcmp(&nextFormat, &mRingBufferFormat, sizeof(mRingBufferFormat)));
	//	bool	channelLayoutsMatch		= channelLayoutsAreEqual(&nextChannelLayout, &mChannelLayout);
		
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

	// If the stream's virtual format changed and IO is running, stop it immediately or bad things will happen
	if(eAudioPlayerFlagVirtualFormatChanged & mFlags) {
		StopOutput();
		
		// The buffers are pre-zeroed so just return
		return kAudioHardwareNoError;
	}

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

	UInt32 desiredFrames = outOutputData->mBuffers[0].mDataByteSize / mStreamVirtualFormat.mBytesPerFrame;

	// Reset state
	mFramesRenderedLastPass = 0;
	
	if(mPCMConverter) {
		UInt32 framesToRead = std::min(framesAvailableToRead, desiredFrames);
		
		CARingBufferError result = mRingBuffer->Fetch(mConversionBuffer, framesToRead, mFramesRendered, false);
		
		if(kCARingBufferError_OK != result) {
			ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
			return ioErr;
		}

		UInt32 framesConverted = mPCMConverter->Convert(mConversionBuffer, outOutputData, framesToRead);
		
		if(framesConverted != framesToRead)
			ERR("PCMConverter::Convert failed");
		
		OSAtomicAdd64Barrier(framesConverted, &mFramesRendered);
		
		mFramesRenderedLastPass += framesToRead;
	}
	else {
		OSStatus result = AudioConverterFillComplexBuffer(mConverter, 
														  myConverterInputProc,
														  this,
														  &desiredFrames, 
														  outOutputData,
														  NULL);
		
		if(noErr != result)
			ERR("AudioConverterFillComplexBuffer failed: %i", result);		
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
	// ========================================
	// AudioDevice properties
	if(inObjectID == mOutputDeviceID) {
		for(UInt32 addressIndex = 0; addressIndex < inNumberAddresses; ++addressIndex) {
			AudioObjectPropertyAddress currentAddress = inAddresses[addressIndex];

			switch(currentAddress.mSelector) {
				case kAudioDevicePropertyDeviceIsRunning:
				{
#if DEBUG
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

					LOG("-> kAudioDevicePropertyDeviceIsRunning is %s", isRunning ? "True" : "False");
#endif

					break;
				}
					
				case kAudioDevicePropertyStreams:
				{
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
						LOG("Found %i AudioStream(s) on device %x", streamCount, mOutputDeviceID);
					
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

					break;
				}
					
				case kAudioDeviceProcessorOverload:
					ERR("kAudioDeviceProcessorOverload: Unable to meet IOProc time constraints");
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
					// Stop IO
					StopOutput();
					
					// Changing virtual formats involves numerous thread-unsafe operations
					// Once this flag is set, rendering will cease until it is clear
					OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagVirtualFormatChanged */, &mFlags);

					// Get the new virtual format
					if(false == GetOutputStreamVirtualFormat(mStreamVirtualFormat))
						ERR("Couldn't get stream virtual format");

#if DEBUG
					else {
						CAStreamBasicDescription virtualFormat(mStreamVirtualFormat);
						fprintf(stderr, "-> Virtual format changed: ");
						virtualFormat.Print(stderr);
					}
#endif

					if(false == CreateConverterAndConversionBuffer())
						ERR("Couldn't create AudioConverter");
					
					// It is now safe to resume rendering
					OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagVirtualFormatChanged */, &mFlags);

					if(IsPlaying())
						StartOutput();
					
					break;
				}
					
				case kAudioStreamPropertyPhysicalFormat:
				{
#if DEBUG
					// Get the new physical format
					CAStreamBasicDescription physicalFormat;
					if(false == GetOutputStreamPhysicalFormat(physicalFormat))
						ERR("Couldn't get stream physical format");
					else {
						fprintf(stderr, "-> Physical format changed: ");
						physicalFormat.Print(stderr);
					}
#endif

					break;
				}
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
			
			// ========================================
			// Create the AudioConverter which will deinterleave the decoder's format
			AudioConverterRef audioConverter = NULL;
			OSStatus result = AudioConverterNew(&decoderFormat, &mRingBufferFormat, &audioConverter);
			
			if(noErr != result) {
				ERR("AudioConverterNew failed: %i", result);
				
				// If this happens, output will be impossible
				OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
			}
			
			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
			UInt32 inputBufferSize = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES * mRingBufferFormat.mBytesPerFrame;
			UInt32 dataSize = sizeof(inputBufferSize);
			result = AudioConverterGetProperty(audioConverter, 
											   kAudioConverterPropertyCalculateInputBufferSize, 
											   &dataSize, 
											   &inputBufferSize);
			
			if(noErr != result)
				ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
			
			decoderState->AllocateBufferList(inputBufferSize / decoderFormat.mBytesPerFrame);

			// CARingBuffer only handles deinterleaved channels
			AudioBufferList *bufferList = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * mRingBufferFormat.mChannelsPerFrame)));
			
			bufferList->mNumberBuffers = mRingBufferFormat.mChannelsPerFrame;
			
			for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
				bufferList->mBuffers[bufferIndex].mData = static_cast<void *>(calloc(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES, mRingBufferFormat.mBytesPerFrame));
				bufferList->mBuffers[bufferIndex].mDataByteSize = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES * mRingBufferFormat.mBytesPerFrame;
				bufferList->mBuffers[bufferIndex].mNumberChannels = 1;
			}
			
			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			bool decodingFinished = false;
			while(!decodingFinished && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)) {
				
				// Fill the ring buffer with as much data as possible
				while(!decodingFinished) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = static_cast<UInt32>(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));
					
					// Force writes to the ring buffer to be at least RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES
					if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite) {
						
						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagIsSeeking */, &mFlags);
							
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
								
								// Reset the converter and output to flush any buffers
								result = AudioConverterReset(audioConverter);
								
								if(noErr != result)
									ERR("AudioConverterReset failed: %i", result);

								// This is safe to call at this point, because eAudioPlayerFlagIsSeeking is set so
								// no rendering is being performed
								ResetOutput();
							}

							OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagIsSeeking */, &mFlags);
						}
						
						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						// Read the input chunk, deinterleaving as necessary
						UInt32 framesDecoded = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES;
						
						result = AudioConverterFillComplexBuffer(audioConverter, 
																 myDeinterleaverInputProc,
																 decoderState,
																 &framesDecoded, 
																 bufferList,
																 NULL);
						
						if(noErr != result)
							ERR("AudioConverterFillComplexBuffer failed: %i", result);

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber)
							decoder->PerformDecodingStartedCallback();
						
						// Store the decoded audio
						if(0 != framesDecoded) {
							result = mRingBuffer->Store(bufferList, 
														framesDecoded, 
														startingFrameNumber + startTime);
							
							if(kCARingBufferError_OK != result)
								ERR("CARingBuffer::Store() failed: %i", result);
							
							OSAtomicAdd64Barrier(framesDecoded, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded) {
							decoder->PerformDecodingFinishedCallback();
							
							// Free unneeded memory
							decoderState->DeallocateBufferList();
							
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update 
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							// Decoding is complete
							// The local variable decodingFinished is necessary because once 
							// eDecoderStateDataFlagDecodingFinished is set, the decoderStateData object may
							// be freed at any time by the collector thread and thus may no longer be accessed
							decodingFinished = true;

							OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
							
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
			if(NULL != bufferList) {
				for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
					free(bufferList->mBuffers[bufferIndex].mData), bufferList->mBuffers[bufferIndex].mData = NULL;
				
				free(bufferList), bufferList = NULL;
			}
			
			if(NULL != audioConverter) {
				result = AudioConverterDispose(audioConverter);
				audioConverter = NULL;
				if(noErr != result)
					ERR("AudioConverterDispose failed: %i", result);
			}
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
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDeviceProcessorOverload) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);
		return false;
	}
	
	// Listen for changes to the device's sample rate and streams
	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;

    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectAddPropertyListener (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	
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
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDeviceProcessorOverload) failed: %i", result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: %i", result);
		return false;
	}

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
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectRemovePropertyListener (kAudioDevicePropertyStreams) failed: %i", result);
		return false;
	}

	return true;
}

bool AudioPlayer::StartOutput()
{
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
		kAudioDevicePropertyScopeInput, 
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
	if(NULL != mConverter) {
		OSStatus result = AudioConverterReset(mConverter);
		
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
	if(mConverter) {
		OSStatus result = AudioConverterDispose(mConverter);
		
		if(noErr != result)
			ERR("AudioConverterDispose failed: %i", result);
		
		mConverter = NULL;
	}
	
	if(mPCMConverter)
		delete mPCMConverter, mPCMConverter = NULL;
	
	if(mConversionBuffer)
		deallocateBufferList(mConversionBuffer), mConversionBuffer = NULL;
	
	// Get the output buffer size for the stream
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

	// Use PCMConverter for integer to integer conversion with no sample rate conversions or channel mapping
	if(kAudioFormatLinearPCM == mRingBufferFormat.mFormatID &&
	   kAudioFormatLinearPCM == mStreamVirtualFormat.mFormatID &&
	   mRingBufferFormat.mSampleRate == mStreamVirtualFormat.mSampleRate &&
	   mRingBufferFormat.mChannelsPerFrame == mStreamVirtualFormat.mChannelsPerFrame &&
	   !(kAudioFormatFlagIsFloat & mRingBufferFormat.mFormatFlags) &&
	   !(kAudioFormatFlagIsFloat & mStreamVirtualFormat.mFormatFlags)) {
		mPCMConverter = new PCMConverter(mRingBufferFormat, mStreamVirtualFormat);

		// Allocate the conversion buffer
		bool ringBufferFormatIsInterleaved = !(kAudioFormatFlagIsNonInterleaved & mRingBufferFormat.mFormatFlags);
		
		mConversionBuffer = allocateBufferList(mRingBufferFormat.mChannelsPerFrame, 
											   mRingBufferFormat.mBytesPerFrame, 
											   ringBufferFormatIsInterleaved, 
											   bufferSizeFrames);
	}
	else {	
		// Create the AudioConverter which will convert from the decoder's format to the stream's virtual format
		result = AudioConverterNew(&mRingBufferFormat, &mStreamVirtualFormat, &mConverter);
		
		if(noErr != result) {
			ERR("AudioConverterNew failed: %i", result);
			return false;
		}
		
		// Calculate how large the conversion buffer must be
		UInt32 bufferSizeBytes = bufferSizeFrames * mStreamVirtualFormat.mBytesPerFrame;
		dataSize = sizeof(bufferSizeBytes);
		
		result = AudioConverterGetProperty(mConverter, 
										   kAudioConverterPropertyCalculateInputBufferSize, 
										   &dataSize, 
										   &bufferSizeBytes);
		
		if(noErr != result) {
			ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
			return false;
		}
		
		// Allocate the conversion buffer
		bool ringBufferFormatIsInterleaved = !(kAudioFormatFlagIsNonInterleaved & mRingBufferFormat.mFormatFlags);
		
		mConversionBuffer = allocateBufferList(mRingBufferFormat.mChannelsPerFrame, 
											   mRingBufferFormat.mBytesPerFrame, 
											   ringBufferFormatIsInterleaved, 
											   bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);
	}
	
	return true;
}
