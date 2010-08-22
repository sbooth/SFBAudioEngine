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
#include <Accelerate/Accelerate.h>

#include "AudioEngineDefines.h"
#include "DSPAudioPlayer.h"
#include "AudioDecoder.h"
#include "DecoderStateData.h"

#include "CARingBuffer.h"


// ========================================
// Macros
// ========================================
#define RING_BUFFER_SIZE_FRAMES					16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define DECODER_THREAD_IMPORTANCE				6


// ========================================
// Utility functions
// ========================================
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
	assert(NULL != inRefCon);
	
	DSPAudioPlayer *player = static_cast<DSPAudioPlayer *>(inRefCon);
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
	assert(NULL != inRefCon);
	
	DSPAudioPlayer *player = static_cast<DSPAudioPlayer *>(inRefCon);
	return player->DidRender(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

// ========================================
// The decoder thread's entry point
// ========================================
static void *
decoderEntry(void *arg)
{
	assert(NULL != arg);
	
	DSPAudioPlayer *player = static_cast<DSPAudioPlayer *>(arg);
	return player->DecoderThreadEntry();
}

// ========================================
// The collector thread's entry point
// ========================================
static void *
collectorEntry(void *arg)
{
	assert(NULL != arg);
	
	DSPAudioPlayer *player = static_cast<DSPAudioPlayer *>(arg);
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


DSPAudioPlayer::DSPAudioPlayer()
	: mDecoderQueue(NULL), mRingBuffer(NULL), mFramesDecoded(0), mFramesRendered(0), mPreGain(0), mPerformHardLimiting(false)
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
	for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i)
		mActiveDecoders[i] = NULL;

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
	// The AUGraph will always receive audio in the canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mSampleRate			= 0;
	mFormat.mChannelsPerFrame	= 0;
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
	
	// ========================================
	// Set up our AUGraph and set pregain to 0
	if(false == OpenOutput())
		throw std::runtime_error("OpenOutput failed");
	
	if(false == SetPreGain(0))
		ERR("SetPreGain failed");
}

DSPAudioPlayer::~DSPAudioPlayer()
{
	// Stop the processing graph and reclaim its resources
	CloseOutput();

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
	for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i) {
		if(NULL != mActiveDecoders[i])
			delete mActiveDecoders[i], mActiveDecoders[i] = NULL;
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


void DSPAudioPlayer::Play()
{
	if(IsPlaying())
		return;
	
	StartOutput();
}

void DSPAudioPlayer::Pause()
{
	if(false == IsPlaying())
		return;
	
	StopOutput();
}

void DSPAudioPlayer::Stop()
{
	Pause();
	
	StopActiveDecoders();
	
	ResetOutput();
	
	mFramesDecoded = 0;
	mFramesRendered = 0;
}

bool DSPAudioPlayer::IsPlaying()
{
	return OutputIsRunning();
}

CFURLRef DSPAudioPlayer::GetPlayingURL()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return NULL;
	
	return currentDecoderState->mDecoder->GetURL();
}


#pragma mark Playback Properties


SInt64 DSPAudioPlayer::GetCurrentFrame()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return (-1 == currentDecoderState->mFrameToSeek ? currentDecoderState->mFramesRendered : currentDecoderState->mFrameToSeek);
}

SInt64 DSPAudioPlayer::GetTotalFrames()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return currentDecoderState->mTotalFrames;
}

CFTimeInterval DSPAudioPlayer::GetCurrentTime()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return static_cast<CFTimeInterval>(GetCurrentFrame() / currentDecoderState->mDecoder->GetFormat().mSampleRate);
}

CFTimeInterval DSPAudioPlayer::GetTotalTime()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return -1;
	
	return static_cast<CFTimeInterval>(currentDecoderState->mTotalFrames / currentDecoderState->mDecoder->GetFormat().mSampleRate);
}


#pragma mark Seeking


bool DSPAudioPlayer::SeekForward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 desiredFrame		= GetCurrentFrame() + frameCount;
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::min(desiredFrame, totalFrames - 1));
}

bool DSPAudioPlayer::SeekBackward(CFTimeInterval secondsToSkip)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;

	SInt64 frameCount		= static_cast<SInt64>(secondsToSkip * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 currentFrame		= GetCurrentFrame();
	SInt64 desiredFrame		= currentFrame - frameCount;
	
	return SeekToFrame(std::max(0LL, desiredFrame));
}

bool DSPAudioPlayer::SeekToTime(CFTimeInterval timeInSeconds)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	SInt64 desiredFrame		= static_cast<SInt64>(timeInSeconds * currentDecoderState->mDecoder->GetFormat().mSampleRate);	
	SInt64 totalFrames		= currentDecoderState->mTotalFrames;
	
	return SeekToFrame(std::max(0LL, std::min(desiredFrame, totalFrames - 1)));
}

bool DSPAudioPlayer::SeekToFrame(SInt64 frame)
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	if(false == currentDecoderState->mDecoder->SupportsSeeking())
		return false;

	if(0 > frame || frame >= currentDecoderState->mTotalFrames)
		return false;
	
//	Float64 graphLatency = GetAUGraphLatency();
//	if(-1 != graphLatency) {
//		SInt64 graphLatencyFrames = static_cast<SInt64>(graphLatency * mAUGraphFormat.mSampleRate);
//		frame -= graphLatencyFrames;
//	}
	
	if(false == OSAtomicCompareAndSwap64Barrier(currentDecoderState->mFrameToSeek, frame, &currentDecoderState->mFrameToSeek))
		return false;
	
	semaphore_signal(mDecoderSemaphore);

	return true;	
}

