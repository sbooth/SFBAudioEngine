/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Stephen F. Booth <me@sbooth.org>
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
#include <iomanip>

#include <log4cxx/logger.h>
#include <log4cxx/logmanager.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/ndc.h>

#include "AudioPlayer.h"
#include "AudioDecoder.h"
#include "DecoderStateData.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "ChannelLayoutsAreEqual.h"
#include "DeinterleavingFloatConverter.h"
#include "PCMConverter.h"
#include "CFOperatorOverloads.h"
#include "CreateChannelLayout.h"

#include "CARingBuffer.h"

// ========================================
// Macros
// ========================================
#define RING_BUFFER_SIZE_FRAMES					16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6
#define SLEEP_TIME_USEC							1000

static void InitializationLoggingSubsystem() __attribute__ ((constructor));
static void InitializationLoggingSubsystem()
{
	// Turn off logging by default
	if(!log4cxx::LogManager::getLoggerRepository()->isConfigured())
		log4cxx::Logger::getRootLogger()->setLevel(log4cxx::Level::getOff());
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
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_WARN(logger, "Couldn't set thread's extended policy: " << mach_error_string(error));
		return false;
	}
	
	// Give the thread the specified importance
	thread_precedence_policy_data_t precedencePolicy = { importance };
	error = thread_policy_set(mach_thread_self(), 
							  THREAD_PRECEDENCE_POLICY, 
							  (thread_policy_t)&precedencePolicy, 
							  THREAD_PRECEDENCE_POLICY_COUNT);
	
	if (error != KERN_SUCCESS) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_WARN(logger, "Couldn't set thread's precedence policy: " << mach_error_string(error));
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
	: mOutputDeviceID(kAudioDeviceUnknown), mOutputDeviceIOProcID(NULL), mOutputDeviceBufferFrameSize(0), mFlags(0), mDecoderQueue(NULL), mRingBuffer(NULL), mRingBufferChannelLayout(NULL), mOutputConverters(NULL), mSampleRateConverter(NULL), mSampleRateConversionBuffer(NULL), mOutputBuffer(NULL), mFramesDecoded(0), mFramesRendered(0), mDigitalVolume(1.0), mDigitalPreGain(0.0)
{
	mDecoderQueue = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
	
	if(NULL == mDecoderQueue)
		throw std::bad_alloc();

	mRingBuffer = new CARingBuffer();

	// ========================================
	// Create the semaphore and mutex to be used by the decoding and rendering threads
	kern_return_t result = semaphore_create(mach_task_self(), &mDecoderSemaphore, SYNC_POLICY_FIFO, 0);
	if(KERN_SUCCESS != result) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "semaphore_create failed: " << mach_error_string(result));

		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		throw std::runtime_error("semaphore_create failed");
	}

	result = semaphore_create(mach_task_self(), &mCollectorSemaphore, SYNC_POLICY_FIFO, 0);
	if(KERN_SUCCESS != result) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "semaphore_create failed: " << mach_error_string(result));
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));
		
		throw std::runtime_error("semaphore_create failed");
	}
	
	int success = pthread_mutex_init(&mMutex, NULL);
	if(0 != success) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "pthread_mutex_init failed: " << strerror(success));
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));

		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));

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
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "pthread_create failed: " << strerror(creationResult));
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));

		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));
		
		throw std::runtime_error("pthread_create failed");
	}
	
	// ========================================
	// Launch the collector thread
	mKeepCollecting = true;
	creationResult = pthread_create(&mCollectorThread, NULL, collectorEntry, this);
	if(0 != creationResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "pthread_create failed: " << strerror(creationResult));
		
		mKeepDecoding = false;
		kern_return_t error = semaphore_signal(mDecoderSemaphore);
		if(KERN_SUCCESS != error)
			LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
		
		int joinResult = pthread_join(mDecoderThread, NULL);
		if(0 != joinResult)
			LOG4CXX_WARN(logger, "pthread_join failed: " << strerror(joinResult));
		
		mDecoderThread = static_cast<pthread_t>(0);
		
		CFRelease(mDecoderQueue), mDecoderQueue = NULL;
		delete mRingBuffer, mRingBuffer = NULL;

		result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));
		
		result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
		if(KERN_SUCCESS != result)
			LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));

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
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << hwResult);
		throw std::runtime_error("AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed");
	}

	if(!OpenOutput()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_FATAL(logger, "OpenOutput() failed");
		throw std::runtime_error("OpenOutput() failed");
	}
}

AudioPlayer::~AudioPlayer()
{
	// Stop the processing graph and reclaim its resources
	if(!CloseOutput()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_ERROR(logger, "CloseOutput() failed");
	}

	// Dispose of all active decoders
	StopActiveDecoders();
	
	// End the decoding thread
	mKeepDecoding = false;
	kern_return_t error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	}

	int joinResult = pthread_join(mDecoderThread, NULL);
	if(0 != joinResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_ERROR(logger, "pthread_join failed: " << strerror(joinResult));
	}
	
	mDecoderThread = static_cast<pthread_t>(0);

	// End the collector thread
	mKeepCollecting = false;
	error = semaphore_signal(mCollectorSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the collector semaphore: " << mach_error_string(error));
	}
	
	joinResult = pthread_join(mCollectorThread, NULL);
	if(0 != joinResult) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_ERROR(logger, "pthread_join failed: " << strerror(joinResult));
	}
	
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

	// Clean up the ring buffer and associated resources
	if(mRingBuffer)
		delete mRingBuffer, mRingBuffer = NULL;

	if(mRingBufferChannelLayout)
		free(mRingBufferChannelLayout), mRingBufferChannelLayout = NULL;

	// Clean up the converters and conversion buffers
	if(mOutputConverters) {
		for(UInt32 i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
			delete mOutputConverters[i], mOutputConverters[i] = NULL;
		delete [] mOutputConverters, mOutputConverters = NULL;
	}
	
	if(mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = NULL;

		if(noErr != result) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
			LOG4CXX_ERROR(logger, "AudioConverterDispose failed: " << result);
		}
	}

	if(mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);

	if(mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
	
	// Destroy the decoder and collector semaphores
	kern_return_t result = semaphore_destroy(mach_task_self(), mDecoderSemaphore);
	if(KERN_SUCCESS != result) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));
	}

	result = semaphore_destroy(mach_task_self(), mCollectorSemaphore);
	if(KERN_SUCCESS != result) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_WARN(logger, "semaphore_destroy failed: " << mach_error_string(result));
	}
	
	// Destroy the decoder mutex
	int success = pthread_mutex_destroy(&mMutex);
	if(0 != success) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");
		LOG4CXX_ERROR(logger, "pthread_mutex_destroy failed: " << strerror(success));
	}
}

#pragma mark Playback Control

void AudioPlayer::Play()
{
	if(!IsPlaying())
		StartOutput();
}

void AudioPlayer::Pause()
{
	if(IsPlaying())
		OSAtomicTestAndSetBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);
}

void AudioPlayer::Stop()
{
	Pause();
	
	StopActiveDecoders();
	
	ResetOutput();

	mFramesDecoded = 0;
	mFramesRendered = 0;
}

CFURLRef AudioPlayer::GetPlayingURL() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return NULL;
	
	return currentDecoderState->mDecoder->GetURL();
}

#pragma mark Playback Properties

SInt64 AudioPlayer::GetCurrentFrame() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
}

SInt64 AudioPlayer::GetTotalFrames() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return currentDecoderState->mTotalFrames;
}

CFTimeInterval AudioPlayer::GetCurrentTime() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return static_cast<CFTimeInterval>(GetCurrentFrame() / currentDecoderState->mDecoder->GetFormat().mSampleRate);
}

CFTimeInterval AudioPlayer::GetTotalTime() const
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
	
	if(!currentDecoderState->mDecoder->SupportsSeeking())
		return false;
	
	if(0 > frame || frame >= currentDecoderState->mTotalFrames)
		return false;

	if(!OSAtomicCompareAndSwap64Barrier(currentDecoderState->mFrameToSeek, frame, &currentDecoderState->mFrameToSeek))
		return false;
	
	kern_return_t error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	}

	return true;	
}

bool AudioPlayer::SupportsSeeking() const
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}

#pragma mark Player Parameters

bool AudioPlayer::GetMasterVolume(Float32& volume) const
{
	return GetVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::SetMasterVolume(Float32 volume)
{
	return SetVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool AudioPlayer::GetChannelCount(UInt32& channelCount) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyStreamConfiguration,
		kAudioDevicePropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectHasProperty (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) is false");
		return false;
	}

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize);

	if(kAudioHardwareNoError != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);

	if(NULL == bufferList) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Unable to allocate << " << dataSize << " bytes");
		return false;
	}
	
	result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize, bufferList);
	
	if(kAudioHardwareNoError != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput) failed: " << result);
		free(bufferList), bufferList = NULL;
		return false;
	}
	
	channelCount = 0;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;

	free(bufferList), bufferList = NULL;
	return true;
}

bool AudioPlayer::GetPreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyPreferredChannelsForStereo, 
		kAudioDevicePropertyScopeOutput,
		kAudioObjectPropertyElementMaster 
	};
	
	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed is false");
		return false;
	}
	
	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize, &preferredChannels);
	
	if(kAudioHardwareNoError != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioDevicePropertyScopeOutput) failed: " << result);
		return false;
	}

	preferredStereoChannels.first = preferredChannels[0];
	preferredStereoChannels.second = preferredChannels[1];

	return true;
}

bool AudioPlayer::GetVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyVolumeScalar, 
		kAudioDevicePropertyScopeOutput,
		channel 
	};
	
	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}
	
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	
	if(kAudioHardwareNoError != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetVolumeForChannel(UInt32 channel, Float32 volume)
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting output device 0x" << std::hex << mOutputDeviceID << " channel " << channel << " volume to " << volume);

	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyVolumeScalar, 
		kAudioDevicePropertyScopeOutput,
		channel 
	};
	
	if(!AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOG4CXX_WARN(logger, "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	OSStatus result = AudioObjectSetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}
	
	return true;
}

void AudioPlayer::EnableDigitalVolume(bool enableDigitalVolume)
{
	if(enableDigitalVolume)
		OSAtomicTestAndSetBarrier(4 /* eAudioPlayerFlagDigitalVolumeEnabled */, &mFlags);
	else
		OSAtomicTestAndClearBarrier(4 /* eAudioPlayerFlagDigitalVolumeEnabled */, &mFlags);
}

bool AudioPlayer::GetDigitalVolume(double& volume) const
{
	volume = mDigitalVolume;
	return true;
}

bool AudioPlayer::SetDigitalVolume(double volume)
{
	mDigitalVolume = std::min(1.0, std::max(0.0, volume));
	return true;
}

void AudioPlayer::EnableDigitalPreGain(bool enableDigitalPreGain)
{
	if(enableDigitalPreGain)
		OSAtomicTestAndSetBarrier(3 /* eAudioPlayerFlagDigitalPreGainEnabled */, &mFlags);
	else
		OSAtomicTestAndClearBarrier(3 /* eAudioPlayerFlagDigitalPreGainEnabled */, &mFlags);
}

bool AudioPlayer::GetDigitalPreGain(double& preGain) const
{
	preGain = mDigitalPreGain;
	return true;
}

bool AudioPlayer::SetDigitalPreGain(double preGain)
{
	mDigitalVolume = std::min(15.0, std::max(-15.0, preGain));
	return true;
}

bool AudioPlayer::SetSampleRateConverterQuality(UInt32 srcQuality)
{
	if(NULL == mSampleRateConverter)
		return false;

	OSStatus result = AudioConverterSetProperty(mSampleRateConverter, 
												kAudioConverterSampleRateConverterQuality, 
												sizeof(srcQuality), 
												&srcQuality);

	if(noErr != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioConverterSetProperty (kAudioConverterSampleRateConverterQuality) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::SetSampleRateConverterComplexity(OSType srcComplexity)
{
	if(NULL == mSampleRateConverter)
		return false;
	
	OSStatus result = AudioConverterSetProperty(mSampleRateConverter, 
												kAudioConverterSampleRateConverterComplexity, 
												sizeof(srcComplexity), 
												&srcComplexity);
	
	if(noErr != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioConverterSetProperty (kAudioConverterSampleRateConverterComplexity) failed: " << result);
		return false;
	}
	
	return true;
}

#pragma mark Device Management

CFStringRef AudioPlayer::CreateOutputDeviceUID() const
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: " << result);
		return NULL;
	}
	
	return deviceUID;
}

bool AudioPlayer::SetOutputDeviceUID(CFStringRef deviceUID)
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting output device UID to " << deviceUID);

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
			LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << result);
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
			LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: " << result);
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

	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting output device ID to 0x" << std::hex << deviceID);
	
	if(deviceID == mOutputDeviceID)
		return true;

	if(!CloseOutput())
		return false;
	
	mOutputDeviceID = deviceID;
	
	if(!OpenOutput())
		return false;
	
	return true;
}