bool DSPAudioPlayer::SupportsSeeking()
{
	DecoderStateData *currentDecoderState = GetCurrentDecoderState();
	
	if(NULL == currentDecoderState)
		return false;
	
	return currentDecoderState->mDecoder->SupportsSeeking();
}


#pragma mark Player Parameters


bool DSPAudioPlayer::GetVolume(Float32& volume)
{
	AudioUnit au = NULL;
	OSStatus auResult = AUGraphNodeInfo(mAUGraph, 
										mOutputNode, 
										NULL, 
										&au);

	if(noErr != auResult) {
		ERR("AUGraphNodeInfo failed: %i", auResult);
		return false;
	}
	
	AudioUnitParameterValue auVolume;
	ComponentResult result = AudioUnitGetParameter(au,
												   kHALOutputParam_Volume,
												   kAudioUnitScope_Global,
												   0,
												   &auVolume);
	
	if(noErr != result) {
		ERR("AudioUnitGetParameter (kHALOutputParam_Volume) failed: %i", result);
		return false;
	}

	volume = static_cast<Float32>(auVolume);
	
	return true;
}

bool DSPAudioPlayer::SetVolume(Float32 volume)
{
	assert(0 <= volume);
	assert(volume <= 1);
	
	AudioUnit au = NULL;
	OSStatus auResult = AUGraphNodeInfo(mAUGraph, 
										mOutputNode, 
										NULL, 
										&au);
	
	if(noErr != auResult) {
		ERR("AUGraphNodeInfo failed: %i", auResult);
		return -1;
	}
	
	ComponentResult result = AudioUnitSetParameter(au,
												   kHALOutputParam_Volume,
												   kAudioUnitScope_Global,
												   0,
												   volume,
												   0);

	if(noErr != result) {
		ERR("AudioUnitSetParameter (kHALOutputParam_Volume) failed: %i", result);
		return false;
	}
	
	return true;
}

bool DSPAudioPlayer::GetPreGain(Float32& preGain)
{
	preGain = mPreGain;
	
	return true;
}

bool DSPAudioPlayer::SetPreGain(Float32 preGain)
{
	assert(-40.f <= preGain);
	assert(40.f >= preGain);
	
	mPreGain = preGain;	
	return true;
	
	//	return OSAtomicCompareAndSwap32Barrier(*reinterpret_cast<int32_t *>(&mPreGain), *reinterpret_cast<int32_t *>(&preGain), reinterpret_cast<int32_t *>(&mPreGain));
}


#pragma mark DSP Effects


bool DSPAudioPlayer::AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit1)
{
	// Get the source node for the graph's output node
	UInt32 numInteractions = 0;
	OSStatus result = AUGraphCountNodeInteractions(mAUGraph, 
												   mOutputNode, 
												   &numInteractions);
	if(noErr != result) {
		ERR("AUGraphCountNodeConnections failed: %i", result);
		return false;
	}
	
	AUNodeInteraction interactions [numInteractions];
	
	result = AUGraphGetNodeInteractions(mAUGraph, 
										mOutputNode,
										&numInteractions, 
										interactions);
	
	if(noErr != result) {
		ERR("AUGraphGetNodeInteractions failed: %i", result);
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
		ERR("Unable to determine input node");
		return false;
	}
	
	// Create the effect node and set its format
	ComponentDescription desc = { kAudioUnitType_Effect, subType, manufacturer, flags, mask };
	
	AUNode effectNode = -1;
	result = AUGraphAddNode(mAUGraph, 
							&desc, 
							&effectNode);
	
	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);
		return false;
	}
	
	AudioUnit effectUnit = NULL;
	result = AUGraphNodeInfo(mAUGraph, 
							 effectNode, 
							 NULL, 
							 &effectUnit);

	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);
		
		result = AUGraphRemoveNode(mAUGraph, effectNode);
		
		if(noErr != result)
			ERR("AUGraphRemoveNode failed: %i", result);

		return false;
	}
	
	result = AudioUnitSetProperty(effectUnit,
								  kAudioUnitProperty_StreamFormat, 
								  kAudioUnitScope_Input, 
								  0,
								  &mFormat,
								  sizeof(mFormat));

	if(noErr != result) {
		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);

		// If the property couldn't be set (the AU may not support this format), remove the new node
		result = AUGraphRemoveNode(mAUGraph, effectNode);

		if(noErr != result)
			ERR("AUGraphRemoveNode failed: %i", result);
				
		return false;
	}
	
	result = AudioUnitSetProperty(effectUnit,
								  kAudioUnitProperty_StreamFormat, 
								  kAudioUnitScope_Output, 
								  0,
								  &mFormat,
								  sizeof(mFormat));
	
	if(noErr != result) {
		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);
		
		// If the property couldn't be set (the AU may not support this format), remove the new node
		result = AUGraphRemoveNode(mAUGraph, effectNode);
		
		if(noErr != result)
			ERR("AUGraphRemoveNode failed: %i", result);
		
		return false;
	}

	// Insert the effect at the end of the graph, before the output node
	result = AUGraphDisconnectNodeInput(mAUGraph, 
										mOutputNode,
										0);

	if(noErr != result) {
		ERR("AUGraphDisconnectNodeInput failed: %i", result);
		
		result = AUGraphRemoveNode(mAUGraph, effectNode);
		
		if(noErr != result)
			ERR("AUGraphRemoveNode failed: %i", result);
		
		return false;
	}
	
	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, 
									 sourceNode,
									 0,
									 effectNode,
									 0);
	if(noErr != result) {
		ERR("AUGraphConnectNodeInput failed: %i", result);
		return false;
	}
	
	result = AUGraphConnectNodeInput(mAUGraph, 
									 effectNode,
									 0,
									 mOutputNode,
									 0);
	if(noErr != result) {
		ERR("AUGraphConnectNodeInput failed: %i", result);
		return false;
	}
	
	result = AUGraphUpdate(mAUGraph, NULL);
	if(noErr != result) {
		ERR("AUGraphUpdate failed: %i", result);

		// If the update failed, restore the previous node state
		result = AUGraphConnectNodeInput(mAUGraph,
										 sourceNode,
										 0,
										 mOutputNode,
										 0);

		if(noErr != result) {
			ERR("AUGraphConnectNodeInput failed: %i", result);
			return false;
		}
	}
	
	if(NULL != effectUnit1)
		*effectUnit1 = effectUnit;
	
	return true;
}

bool DSPAudioPlayer::RemoveEffect(AudioUnit effectUnit)
{
	assert(NULL != effectUnit);
	
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	
	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);
		return false;
	}
	
	AUNode effectNode = -1;
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = -1;
		result = AUGraphGetIndNode(mAUGraph, 
								   nodeIndex, 
								   &node);
		
		if(noErr != result) {
			ERR("AUGraphGetIndNode failed: %i", result);
			return false;
		}
		
		AudioUnit au = NULL;
		result = AUGraphNodeInfo(mAUGraph, 
								 node, 
								 NULL, 
								 &au);
		
		if(noErr != result) {
			ERR("AUGraphNodeInfo failed: %i", result);
			return false;
		}
		
		// This is the unit to remove
		if(effectUnit == au) {
			effectNode = node;
			break;
		}
	}
	
	if(-1 == effectNode) {
		ERR("Unable to find the AUNode for the specified AudioUnit");
		return false;
	}
	
	// Get the current input and output nodes for the node to delete
	UInt32 numInteractions = 0;
	result = AUGraphCountNodeInteractions(mAUGraph, 
										  effectNode, 
										  &numInteractions);
	if(noErr != result) {
		ERR("AUGraphCountNodeConnections failed: %i", result);
		return false;
	}
	
	AUNodeInteraction *interactions = static_cast<AUNodeInteraction *>(calloc(numInteractions, sizeof(AUNodeInteraction)));
	if(NULL == interactions) {
		ERR("Unable to allocate memory");
		return false;
	}

	result = AUGraphGetNodeInteractions(mAUGraph, 
										effectNode,
										&numInteractions, 
										interactions);
	
	if(noErr != result) {
		ERR("AUGraphGetNodeInteractions failed: %i", result);
		
		free(interactions), interactions = NULL;
		
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
	
	free(interactions), interactions = NULL;
	
	if(-1 == sourceNode || -1 == destNode) {
		ERR("Unable to find the source or destination nodes");
		return false;
	}
	
	result = AUGraphDisconnectNodeInput(mAUGraph, effectNode, 0);
	if(noErr != result) {
		ERR("AUGraphDisconnectNodeInput failed: %i", result);
		return false;
	}
	
	result = AUGraphDisconnectNodeInput(mAUGraph, destNode, 0);
	if(noErr != result) {
		ERR("AUGraphDisconnectNodeInput failed: %i", result);
		return false;
	}
	
	result = AUGraphRemoveNode(mAUGraph, effectNode);
	if(noErr != result) {
		ERR("AUGraphRemoveNode failed: %i", result);
		return false;
	}
	
	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, destNode, 0);
	if(noErr != result) {
		ERR("AUGraphConnectNodeInput failed: %i", result);
		return false;
	}
	
	result = AUGraphUpdate(mAUGraph, NULL);
	if(noErr != result) {
		ERR("AUGraphUpdate failed: %i", result);
		return false;
	}
	
	return true;
}


#pragma mark Device Management


CFStringRef DSPAudioPlayer::CreateOutputDeviceUID()
{
	AudioDeviceID deviceID = GetOutputDeviceID();

	if(kAudioDeviceUnknown == deviceID)
		return NULL;

	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyDeviceUID, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	CFStringRef deviceUID = NULL;
	UInt32 dataSize = sizeof(deviceUID);
	
	OSStatus result = AudioObjectGetPropertyData(deviceID,
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

bool DSPAudioPlayer::SetOutputDeviceUID(CFStringRef deviceUID)
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

AudioDeviceID DSPAudioPlayer::GetOutputDeviceID()
{
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mOutputNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return kAudioDeviceUnknown;
	}
	
	AudioDeviceID deviceID = kAudioDeviceUnknown;
	UInt32 dataSize = sizeof(deviceID);
	
	result = AudioUnitGetProperty(au,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &deviceID,
								  &dataSize);
	
	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: %i", result);
		return kAudioDeviceUnknown;
	}
	
	return deviceID;
}

bool DSPAudioPlayer::SetOutputDeviceID(AudioDeviceID deviceID)
{
	assert(kAudioDeviceUnknown != deviceID);

	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mOutputNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return false;
	}
	
	// Update our output AU to use the specified device
	result = AudioUnitSetProperty(au,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &deviceID,
								  sizeof(deviceID));
	
	if(noErr != result) {
		ERR("AudioUnitSetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: %i", result);
		return false;
	}
	
	return true;
}