bool AudioPlayer::GetOutputDeviceSampleRate(Float64& deviceSampleRate) const
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetOutputDeviceSampleRate(Float64 deviceSampleRate)
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting device 0x" << std::hex << mOutputDeviceID << " sample rate to " << deviceSampleRate << " Hz");

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
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

bool AudioPlayer::OutputDeviceIsHogged() const
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool AudioPlayer::StartHoggingOutputDevice()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Taking hog mode for device 0x" << std::hex << mOutputDeviceID);

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
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}
	
	// The device is already hogged
	if(hogPID != static_cast<pid_t>(-1)) {
		LOG4CXX_DEBUG(logger, "Device is already hogged by pid: " << hogPID);
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
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(wasPlaying)
		Play();

	return true;
}

bool AudioPlayer::StopHoggingOutputDevice()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Releasing hog mode for device 0x" << std::hex << mOutputDeviceID);

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
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
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
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}
	
	if(wasPlaying)
		Play();
	
	return true;
}

#pragma mark Stream Management

bool AudioPlayer::GetOutputStreams(std::vector<AudioStreamID>& streams) const
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: " << result);
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: " << result);
		return false;
	}

	streams.reserve(streamCount);
	for(UInt32 i = 0; i < streamCount; ++i)
		streams.push_back(audioStreamIDs[i]);

	return true;
}

bool AudioPlayer::GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat) const
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Unknown AudioStreamID: " << std::hex << streamID);
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
		return false;
	}
	
	return true;	
}

bool AudioPlayer::SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat)
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting stream 0x" << std::hex << streamID << " virtual format to: " << virtualFormat);

	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOG4CXX_WARN(logger, "Unknown AudioStreamID: " << std::hex << streamID);
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
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat) const
{
	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Unknown AudioStreamID: " << std::hex << streamID);
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat)
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Setting stream 0x" << std::hex << streamID << " physical format to: " << physicalFormat);

	if(mOutputDeviceStreamIDs.end() == std::find(mOutputDeviceStreamIDs.begin(), mOutputDeviceStreamIDs.end(), streamID)) {
		LOG4CXX_WARN(logger, "Unknown AudioStreamID: " << std::hex << streamID);
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
		LOG4CXX_WARN(logger, "AudioObjectSetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
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
	
	if(!success)
		delete decoder;
	
	return success;
}

bool AudioPlayer::Enqueue(AudioDecoder *decoder)
{
	assert(NULL != decoder);

	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_DEBUG(logger, "Enqueuing \"" << decoder->GetURL() << "\"");
	
	int result = pthread_mutex_lock(&mMutex);
	if(0 != result) {
		LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
		return false;
	}
	
	bool queueEmpty = (0 == CFArrayGetCount(mDecoderQueue));
		
	result = pthread_mutex_unlock(&mMutex);
	if(0 != result)
		LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));
	
	// If there are no decoders in the queue, set up for playback
	if(NULL == GetCurrentDecoderState() && queueEmpty) {
		if(mRingBufferChannelLayout)
			free(mRingBufferChannelLayout), mRingBufferChannelLayout = NULL;

		AudioStreamBasicDescription format = decoder->GetFormat();

		// The ring buffer contains deinterleaved floats at the decoder's sample rate and channel layout
		mRingBufferFormat.mSampleRate			= format.mSampleRate;
		mRingBufferFormat.mChannelsPerFrame		= format.mChannelsPerFrame;
		mRingBufferChannelLayout				= CopyChannelLayout(decoder->GetChannelLayout());

		result = pthread_mutex_lock(&mMutex);
		if(0 != result) {
			LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
			return false;
		}

		if(!CreateConvertersAndSRCBuffer())
			LOG4CXX_WARN(logger, "CreateConvertersAndSRCBuffer failed");

		result = pthread_mutex_unlock(&mMutex);
		if(0 != result)
			LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));

		// Allocate enough space in the ring buffer for the new format
		mRingBuffer->Allocate(mRingBufferFormat.mChannelsPerFrame,
							  mRingBufferFormat.mBytesPerFrame,
							  RING_BUFFER_SIZE_FRAMES);
	}
	// Otherwise, enqueue this decoder if the format matches
	else {
		AudioStreamBasicDescription		nextFormat			= decoder->GetFormat();
		AudioChannelLayout				*nextChannelLayout	= decoder->GetChannelLayout();
		
		bool	sampleRatesMatch		= (nextFormat.mSampleRate == mRingBufferFormat.mSampleRate);
		bool	channelCountsMatch		= (nextFormat.mChannelsPerFrame == mRingBufferFormat.mChannelsPerFrame);
		bool	channelLayoutsMatch		= ChannelLayoutsAreEqual(nextChannelLayout, mRingBufferChannelLayout);

		// The two files can be joined seamlessly only if they have the same sample rates, channel counts and channel layouts
		if(!sampleRatesMatch || !channelCountsMatch || !channelLayoutsMatch) {
			if(!sampleRatesMatch)
				LOG4CXX_WARN(logger, "Enqueue failed: Ring buffer sample rate (" << mRingBufferFormat.mSampleRate << " Hz) and decoder sample rate (" << nextFormat.mSampleRate << " Hz) don't match");
			if(!channelCountsMatch)
				LOG4CXX_WARN(logger, "Enqueue failed: Ring buffer channel count (" << mRingBufferFormat.mChannelsPerFrame << ") and decoder channel count (" << nextFormat.mChannelsPerFrame << ") don't match");
			if(!channelLayoutsMatch)
				LOG4CXX_WARN(logger, "Enqueue failed: Ring buffer channel layout (" << *mRingBufferChannelLayout << ") and decoder channel layout (" << *nextChannelLayout << ") don't match");
			return false;
		}
	}
	
	// Add the decoder to the queue
	result = pthread_mutex_lock(&mMutex);
	if(0 != result) {
		LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
		return false;
	}
	
	CFArrayAppendValue(mDecoderQueue, decoder);
	
	result = pthread_mutex_unlock(&mMutex);
	if(0 != result)
		LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));
	
	kern_return_t error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error)
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	
	return true;
}

bool AudioPlayer::SkipToNextTrack()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();

	if(NULL == currentDecoderState)
		return false;

	OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &currentDecoderState->mFlags);

	// Signal the decoding thread that decoding is finished (inner loop)
	kern_return_t error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	}

	// Wait for decoding to finish or a SIGSEGV could occur if the collector collects an active decoder
	while(!(eDecoderStateDataFlagDecodingFinished & currentDecoderState->mFlags)) {
		int result = usleep(SLEEP_TIME_USEC);
		if(0 != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "Couldn't wait for decoding to finish: " << strerror(errno));
		}
	}

	OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &currentDecoderState->mFlags);

	// Effect a flush of the ring buffer
	mFramesDecoded = 0;
	mFramesRendered = 0;
	
	// Signal the decoding thread to start the next decoder (outer loop)
	error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	}

	OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

	return true;
}