bool DSPAudioPlayer::GetOutputDeviceSampleRate(Float64& sampleRate)
{
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mOutputNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return false;
	}
	
	AudioDeviceID deviceID = 0;
	UInt32 dataSize = sizeof(deviceID);
	
	result = AudioUnitGetProperty(au,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &deviceID,
								  &dataSize);
	
	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: %i", result);
		return false;
	}
	
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	dataSize = sizeof(sampleRate);
	
	result = AudioObjectGetPropertyData(deviceID,
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

bool DSPAudioPlayer::SetOutputDeviceSampleRate(Float64 sampleRate)
{
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mOutputNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return false;
	}
	
	AudioDeviceID deviceID = 0;
	UInt32 dataSize = sizeof(deviceID);
	
	result = AudioUnitGetProperty(au,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &deviceID,
								  &dataSize);

	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: %i", result);
		return false;
	}
	
	// Determine if this will actually be a change
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioDevicePropertyNominalSampleRate, 
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster 
	};
	
	Float64 currentSampleRate;
	dataSize = sizeof(currentSampleRate);
	
	result = AudioObjectGetPropertyData(deviceID,
										&propertyAddress,
										0,
										NULL,
										&dataSize,
										&currentSampleRate);
	
	if(kAudioHardwareNoError != result) {
		ERR("AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %i", result);
		return false;
	}
	
	// Nothing to do
	if(currentSampleRate == sampleRate)
		return true;
	
	// Set the sample rate
	dataSize = sizeof(sampleRate);
	
	result = AudioObjectSetPropertyData(deviceID,
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


#pragma mark Playlist Management


bool DSPAudioPlayer::Enqueue(CFURLRef url)
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

bool DSPAudioPlayer::Enqueue(AudioDecoder *decoder)
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
		
		OSStatus result = SetAUGraphSampleRateAndChannelsPerFrame(format.mSampleRate, format.mChannelsPerFrame);
		
		if(noErr != result) {
			ERR("SetAUGraphFormat failed: %i", result);
			return false;
		}
		
		result = SetAUGraphChannelLayout(decoder->GetChannelLayout());
		
		// Not all decoders have channel layouts
		if(noErr != result) {
			ERR("SetAUGraphChannelLayout failed: %i", result);
			//return false;
		}
		
		// Allocate enough space in the ring buffer for the new format
		mRingBuffer->Allocate(mFormat.mChannelsPerFrame,
							  mFormat.mBytesPerFrame,
							  RING_BUFFER_SIZE_FRAMES);
	}
	// Otherwise, enqueue this decoder if the format matches
	else {
		AudioStreamBasicDescription		nextFormat			= decoder->GetFormat();
	//	AudioChannelLayout				nextChannelLayout	= decoder->GetChannelLayout();
		
		bool	formatsMatch			= (nextFormat.mSampleRate == mFormat.mSampleRate && nextFormat.mChannelsPerFrame == mFormat.mChannelsPerFrame);
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

bool DSPAudioPlayer::ClearQueuedDecoders()
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


#pragma mark Callbacks


OSStatus DSPAudioPlayer::Render(AudioUnitRenderActionFlags		*ioActionFlags,
								const AudioTimeStamp			*inTimeStamp,
								UInt32							inBusNumber,
								UInt32							inNumberFrames,
								AudioBufferList					*ioData)
{

#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)
	
	assert(NULL != ioActionFlags);
	assert(NULL != ioData);

	// If the ring buffer doesn't contain any valid audio, skip some work
	UInt32 framesAvailableToRead = static_cast<UInt32>(mFramesDecoded - mFramesRendered);
	if(0 == framesAvailableToRead) {
		*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		
		size_t byteCountToZero = inNumberFrames * sizeof(float);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			memset(ioData->mBuffers[bufferIndex].mData, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize = static_cast<UInt32>(byteCountToZero);
		}
		
		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = std::min(framesAvailableToRead, inNumberFrames);
	CARingBufferError result = mRingBuffer->Fetch(ioData, framesToRead, mFramesRendered, false);
	if(kCARingBufferError_OK != result) {
		ERR("CARingBuffer::Fetch() failed: %d, requested %d frames from %lld", result, framesToRead, mFramesRendered);
		return ioErr;
	}

	mFramesRenderedLastPass = framesToRead;
	OSAtomicAdd64Barrier(framesToRead, &mFramesRendered);
	
	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesToRead != inNumberFrames) {
		LOG("Ring buffer contained insufficient data: %d / %d", framesToRead, inNumberFrames);
		
		UInt32 framesOfSilence = inNumberFrames - framesToRead;
		size_t byteCountToZero = framesOfSilence * sizeof(float);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			float *bufferAlias = static_cast<float *>(ioData->mBuffers[bufferIndex].mData);
			memset(bufferAlias + framesToRead, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize += static_cast<UInt32>(byteCountToZero);
		}
	}
	
	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = static_cast<UInt32>(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));
	if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite)
		semaphore_signal(mDecoderSemaphore);
	
	return noErr;
}

OSStatus DSPAudioPlayer::DidRender(AudioUnitRenderActionFlags		*ioActionFlags,
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

		// There is nothing to do if no frames were rendered
		if(0 == mFramesRenderedLastPass) {
			// If there are no more active decoders, stop playback
			if(NULL == GetCurrentDecoderState())
				Stop();

			return noErr;
		}
		
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
				OSMemoryBarrier();

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
	}
	
	return noErr;
}