bool AudioPlayer::ClearQueuedDecoders()
{
	int result = pthread_mutex_lock(&mMutex);
	if(0 != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
		return false;
	}

	while(0 < CFArrayGetCount(mDecoderQueue)) {
		AudioDecoder *decoder = static_cast<AudioDecoder *>(const_cast<void *>(CFArrayGetValueAtIndex(mDecoderQueue, 0)));
		CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		delete decoder;
	}

	result = pthread_mutex_unlock(&mMutex);
	if(0 != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));
	}
	
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

	// Stop output if requested
	if(eAudioPlayerFlagStopRequested & mFlags) {
		OSAtomicTestAndClearBarrier(5 /* eAudioPlayerFlagStopRequested */, &mFlags);

		StopOutput();

		return kAudioHardwareNoError;
	}

	// Mute functionality
	if(eAudioPlayerFlagMuteOutput & mFlags)
		return kAudioHardwareNoError;

	// If the ring buffer doesn't contain any valid audio, skip some work
	if(mFramesDecoded == mFramesRendered) {
		// If there are no decoders in the queue, stop IO
		if(NULL == GetCurrentDecoderState())
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
														  NULL);
		
		if(noErr != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_ERROR(logger, "AudioConverterFillComplexBuffer failed: " << result);
			return result;
		}
	}
	// Otherwise fetch the output from the ring buffer
	else {
		UInt32 framesAvailableToRead = static_cast<UInt32>(mFramesDecoded - mFramesRendered);
		framesToRead = std::min(framesAvailableToRead, mOutputDeviceBufferFrameSize);

		CARingBufferError result = mRingBuffer->Fetch(mOutputBuffer, framesToRead, mFramesRendered);
		
		if(kCARingBufferError_OK != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_ERROR(logger, "CARingBuffer::Fetch failed: " << result << ", requested " << framesToRead << " frames from " << mFramesRendered);
			return ioErr;
		}
		
		OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
		
		mFramesRenderedLastPass += framesToRead;
	}

	// Apply digital volume
	if(eAudioPlayerFlagDigitalVolumeEnabled & mFlags) {
		for(UInt32 bufferIndex = 0; bufferIndex < mOutputBuffer->mNumberBuffers; ++bufferIndex) {
			double *buffer = static_cast<double *>(mOutputBuffer->mBuffers[bufferIndex].mData);
			vDSP_vsmulD(buffer, 1, &mDigitalVolume, buffer, 1, framesToRead);
		}
	}

	// Iterate through each stream and render output in the stream's format
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(NULL == mOutputConverters[i])
			continue;

		// Convert to the output device's format
		UInt32 framesConverted = mOutputConverters[i]->Convert(mOutputBuffer, outOutputData, framesToRead);
		
		if(framesConverted != framesToRead) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "Conversion to output format failed; all frames may not be rendered");
		}
	}

	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = static_cast<UInt32>(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));

	if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite) {
		kern_return_t error = semaphore_signal(mDecoderSemaphore);
		if(KERN_SUCCESS != error) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
		}
	}

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
		
		SInt64 decoderFramesRemaining = (-1 == decoderState->mTotalFrames ? mFramesRenderedLastPass : decoderState->mTotalFrames - decoderState->mFramesRendered);
		SInt64 framesFromThisDecoder = std::min(decoderFramesRemaining, static_cast<SInt64>(mFramesRenderedLastPass));
		
		if(0 == decoderState->mFramesRendered && !(eDecoderStateDataFlagRenderingStarted & decoderState->mFlags)) {
			decoderState->mDecoder->PerformRenderingStartedCallback();
			OSAtomicTestAndSetBarrier(5 /* eDecoderStateDataFlagRenderingStarted */, &decoderState->mFlags);
		}
		
		OSAtomicAdd64Barrier(framesFromThisDecoder, &decoderState->mFramesRendered);
		
		if(decoderState->mFramesRendered == decoderState->mTotalFrames/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags)*/) {
			decoderState->mDecoder->PerformRenderingFinishedCallback();			

			OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);

			// Since rendering is finished, signal the collector to clean up this decoder
			kern_return_t error = semaphore_signal(mCollectorSemaphore);
			if(KERN_SUCCESS != error) {
				log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
				LOG4CXX_WARN(logger, "Couldn't signal the collector semaphore: " << mach_error_string(error));
			}
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
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));

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
																 NULL, 
																 &dataSize,
																 &isRunning);
					
					if(kAudioHardwareNoError != result) {
						LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: " << result);
						continue;
					}

					if(isRunning)
						OSAtomicTestAndSetBarrier(7 /* eAudioPlayerFlagIsPlaying */, &mFlags);
					else
						OSAtomicTestAndClearBarrier(7 /* eAudioPlayerFlagIsPlaying */, &mFlags);

					LOG4CXX_DEBUG(logger, "-> kAudioDevicePropertyDeviceIsRunning [0x" << std::hex << inObjectID << "]: " << (isRunning ? "True" : "False"));

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
						LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
						continue;
					}
					
					LOG4CXX_DEBUG(logger, "-> kAudioDevicePropertyNominalSampleRate [0x" << std::hex << inObjectID << "]: " << deviceSampleRate << " Hz");
					
					break;
				}

				case kAudioDevicePropertyStreams:
				{
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					bool restartIO = false;
					if(OutputIsRunning())
						restartIO = StopOutput();

					// Stop observing properties on the defunct streams
					if(!RemoveVirtualFormatPropertyListeners())
						LOG4CXX_WARN(logger, "RemoveVirtualFormatPropertyListeners failed");
					
					for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
						if(NULL != mOutputConverters[i])
							delete mOutputConverters[i], mOutputConverters[i] = NULL;
					}
					
					delete [] mOutputConverters, mOutputConverters = NULL;
					
					mOutputDeviceStreamIDs.clear();
					
					// Update our list of cached streams
					if(!GetOutputStreams(mOutputDeviceStreamIDs)) 
						continue;

					// Observe the new streams for changes
					if(!AddVirtualFormatPropertyListeners())
						LOG4CXX_WARN(logger, "AddVirtualFormatPropertyListeners failed");
					
					mOutputConverters = new PCMConverter * [mOutputDeviceStreamIDs.size()];
					for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
						mOutputConverters[i] = NULL;

					int result = pthread_mutex_lock(&mMutex);
					if(0 != result) {
						LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
						continue;
					}

					if(!CreateConvertersAndSRCBuffer())
						LOG4CXX_WARN(logger, "CreateConvertersAndSRCBuffer failed");

					result = pthread_mutex_unlock(&mMutex);
					if(0 != result)
						LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));

					if(restartIO)
						StartOutput();

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
					
					LOG4CXX_DEBUG(logger, "-> kAudioDevicePropertyStreams [0x" << std::hex << inObjectID << "]");

					break;
				}

				case kAudioDevicePropertyBufferFrameSize:
				{
					OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					bool restartIO = false;
					if(OutputIsRunning())
						restartIO = StopOutput();

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
						LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: " << result);
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
							LOG4CXX_WARN(logger, "AudioConverterGetProperty (kAudioConverterCurrentOutputStreamDescription) failed: " << result);
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
							LOG4CXX_WARN(logger, "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);
							continue;
						}
						
						// Allocate the sample rate conversion buffer (data is at the ring buffer's sample rate)
						mSampleRateConversionBuffer = AllocateABL(mRingBufferFormat, bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);
					}
					
					// Allocate the output buffer (data is at the device's sample rate)
					mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);

					if(restartIO)
						StartOutput();

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

					LOG4CXX_DEBUG(logger, "-> kAudioDevicePropertyBufferFrameSize [0x" << std::hex << inObjectID << "]: " << mOutputDeviceBufferFrameSize);

					break;
				}

				case kAudioDeviceProcessorOverload:
					LOG4CXX_WARN(logger, "-> kAudioDeviceProcessorOverload [0x" << std::hex << inObjectID << "]: Unable to meet IOProc time constraints");
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
						LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
						continue;
					}

					LOG4CXX_DEBUG(logger, "-> kAudioStreamPropertyVirtualFormat [0x" << std::hex << inObjectID << "]: " << virtualFormat);

					result = pthread_mutex_lock(&mMutex);
					if(0 != result) {
						LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(result));
						continue;
					}

					if(!CreateConvertersAndSRCBuffer())
						LOG4CXX_WARN(logger, "CreateConvertersAndSRCBuffer failed");

					result = pthread_mutex_unlock(&mMutex);
					if(0 != result)
						LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(result));

					if(restartIO)
						StartOutput();

					OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);

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
																 NULL, 
																 &dataSize,
																 &physicalFormat);
					
					if(kAudioHardwareNoError != result) {
						LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
						continue;
					}

					LOG4CXX_DEBUG(logger, "-> kAudioStreamPropertyPhysicalFormat [0x" << std::hex << inObjectID << "]: " << physicalFormat);

					break;
				}
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
		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min(framesAvailableToRead, *ioNumberDataPackets);

	CARingBufferError result = mRingBuffer->Fetch(mSampleRateConversionBuffer, framesToRead, mFramesRendered);
	
	if(kCARingBufferError_OK != result) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_ERROR(logger, "CARingBuffer::Fetch failed: " << result << ", requested " << framesToRead << " frames from " << mFramesRendered);
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
	log4cxx::NDC::push("Decoding Thread");
	log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");

	// ========================================
	// Make ourselves a high priority thread
	if(!setThreadPolicy(DECODER_THREAD_IMPORTANCE))
		LOG4CXX_WARN(logger, "Couldn't set decoder thread importance");
	
	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };

	while(mKeepDecoding) {
		AudioDecoder *decoder = NULL;
		
		// ========================================
		// Lock the queue and remove the head element, which contains the next decoder to use
		int lockResult = pthread_mutex_lock(&mMutex);
		if(0 != lockResult) {
			LOG4CXX_ERROR(logger, "pthread_mutex_lock failed: " << strerror(lockResult));
			
			// Stop now, to avoid risking data corruption
			continue;
		}

		if(0 < CFArrayGetCount(mDecoderQueue)) {
			decoder = (AudioDecoder *)CFArrayGetValueAtIndex(mDecoderQueue, 0);
			CFArrayRemoveValueAtIndex(mDecoderQueue, 0);
		}
		
		lockResult = pthread_mutex_unlock(&mMutex);
		if(0 != lockResult)
			LOG4CXX_WARN(logger, "pthread_mutex_unlock failed: " << strerror(lockResult));
		
		// ========================================
		// If a decoder was found at the head of the queue, process it
		if(NULL != decoder) {

			LOG4CXX_DEBUG(logger, "Decoding starting for \"" << decoder->GetURL() << "\"");
			LOG4CXX_DEBUG(logger, "Decoder format: " << decoder->GetFormat());
			
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
					LOG4CXX_WARN(logger, "OSAtomicCompareAndSwapPtrBarrier() failed");
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
			while(decoderState && !(eDecoderStateDataFlagStopDecoding & decoderState->mFlags)) {
				
				// Fill the ring buffer with as much data as possible
				for(;;) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = static_cast<UInt32>(mRingBuffer->GetCapacityFrames() - (mFramesDecoded - mFramesRendered));
					
					// Force writes to the ring buffer to be at least RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES
					if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite) {
						
						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
							LOG4CXX_TRACE(logger, "Seeking to frame " << decoderState->mFrameToSeek);

							OSAtomicTestAndSetBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
							
							SInt64 currentFrameBeforeSeeking = decoder->GetCurrentFrame();
							
							SInt64 newFrame = decoder->SeekToFrame(decoderState->mFrameToSeek);
							
							if(newFrame != decoderState->mFrameToSeek)
								LOG4CXX_ERROR(logger, "Error seeking to frame  " << decoderState->mFrameToSeek);
							
							// Update the seek request
							if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFrameToSeek, -1, &decoderState->mFrameToSeek))
								LOG4CXX_ERROR(logger, "OSAtomicCompareAndSwap64Barrier() failed ");
							
							// If the seek failed do not update the counters
							if(-1 != newFrame) {
								SInt64 framesSkipped = newFrame - currentFrameBeforeSeeking;
								
								// Treat the skipped frames as if they were rendered, and update the counters accordingly
								if(!OSAtomicCompareAndSwap64Barrier(decoderState->mFramesRendered, newFrame, &decoderState->mFramesRendered))
									LOG4CXX_ERROR(logger, "OSAtomicCompareAndSwap64Barrier() failed ");
								
								OSAtomicAdd64Barrier(framesSkipped, &mFramesDecoded);
								if(!OSAtomicCompareAndSwap64Barrier(mFramesRendered, mFramesDecoded, &mFramesRendered))
									LOG4CXX_ERROR(logger, "OSAtomicCompareAndSwap64Barrier() failed ");
								
								// This is safe to call at this point, because eAudioPlayerFlagMuteOutput is set so
								// no rendering is being performed
								ResetOutput();
							}

							OSAtomicTestAndClearBarrier(6 /* eAudioPlayerFlagMuteOutput */, &mFlags);
						}
						
						SInt64 startingFrameNumber = decoder->GetCurrentFrame();

						if(-1 == startingFrameNumber) {
							LOG4CXX_ERROR(logger, "Unable to determine starting frame number ");
							break;
						}

						// If this is the first frame, decoding is just starting
						if(0 == startingFrameNumber && !(eDecoderStateDataFlagDecodingStarted & decoderState->mFlags)) {
							decoder->PerformDecodingStartedCallback();
							OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingStarted */, &decoderState->mFlags);
						}

						// Read the input chunk
						UInt32 framesDecoded = decoderState->ReadAudio(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES);
						
						// Convert and store the decoded audio
						if(0 != framesDecoded) {
							UInt32 framesConverted = converter->Convert(decoderState->mBufferList, bufferList, framesDecoded);
							
							if(framesConverted != framesDecoded)
								LOG4CXX_ERROR(logger, "Incomplete conversion:  " << framesConverted <<  "/" << framesDecoded << " frames");

							// Apply digital pre-gain
							if(eAudioPlayerFlagDigitalPreGainEnabled & mFlags) {
								double linearGain = pow(10.0, mDigitalPreGain / 20.0);
								for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
									double *buffer = static_cast<double *>(bufferList->mBuffers[bufferIndex].mData);
									vDSP_vsmulD(buffer, 1, &linearGain, buffer, 1, framesConverted);
								}
							}

							CARingBufferError result = mRingBuffer->Store(bufferList, 
																		  framesConverted, 
																		  startingFrameNumber + startTime);
							
							if(kCARingBufferError_OK != result)
								LOG4CXX_ERROR(logger, "CARingBuffer::Store failed: " << result);

							OSAtomicAdd64Barrier(framesConverted, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded/* && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)*/) {
							LOG4CXX_DEBUG(logger, "Decoding finished for \"" << decoder->GetURL() << "\"");

							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							// Rather than require preprocessing to ensure an accurate frame count, update 
							// it here so EOS is correctly detected in DidRender()
							decoderState->mTotalFrames = startingFrameNumber;

							decoder->PerformDecodingFinishedCallback();
							
							// Decoding is complete
							OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);

							decoderState = NULL;

							break;
						}
					}
					// Not enough space remains in the ring buffer to write an entire decoded chunk
					else
						break;
				}
				
				// Wait for the audio rendering thread to signal us that it could use more data, or for the timeout to happen
				kern_return_t error = semaphore_timedwait(mDecoderSemaphore, timeout);
				if(KERN_SUCCESS != error && KERN_OPERATION_TIMED_OUT != error)
					LOG4CXX_WARN(logger, "Decoder semaphore couldn't wait: " << mach_error_string(error));
			}
			
			// ========================================
			// Clean up
			// Set the appropriate flags for collection if decoding was stopped early
			if(decoderState) {
				OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
				decoderState = NULL;
			}

			if(NULL != bufferList)
				bufferList = DeallocateABL(bufferList);
			
			if(NULL != converter)
				delete converter, converter = NULL;
		}

		// Wait for another thread to wake us, or for the timeout to happen
		kern_return_t error = semaphore_timedwait(mDecoderSemaphore, timeout);
		if(KERN_SUCCESS != error && KERN_OPERATION_TIMED_OUT != error)
			LOG4CXX_WARN(logger, "Decoder semaphore couldn't wait: " << mach_error_string(error));
	}
	
	LOG4CXX_DEBUG(logger, "Decoding thread terminating");

	log4cxx::NDC::pop();

	return NULL;
}