void * DSPAudioPlayer::DecoderThreadEntry()
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
			// Create the AudioConverter which will convert from the decoder's format to the graph's format
			AudioConverterRef audioConverter = NULL;
			OSStatus result = AudioConverterNew(&decoderFormat, &mFormat, &audioConverter);

			if(noErr != result) {
				ERR("AudioConverterNew failed: %i", result);

				// If this happens, output will be impossible
				OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
			}

			// ========================================
			// Allocate the buffer lists which will serve as the transport between the decoder and the ring buffer
			UInt32 inputBufferSize = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES * mFormat.mBytesPerFrame;
			UInt32 dataSize = sizeof(inputBufferSize);
			result = AudioConverterGetProperty(audioConverter, 
											   kAudioConverterPropertyCalculateInputBufferSize, 
											   &dataSize, 
											   &inputBufferSize);
			
			if(noErr != result)
				ERR("AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %i", result);
			
			decoderState->AllocateBufferList(inputBufferSize / decoderFormat.mBytesPerFrame);

			// The AUGraph expects the canonical Core Audio format
			AudioBufferList *bufferList = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * mFormat.mChannelsPerFrame)));
			
			bufferList->mNumberBuffers = mFormat.mChannelsPerFrame;
			
			for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
				bufferList->mBuffers[bufferIndex].mData = static_cast<void *>(calloc(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES, sizeof(float)));
				bufferList->mBuffers[bufferIndex].mDataByteSize = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES * sizeof(float);
				bufferList->mBuffers[bufferIndex].mNumberChannels = 1;
			}
			
			// ========================================
			// Decode the audio file in the ring buffer until finished or cancelled
			while(decoderState && !(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags)) {

				// Fill the ring buffer with as much data as possible
				while(decoderState) {
					// Determine how many frames are available in the ring buffer
					UInt32 framesAvailableToWrite = static_cast<UInt32>(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));

					// Force writes to the ring buffer to be at least RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES
					if(framesAvailableToWrite >= RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES) {

						// Seek to the specified frame
						if(-1 != decoderState->mFrameToSeek) {
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

								ResetOutput();
							}
						}
						
						SInt64 startingFrameNumber = decoder->GetCurrentFrame();
						
						// Read the input chunk, converting from the decoder's format to the AUGraph's format
						UInt32 framesDecoded = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES;

						result = AudioConverterFillComplexBuffer(audioConverter, 
																 myAudioConverterComplexInputDataProc,
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

							// Apply pregain
							if(0.f != mPreGain) {
								// Convert pregain in dB to a linear gain value
								float linearGain = powf(10, mPreGain / 20);
								
								// Apply pre-gain to the decoded samples
								for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
									float *buffer = static_cast<float *>(bufferList->mBuffers[bufferIndex].mData);
									vDSP_vsmul(buffer, 1, &linearGain, buffer, 1, framesDecoded);
								}
							}
							
							// Clip the samples to [-1, +1), if desired
							if(mPerformHardLimiting) {
								UInt32 bitsPerChannel = decoder->GetSourceFormat().mBitsPerChannel;
								
								// For compressed formats, pretend the samples are 24-bit
								if(0 == bitsPerChannel)
									bitsPerChannel = 24;
								
								// The maximum allowable sample value
								float minValue = -1.f;
								float maxValue = 1.f - (1.f / (1 << (bitsPerChannel - 1)));
								
								for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
									float *buffer = static_cast<float *>(bufferList->mBuffers[bufferIndex].mData);
									vDSP_vclip(buffer, 1, &minValue, &maxValue, buffer, 1, framesDecoded);
								}
							}
							
							// Copy the decoded audio to the ring buffer
							result = mRingBuffer->Store(bufferList, framesDecoded, startingFrameNumber + startTime);

							if(kCARingBufferError_OK != result)
								ERR("CARingBuffer::Store() failed: %i", result);
							
							OSAtomicAdd64Barrier(framesDecoded, &mFramesDecoded);
						}
						
						// If no frames were returned, this is the end of stream
						if(0 == framesDecoded) {
							OSMemoryBarrier();

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

void * DSPAudioPlayer::CollectorThreadEntry()
{
	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };

	while(mKeepCollecting) {
		
		for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i) {
			DecoderStateData *decoderState = mActiveDecoders[i];
			
			if(NULL == decoderState)
				continue;
			
			if(!(eDecoderStateDataFlagDecodingFinished & decoderState->mFlags) || !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags))
				continue;
			
			bool swapSucceeded = OSAtomicCompareAndSwapPtrBarrier(decoderState, NULL, reinterpret_cast<void **>(&mActiveDecoders[i]));
			
			if(swapSucceeded)
				delete decoderState, decoderState = NULL;
		}
		
		// Wait for any thread to signal us to try and collect finished decoders
		semaphore_timedwait(mCollectorSemaphore, timeout);
	}
	
	return NULL;
}


#pragma mark Audio Output Utilities