void * AudioPlayer::CollectorThreadEntry()
{
	log4cxx::NDC::push("Collecting Thread");
	log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer");

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
		kern_return_t error = semaphore_timedwait(mCollectorSemaphore, timeout);
		if(KERN_SUCCESS != error && KERN_OPERATION_TIMED_OUT != error)
			LOG4CXX_WARN(logger, "Collector semaphore couldn't wait: " << mach_error_string(error));
	}
	
	LOG4CXX_DEBUG(logger, "Collecting thread terminating");
	
	log4cxx::NDC::pop();

	return NULL;
}

#pragma mark AudioHardware Utilities

bool AudioPlayer::OpenOutput()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "Opening output for device 0x" << std::hex << mOutputDeviceID);
	
	// Create the IOProc which will feed audio to the device
	OSStatus result = AudioDeviceCreateIOProcID(mOutputDeviceID, 
												myIOProc, 
												this, 
												&mOutputDeviceIOProcID);
	
	if(noErr != result) {
		LOG4CXX_ERROR(logger, "AudioDeviceCreateIOProcID failed: " << result);
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
		LOG4CXX_WARN(logger, "AudioObjectAddPropertyListener (kAudioDeviceProcessorOverload) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioObjectAddPropertyListener (kAudioDevicePropertyBufferFrameSize) failed: " << result);
		return false;
	}
		
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		LOG4CXX_WARN(logger, "AudioObjectAddPropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioObjectAddPropertyListener (kAudioDevicePropertyNominalSampleRate) failed: " << result);
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
		LOG4CXX_DEBUG(logger, "Opening output for device 0x" << std::hex << mOutputDeviceID << " (" << deviceName << ")");
	}
	else
		LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: " << result);

	if(deviceName)
		CFRelease(deviceName), deviceName = NULL;

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	
    result = AudioObjectAddPropertyListener(mOutputDeviceID,
											&propertyAddress,
											myAudioObjectPropertyListenerProc,
											this);
	
	if(kAudioHardwareNoError != result)
		LOG4CXX_WARN(logger, "AudioObjectAddPropertyListener (kAudioDevicePropertyStreams) failed: " << result);

	// Get the device's stream information
	if(!GetOutputStreams(mOutputDeviceStreamIDs))
		return false;

	if(!AddVirtualFormatPropertyListeners())
		return false;

	mOutputConverters = new PCMConverter * [mOutputDeviceStreamIDs.size()];
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i)
		mOutputConverters[i] = NULL;

	return true;
}

bool AudioPlayer::CloseOutput()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "Closing output for device 0x" << std::hex << mOutputDeviceID);

	OSStatus result = AudioDeviceDestroyIOProcID(mOutputDeviceID, 
												 mOutputDeviceIOProcID);

	if(noErr != result) {
		LOG4CXX_ERROR(logger, "AudioDeviceDestroyIOProcID failed: " << result);
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
		LOG4CXX_WARN(logger, "AudioObjectRemovePropertyListener (kAudioDeviceProcessorOverload) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioObjectRemovePropertyListener (kAudioDevicePropertyBufferFrameSize) failed: " << result);
		return false;
	}
	
	propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOG4CXX_WARN(logger, "AudioObjectRemovePropertyListener (kAudioDevicePropertyDeviceIsRunning) failed: " << result);

	propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioObjectRemovePropertyListener (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	
	result = AudioObjectRemovePropertyListener(mOutputDeviceID, 
											   &propertyAddress, 
											   myAudioObjectPropertyListenerProc, 
											   this);
	
	if(kAudioHardwareNoError != result)
		LOG4CXX_WARN(logger, "AudioObjectRemovePropertyListener (kAudioDevicePropertyStreams) failed: " << result);

	if(!RemoveVirtualFormatPropertyListeners())
		LOG4CXX_WARN(logger, "RemoveVirtualFormatPropertyListeners failed");

	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(NULL != mOutputConverters[i])
			delete mOutputConverters[i], mOutputConverters[i] = NULL;
	}
	
	delete [] mOutputConverters, mOutputConverters = NULL;
	
	mOutputDeviceStreamIDs.clear();

	return true;
}