bool DSPAudioPlayer::OpenOutput()
{
	OSStatus result = NewAUGraph(&mAUGraph);
	
	if(noErr != result) {
		ERR("NewAUGraph failed: %i", result);
		return false;
	}
	
	// The graph will look like:
	// MultiChannelMixer -> Effects (if any) -> Output
	ComponentDescription desc;
	
	// Set up the mixer node
	desc.componentType			= kAudioUnitType_Mixer;
	desc.componentSubType		= kAudioUnitSubType_MultiChannelMixer;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	
	AUNode mixerNode;
	result = AUGraphAddNode(mAUGraph, &desc, &mixerNode);
	
	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);
		return false;
	}
	
	// Set up the output node
	desc.componentType			= kAudioUnitType_Output;
	desc.componentSubType		= kAudioUnitSubType_HALOutput;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	
	result = AUGraphAddNode(mAUGraph, &desc, &mOutputNode);
	
	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);
		return false;
	}
	
	result = AUGraphConnectNodeInput(mAUGraph, mixerNode, 0, mOutputNode, 0);
	
	if(noErr != result) {
		ERR("AUGraphConnectNodeInput failed: %i", result);
		return false;
	}
	
	// Install the input callback
	AURenderCallbackStruct cbs = { myAURenderCallback, this };
	result = AUGraphSetNodeInputCallback(mAUGraph, mixerNode, 0, &cbs);
	
	if(noErr != result) {
		ERR("AUGraphSetNodeInputCallback failed: %i", result);
		return false;
	}
	
	// Open the graph
	result = AUGraphOpen(mAUGraph);
	
	if(noErr != result) {
		ERR("AUGraphOpen failed: %i", result);
		return false;
	}
	
	// Initialize the graph
	result = AUGraphInitialize(mAUGraph);
	
	if(noErr != result) {
		ERR("AUGraphInitialize failed: %i", result);
		return false;
	}
	
	// Set the mixer's volume on the input and output
	AudioUnit au = NULL;
	result = AUGraphNodeInfo(mAUGraph, 
							 mixerNode, 
							 NULL, 
							 &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return false;
	}
	
	result = AudioUnitSetParameter(au,
								   kMultiChannelMixerParam_Volume,
								   kAudioUnitScope_Input,
								   0,
								   1.f,
								   0);
	
	if(noErr != result)
		ERR("AudioUnitSetParameter (kMultiChannelMixerParam_Volume) failed: %i", result);
	
	result = AudioUnitSetParameter(au,
								   kMultiChannelMixerParam_Volume,
								   kAudioUnitScope_Output,
								   0,
								   1.f,
								   0);
	
	if(noErr != result)
		ERR("AudioUnitSetParameter (kMultiChannelMixerParam_Volume) failed: %i", result);
	
	// Install the render notification
	result = AUGraphAddRenderNotify(mAUGraph, auGraphDidRender, this);
	
	if(noErr != result) {
		ERR("AUGraphAddRenderNotify failed: %i", result);
		return false;
	}
	
	return true;
}

bool DSPAudioPlayer::CloseOutput()
{
	Boolean graphIsRunning = FALSE;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	
	if(noErr != result) {
		ERR("AUGraphIsRunning failed: %i", result);
		return false;
	}
	
	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		
		if(noErr != result) {
			ERR("AUGraphStop failed: %i", result);
			return false;
		}
	}
	
	Boolean graphIsInitialized = FALSE;	
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	
	if(noErr != result) {
		ERR("AUGraphIsInitialized failed: %i", result);
		return false;
	}
	
	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);
		
		if(noErr != result) {
			ERR("AUGraphUninitialize failed: %i", result);
			return false;
		}
	}
	
	result = AUGraphClose(mAUGraph);
	
	if(noErr != result) {
		ERR("AUGraphClose failed: %i", result);
		return false;
	}
	
	result = DisposeAUGraph(mAUGraph);
	
	if(noErr != result) {
		ERR("DisposeAUGraph failed: %i", result);
		return false;
	}
	
	mAUGraph = NULL;
	
	return true;
}

bool DSPAudioPlayer::StartOutput()
{
	OSStatus result = AUGraphStart(mAUGraph);
	
	if(noErr != result) {
		ERR("AUGraphStart failed: %i", result);
		return false;
	}
	
	return true;
}

bool DSPAudioPlayer::StopOutput()
{
	OSStatus result = AUGraphStop(mAUGraph);
	
	if(noErr != result) {
		ERR("AUGraphStop failed: %i", result);
		return false;
	}
	
	return true;
}

bool DSPAudioPlayer::OutputIsRunning()
{
	Boolean isRunning = FALSE;
	
	OSStatus result = AUGraphIsRunning(mAUGraph, &isRunning);
	
	if(noErr != result) {
		ERR("AUGraphIsRunning failed: %i", result);
		return false;
	}
	
	return isRunning;
}

bool DSPAudioPlayer::ResetOutput()
{
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	
	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);
		return false;
	}
	
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		
		if(noErr != result) {
			ERR("AUGraphGetIndNode failed: %i", result);
			return false;
		}
		
		AudioUnit au = NULL;
		result = AUGraphNodeInfo(mAUGraph, node, NULL, &au);
		
		if(noErr != result) {
			ERR("AUGraphNodeInfo failed: %i", result);
			return false;
		}
		
		result = AudioUnitReset(au, kAudioUnitScope_Global, 0);
		
		if(noErr != result) {
			ERR("AudioUnitReset failed: %i", result);
			return false;
		}
	}
	
	return true;
}


#pragma mark AUGraph Utilities


Float64 DSPAudioPlayer::GetAUGraphLatency()
{
	Float64 graphLatency = 0;
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);

	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);
		return -1;
	}
	
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);

		if(noErr != result) {
			ERR("AUGraphGetIndNode failed: %i", result);
			return -1;
		}
		
		AudioUnit au = NULL;
		result = AUGraphNodeInfo(mAUGraph, node, NULL, &au);

		if(noErr != result) {
			ERR("AUGraphNodeInfo failed: %i", result);
			return -1;
		}
		
		Float64 latency = 0;
		UInt32 dataSize = sizeof(latency);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &dataSize);

		if(noErr != result) {
			ERR("AudioUnitGetProperty failed: %i", result);
			return -1;
		}
		
		graphLatency += latency;
	}
	
	return graphLatency;
}

Float64 DSPAudioPlayer::GetAUGraphTailTime()
{
	Float64 graphTailTime = 0;
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	
	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);
		return -1;
	}
	
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		
		if(noErr != result) {
			ERR("AUGraphGetIndNode failed: %i", result);
			return -1;
		}
		
		AudioUnit au = NULL;
		result = AUGraphNodeInfo(mAUGraph, node, NULL, &au);
		
		if(noErr != result) {
			ERR("AUGraphNodeInfo failed: %i", result);
			return -1;
		}
		
		Float64 tailTime = 0;
		UInt32 dataSize = sizeof(tailTime);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &tailTime, &dataSize);
		
		if(noErr != result) {
			ERR("AudioUnitGetProperty (kAudioUnitProperty_TailTime) failed: %i", result);
			return -1;
		}
		
		graphTailTime += tailTime;
	}
	
	return graphTailTime;
}

OSStatus DSPAudioPlayer::SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize)
{
	assert(NULL != propertyData);
	assert(0 < propertyDataSize);
	
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);

	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);
		return result;
	}
	
	// Iterate through the nodes and attempt to set the property
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node;
		result = AUGraphGetIndNode(mAUGraph, i, &node);

		if(noErr != result) {
			ERR("AUGraphGetIndNode failed: %i", result);
			return result;
		}
		
		AudioUnit au = NULL;
		result = AUGraphNodeInfo(mAUGraph, node, NULL, &au);

		if(noErr != result) {
			ERR("AUGraphNodeInfo failed: %i", result);
			return result;
		}
		
		if(mOutputNode == node) {
			// For AUHAL as the output node, you can't set the device side, so just set the client side
			result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, 0, propertyData, propertyDataSize);

			if(noErr != result) {
				ERR("AudioUnitSetProperty ('%.4s') failed: %i", reinterpret_cast<const char *>(&propertyID), result);
				return result;
			}
		}
		else {
			UInt32 elementCount = 0;
			UInt32 dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &elementCount, &dataSize);

			if(noErr != result) {
				ERR("AudioUnitGetProperty (kAudioUnitProperty_ElementCount) failed: %i", result);
				return result;
			}
			
			for(UInt32 j = 0; j < elementCount; ++j) {
/*				Boolean writable;
				err = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Input, j, &dataSize, &writable);

				if(noErr != err && kAudioUnitErr_InvalidProperty != err)
					return err;
				 
				if(kAudioUnitErr_InvalidProperty == err || !writable)
					continue;*/
				
				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, j, propertyData, propertyDataSize);

				if(noErr != result) {
					ERR("AudioUnitSetProperty ('%.4s') failed: %i", reinterpret_cast<const char *>(&propertyID), result);
					return result;
				}
			}
			
			elementCount = 0;
			dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &elementCount, &dataSize);

			if(noErr != result) {
				ERR("AudioUnitGetProperty (kAudioUnitProperty_ElementCount) failed: %i", result);
				return result;
			}
			
			for(UInt32 j = 0; j < elementCount; ++j) {
/*				Boolean writable;
				err = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Output, j, &dataSize, &writable);

				if(noErr != err && kAudioUnitErr_InvalidProperty != err)
					return err;
				 
				if(kAudioUnitErr_InvalidProperty == err || !writable)
					continue;*/
				
				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Output, j, propertyData, propertyDataSize);

				if(noErr != result) {
					ERR("AudioUnitSetProperty ('%.4s') failed: %i", reinterpret_cast<const char *>(&propertyID), result);
					return result;
				}
			}
		}
	}
	
	return noErr;
}