bool AudioPlayer::StartOutput()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "Starting device 0x" << std::hex << mOutputDeviceID);

	OSStatus result = AudioDeviceStart(mOutputDeviceID, 
									   mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioDeviceStart failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::StopOutput()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "Stopping device 0x" << std::hex << mOutputDeviceID);

	OSStatus result = AudioDeviceStop(mOutputDeviceID, 
									  mOutputDeviceIOProcID);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioDeviceStop failed: " << result);
		return false;
	}
	
	return true;
}

bool AudioPlayer::OutputIsRunning() const
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
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_ERROR(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceIsRunning) failed: " << result);
		return false;
	}
	
	return isRunning;
}

// NOT thread safe
bool AudioPlayer::ResetOutput()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "Resetting output");

	if(NULL != mSampleRateConverter) {
		OSStatus result = AudioConverterReset(mSampleRateConverter);
		
		if(noErr != result) {
			LOG4CXX_ERROR(logger, "AudioConverterReset failed: " << result);
			return false;
		}
	}

	return true;
}

#pragma mark Other Utilities

DecoderStateData * AudioPlayer::GetCurrentDecoderState() const
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

DecoderStateData * AudioPlayer::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const
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
	// Request that any decoders still actively decoding stop
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(NULL == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(3 /* eDecoderStateDataFlagStopDecoding */, &decoderState->mFlags);
	}
	
	kern_return_t error = semaphore_signal(mDecoderSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the decoder semaphore: " << mach_error_string(error));
	}

	// Wait for the player to stop or a SIGSEGV could occur if the collector collects a rendering decoder
	while(OutputIsRunning()) {
		int result = usleep(SLEEP_TIME_USEC);
		if(0 != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "Couldn't wait for player to stop: " << strerror(errno));
		}
	}
	
	for(UInt32 bufferIndex = 0; bufferIndex < kActiveDecoderArraySize; ++bufferIndex) {
		DecoderStateData *decoderState = mActiveDecoders[bufferIndex];
		
		if(NULL == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(4 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
	}
	
	error = semaphore_signal(mCollectorSemaphore);
	if(KERN_SUCCESS != error) {
		log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
		LOG4CXX_WARN(logger, "Couldn't signal the collector semaphore: " << mach_error_string(error));
	}
}

bool AudioPlayer::CreateConvertersAndSRCBuffer()
{
	log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
	LOG4CXX_TRACE(logger, "CreateConvertersAndSRCBuffer");

	// Clean up
	for(std::vector<AudioStreamID>::size_type i = 0; i < mOutputDeviceStreamIDs.size(); ++i) {
		if(NULL != mOutputConverters[i])
			delete mOutputConverters[i], mOutputConverters[i] = NULL;
	}
	
	if(NULL != mSampleRateConverter) {
		OSStatus result = AudioConverterDispose(mSampleRateConverter);
		mSampleRateConverter = NULL;
			
		if(noErr != result)
			LOG4CXX_WARN(logger, "AudioConverterDispose failed: " << result);
	}
	
	if(NULL != mSampleRateConversionBuffer)
		mSampleRateConversionBuffer = DeallocateABL(mSampleRateConversionBuffer);
	
	if(NULL != mOutputBuffer)
		mOutputBuffer = DeallocateABL(mOutputBuffer);
	
	// If the ring buffer does not yet have a format, no buffers can be allocated
	if(0 == mRingBufferFormat.mChannelsPerFrame || 0 == mRingBufferFormat.mSampleRate) {
		LOG4CXX_WARN(logger, "Ring buffer has invalid format");
		return false;
	}

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
		LOG4CXX_ERROR(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyBufferFrameSize) failed: " << result);
		return false;
	}

	// FIXME: Handle devices with variable output buffer sizes
	propertyAddress.mSelector = kAudioDevicePropertyUsesVariableBufferFrameSizes;
	if(AudioObjectHasProperty(mOutputDeviceID, &propertyAddress)) {
		LOG4CXX_ERROR(logger, "Devices with variable buffer sizes not supported");
		return false;
	}

	AudioStreamBasicDescription outputBufferFormat = mRingBufferFormat;

	// Create a sample rate converter if required
	Float64 deviceSampleRate;
	if(!GetOutputDeviceSampleRate(deviceSampleRate)) {
		LOG4CXX_ERROR(logger, "Unable to determine output device sample rate");
		return false;
	}
	
	if(deviceSampleRate != mRingBufferFormat.mSampleRate) {
		outputBufferFormat.mSampleRate = deviceSampleRate;
		
		result = AudioConverterNew(&mRingBufferFormat, &outputBufferFormat, &mSampleRateConverter);
		
		if(noErr != result) {
			LOG4CXX_ERROR(logger, "AudioConverterNew failed: " << result);
			return false;
		}
		
		LOG4CXX_DEBUG(logger, "Using sample rate converter for " << mRingBufferFormat.mSampleRate << " Hz to " << deviceSampleRate << " Hz conversion");
		
		// Calculate how large the sample rate conversion buffer must be
		UInt32 bufferSizeBytes = mOutputDeviceBufferFrameSize * outputBufferFormat.mBytesPerFrame;
		dataSize = sizeof(bufferSizeBytes);
		
		result = AudioConverterGetProperty(mSampleRateConverter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &bufferSizeBytes);
		
		if(noErr != result) {
			LOG4CXX_ERROR(logger, "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: " << result);
			return false;
		}
		
		// Allocate the sample rate conversion buffer (data is at the ring buffer's sample rate)
		mSampleRateConversionBuffer = AllocateABL(mRingBufferFormat, bufferSizeBytes / mRingBufferFormat.mBytesPerFrame);
	}

	// Allocate the output buffer (data is at the device's sample rate)
	mOutputBuffer = AllocateABL(outputBufferFormat, mOutputDeviceBufferFrameSize);

	// Determine the channel map to use when mapping channels to the device for output
	UInt32 deviceChannelCount = 0;
	if(!GetChannelCount(deviceChannelCount)) {
		LOG4CXX_ERROR(logger, "Unable to determine the total number of channels");
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
			
			result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize, &preferredStereoChannels);	
			
			if(kAudioHardwareNoError != result)
				LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo) failed: " << result);
		}
		
		LOG4CXX_DEBUG(logger, "Device preferred stereo channels: " << preferredStereoChannels[0] << " " << preferredStereoChannels[1]);

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
			LOG4CXX_WARN(logger, "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);
			
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
			result = AudioObjectGetPropertyDataSize(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize);
			
			if(kAudioHardwareNoError != result)
				LOG4CXX_WARN(logger, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyPreferredChannelLayout) failed: " << result);

			AudioChannelLayout *preferredChannelLayout = static_cast<AudioChannelLayout *>(malloc(dataSize));
			
			result = AudioObjectGetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, &dataSize, preferredChannelLayout);	
			
			if(kAudioHardwareNoError != result)
				LOG4CXX_WARN(logger, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelLayout) failed: " << result);

			LOG4CXX_DEBUG(logger, "Device preferred channel layout: " << *preferredChannelLayout);
			
			const AudioChannelLayout *specifier [2] = { mRingBufferChannelLayout, preferredChannelLayout };

			// Not all channel layouts can be mapped, so handle failure with a generic mapping
			dataSize = sizeof(deviceChannelMap);
			result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(specifier), specifier, &dataSize, deviceChannelMap);
				
			if(noErr != result) {
				LOG4CXX_WARN(logger, "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);

				// Just use a channel map that makes sense
				for(UInt32 i = 0; i < std::min(outputBufferFormat.mChannelsPerFrame, deviceChannelCount); ++i)
					deviceChannelMap[i] = i;
			}
			
			free(preferredChannelLayout), preferredChannelLayout = NULL;		
		}
		else {
			LOG4CXX_WARN(logger, "No preferred multichannel layout");
			
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

		LOG4CXX_DEBUG(logger, "Stream 0x" << std::hex << streamID << " information: ");

		AudioStreamBasicDescription virtualFormat;
		if(!GetOutputStreamVirtualFormat(streamID, virtualFormat)) {
			LOG4CXX_ERROR(logger, "Unknown virtual format for AudioStreamID 0x" << std::hex << streamID);
			return false;
		}

		// In some cases when this function is called from Enqueue() immediately after a device sample rate change, the device's
		// nominal sample rate has changed but the virtual formats have not
		if(deviceSampleRate != virtualFormat.mSampleRate) {
			LOG4CXX_ERROR(logger, "Internal inconsistency: device sample rate (" << deviceSampleRate << " Hz) and virtual format sample rate (" << virtualFormat.mSampleRate << " Hz) don't match");
			return false;			
		}

		LOG4CXX_DEBUG(logger, "  Virtual format: " << virtualFormat);

		// Set up the channel mapping to determine if this stream is needed
		propertyAddress.mSelector = kAudioStreamPropertyStartingChannel;
		propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
		
		UInt32 startingChannel;
		dataSize = sizeof(startingChannel);
		
		result = AudioObjectGetPropertyData(streamID, &propertyAddress, 0, NULL, &dataSize, &startingChannel);	

		if(kAudioHardwareNoError != result) {
			LOG4CXX_ERROR(logger, "AudioObjectGetPropertyData (kAudioStreamPropertyStartingChannel) failed: " << result);
			return false;
		}
		
		LOG4CXX_DEBUG(logger, "  Starting channel: " << startingChannel);
		
		UInt32 endingChannel = startingChannel + virtualFormat.mChannelsPerFrame;

		std::map<int, int> channelMap;
		for(UInt32 channel = startingChannel; channel < endingChannel; ++channel) {
			if(-1 != deviceChannelMap[channel - 1])
				channelMap[channel - 1] = deviceChannelMap[channel - 1];
		}

		// If the channel map isn't empty, the stream is used and an output converter is necessary
		if(!channelMap.empty()) {
			mOutputConverters[i] = new PCMConverter(outputBufferFormat, virtualFormat);			
			mOutputConverters[i]->SetChannelMap(channelMap);

			LOG4CXX_DEBUG(logger, "  Channel map: ");
			for(std::map<int, int>::const_iterator mapIterator = channelMap.begin(); mapIterator != channelMap.end(); ++mapIterator)
				LOG4CXX_DEBUG(logger, "    " << mapIterator->first << " -> " << mapIterator->second);

			streamUsage->mStreamIsOn[i] = true;
		}
	}

	// Disable the unneeded streams
	propertyAddress.mSelector = kAudioDevicePropertyIOProcStreamUsage;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;

	result = AudioObjectSetPropertyData(mOutputDeviceID, &propertyAddress, 0, NULL, static_cast<UInt32>(streamUsageSize), streamUsage);
	
	if(kAudioHardwareNoError != result) {
		LOG4CXX_ERROR(logger, "AudioObjectSetPropertyData (kAudioDevicePropertyIOProcStreamUsage) failed: " << result);
		return false;
	}

	free(streamUsage), streamUsage = NULL;

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
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_ERROR(logger, "AudioObjectAddPropertyListener (kAudioStreamPropertyVirtualFormat) failed: " << result);
			return false;
		}
		
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectAddPropertyListener(*iter,
												&propertyAddress,
												myAudioObjectPropertyListenerProc,
												this);
		
		if(kAudioHardwareNoError != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_ERROR(logger, "AudioObjectAddPropertyListener (kAudioStreamPropertyPhysicalFormat) failed: " << result);
			return false;
		}
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
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "AudioObjectRemovePropertyListener (kAudioStreamPropertyVirtualFormat) failed: " << result);
			continue;
		}
		
		propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		
		result = AudioObjectRemovePropertyListener(*iter,
												   &propertyAddress,
												   myAudioObjectPropertyListenerProc,
												   this);
		
		if(kAudioHardwareNoError != result) {
			log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("org.sbooth.AudioEngine.AudioPlayer"));
			LOG4CXX_WARN(logger, "AudioObjectRemovePropertyListener (kAudioStreamPropertyPhysicalFormat) failed: " << result);
			continue;
		}
	}
	
	return true;
}