OSStatus DSPAudioPlayer::SetAUGraphSampleRateAndChannelsPerFrame(Float64 sampleRate, UInt32 channelsPerFrame)
{
	// ========================================
	// If the graph is running, stop it
	Boolean graphIsRunning = FALSE;
	OSStatus result = AUGraphIsRunning(mAUGraph, &graphIsRunning);

	if(noErr != result) {
		ERR("AUGraphIsRunning failed: %i", result);
		return result;
	}
	
	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);

		if(noErr != result) {
			ERR("AUGraphStop failed: %i", result);
			return result;
		}
	}
	
	// ========================================
	// If the graph is initialized, uninitialize it
	Boolean graphIsInitialized = FALSE;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);

	if(noErr != result) {
		ERR("AUGraphIsInitialized failed: %i", result);
		return result;
	}
	
	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);

		if(noErr != result) {
			ERR("AUGraphUninitialize failed: %i", result);
			return result;
		}
	}
	
	// ========================================
	// Save the interaction information and then clear all the connections
	UInt32 interactionCount = 0;
	result = AUGraphGetNumberOfInteractions(mAUGraph, &interactionCount);

	if(noErr != result) {
		ERR("AUGraphGetNumberOfInteractions failed: %i", result);
		return result;
	}
	
	AUNodeInteraction interactions [interactionCount];

	for(UInt32 i = 0; i < interactionCount; ++i) {
		result = AUGraphGetInteractionInfo(mAUGraph, i, &interactions[i]);

		if(noErr != result) {
			ERR("AUGraphGetInteractionInfo failed: %i", result);
			return result;
		}
	}
	
	result = AUGraphClearConnections(mAUGraph);

	if(noErr != result) {
		ERR("AUGraphClearConnections failed: %i", result);
		return result;
	}
	
	AudioStreamBasicDescription format = mFormat;
	
	format.mChannelsPerFrame	= channelsPerFrame;
	format.mSampleRate			= sampleRate;

	// ========================================
	// Attempt to set the new stream format
	result = SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &format, sizeof(format));

	if(noErr != result) {
		
		// If the new format could not be set, restore the old format to ensure a working graph
		OSStatus newErr = SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &mFormat, sizeof(mFormat));

		if(noErr != newErr)
			ERR("Unable to restore AUGraph format: %i", result);

		// Do not free connections here, so graph can be rebuilt
		result = newErr;
	}
	else
		mFormat = format;

	
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
					ERR("AUGraphConnectNodeInput failed: %i", result);
					return result;
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
					ERR("AUGraphSetNodeInputCallback failed: %i", result);
					return result;
				}
				
				break;
			}				
		}
	}
	
	// ========================================
	// Output units perform sample rate conversion if the input sample rate is not equal to
	// the output sample rate. For high sample rates, the sample rate conversion can require 
	// more rendered frames than are available by default in kAudioUnitProperty_MaximumFramesPerSlice (512)
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the output device don't match, adjust 
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle

	AudioUnit au = NULL;
	result = AUGraphNodeInfo(mAUGraph, 
							 mOutputNode, 
							 NULL, 
							 &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return result;
	}

	Float64 inputSampleRate = 0;
	UInt32 dataSize = sizeof(inputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0, &inputSampleRate, &dataSize);

	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioUnitProperty_SampleRate) [kAudioUnitScope_Input] failed: %i", result);
		return result;
	}
	
	Float64 outputSampleRate = 0;
	dataSize = sizeof(outputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &outputSampleRate, &dataSize);

	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioUnitProperty_SampleRate) [kAudioUnitScope_Output] failed: %i", result);
		return result;
	}
	
	if(inputSampleRate != outputSampleRate) {
		LOG("Input sample rate (%f) and output sample rate (%f) don't match", inputSampleRate, outputSampleRate);

		UInt32 currentMaxFrames = 0;
		dataSize = sizeof(currentMaxFrames);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &currentMaxFrames, &dataSize);
		
		if(noErr != result) {
			ERR("AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice) failed: %i", result);
			return result;
		}
		
		Float64 ratio = inputSampleRate / outputSampleRate;
		Float64 multiplier = std::max(1.0, ceil(ratio));
		
		// Round up to the nearest power of 16
		UInt32 newMaxFrames = static_cast<UInt32>(currentMaxFrames * multiplier);
		newMaxFrames += 16;
		newMaxFrames &= 0xFFFFFFF0;

		if(newMaxFrames > currentMaxFrames) {
			LOG("Adjusting kAudioUnitProperty_MaximumFramesPerSlice to %d", newMaxFrames);
			
			result = SetPropertyOnAUGraphNodes(kAudioUnitProperty_MaximumFramesPerSlice, &newMaxFrames, sizeof(newMaxFrames));
			
			if(noErr != result) {
				ERR("SetPropertyOnAUGraphNodes (kAudioUnitProperty_MaximumFramesPerSlice) failed: %i", result);
				return result;
			}
		}
	}

	// If the graph was initialized, reinitialize it
	if(graphIsInitialized) {
		result = AUGraphInitialize(mAUGraph);

		if(noErr != result) {
			ERR("AUGraphInitialize failed: %i", result);
			return result;
		}
	}
	
	// If the graph was running, restart it
	if(graphIsRunning) {
		result = AUGraphStart(mAUGraph);

		if(noErr != result) {
			ERR("AUGraphStart failed: %i", result);
			return result;
		}
	}
	
	return noErr;
}

OSStatus DSPAudioPlayer::SetAUGraphChannelLayout(AudioChannelLayout channelLayout)
{
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mOutputNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);
		return result;
	}
	
	// Attempt to set the new channel layout
	result = SetPropertyOnAUGraphNodes(kAudioUnitProperty_AudioChannelLayout, 
									   &channelLayout, 
									   sizeof(channelLayout));
	
	if(noErr != result) {
		ERR("SetPropertyOnAUGraphNodes (kAudioUnitProperty_AudioChannelLayout) failed: %i", result);
		return result;
	}
	else
		mChannelLayout = channelLayout;
	
	return noErr;
}


#pragma mark Other Utilities


DecoderStateData * DSPAudioPlayer::GetCurrentDecoderState()
{
	DecoderStateData *result = NULL;
	for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i) {
		DecoderStateData *decoderState = mActiveDecoders[i];
		
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

DecoderStateData * DSPAudioPlayer::GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp)
{
	DecoderStateData *result = NULL;
	for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i) {
		DecoderStateData *decoderState = mActiveDecoders[i];
		
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

void DSPAudioPlayer::StopActiveDecoders()
{
	// End any still-active decoders
	for(UInt32 i = 0; i < kActiveDecoderArraySize; ++i) {
		DecoderStateData *decoderState = mActiveDecoders[i];
		
		if(NULL == decoderState)
			continue;
		
		OSAtomicTestAndSetBarrier(7 /* eDecoderStateDataFlagDecodingFinished */, &decoderState->mFlags);
		OSAtomicTestAndSetBarrier(6 /* eDecoderStateDataFlagRenderingFinished */, &decoderState->mFlags);
	}
	
	// Signal the collector to collect 
	semaphore_signal(mDecoderSemaphore);
	semaphore_signal(mCollectorSemaphore);
}
