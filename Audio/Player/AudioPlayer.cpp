/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Stephen F. Booth nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY STEPHEN F. BOOTH ''AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL STEPHEN F. BOOTH BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libkern/OSAtomic.h>
#include <pthread.h>
#include <mach/thread_act.h>
#include <mach/mach_error.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>

#include "AudioEngineDefines.h"
#include "AudioPlayer.h"
#include "CoreAudioDecoder.h"

#include "CARingBuffer.h"


// ========================================
// Macros
// ========================================
#define RING_BUFFER_SIZE_FRAMES					16384
#define RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES		2048
#define FEEDER_THREAD_IMPORTANCE				6


// ========================================
// Constants
// ========================================
CFStringRef const AudioPlayerErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.AudioPlayer");


// ========================================
// Utility functions
// ========================================
static bool
channelLayoutsAreEqual(AudioChannelLayout *layoutA,
					   AudioChannelLayout *layoutB)
{
	assert(NULL != layoutA);
	assert(NULL != layoutB);
	
	// First check if the tags are equal
	if(layoutA->mChannelLayoutTag != layoutB->mChannelLayoutTag)
		return false;
	
	// If the tags are equal, check for special values
	if(kAudioChannelLayoutTag_UseChannelBitmap == layoutA->mChannelLayoutTag)
		return (layoutA->mChannelBitmap == layoutB->mChannelBitmap);
	
	if(kAudioChannelLayoutTag_UseChannelDescriptions == layoutA->mChannelLayoutTag) {
		if(layoutA->mNumberChannelDescriptions != layoutB->mNumberChannelDescriptions)
			return false;
		
		size_t bytesToCompare = layoutA->mNumberChannelDescriptions * sizeof(AudioChannelDescription);
		return (0 == memcmp(&layoutA->mChannelDescriptions, &layoutB->mChannelDescriptions, bytesToCompare));
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
		mach_error((char *)"Couldn't set thread's extended policy", error);
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
		mach_error((char *)"Couldn't set thread's precedence policy", error);
#endif
		return false;
	}
	
	return true;
}

static OSStatus
myAURenderCallback(void *							inRefCon,
				   AudioUnitRenderActionFlags *		ioActionFlags,
				   const AudioTimeStamp *			inTimeStamp,
				   UInt32							inBusNumber,
				   UInt32							inNumberFrames,
				   AudioBufferList *				ioData)
{
	assert(NULL != inRefCon);
	
	AudioPlayer *player = (AudioPlayer *)inRefCon;
	return player->Render(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

static void *
fileReaderEntry(void *arg)
{
	assert(NULL != arg);
	
	AudioPlayer *player = (AudioPlayer *)arg;
	return player->FileReaderThreadEntry();
}


#pragma mark Creation/Destruction

AudioPlayer::AudioPlayer()
	: mRingBuffer(NULL), mFramesDecoded(0), mFramesRendered(0)
{
	mRingBuffer = new CARingBuffer();		

	kern_return_t err = semaphore_create(mach_task_self(), &mSemaphore, SYNC_POLICY_FIFO, 0);
	if(KERN_SUCCESS != err) {
#if DEBUG
		mach_error((char *)"semaphore_create", err);
#endif
	}
	
	CreateAUGraph();
}

AudioPlayer::~AudioPlayer()
{
	DisposeAUGraph();
	
	if(mRingBuffer)
		delete mRingBuffer, mRingBuffer = NULL;
}

#pragma mark Playback Control

void AudioPlayer::Play()
{
	if(IsPlaying())
		return;
	
	OSStatus result = AUGraphStart(mAUGraph);

	if(noErr != result)
		ERR("AUGraphStart failed: %i", result);
}

void AudioPlayer::Pause()
{
	if(!IsPlaying())
		return;
	
	OSStatus result = AUGraphStop(mAUGraph);

	if(noErr != result)
		ERR("AUGraphStop failed: %i", result);
}

void AudioPlayer::PlayPause()
{
	IsPlaying() ? Pause() : Play();
}

void AudioPlayer::Stop()
{
	if(!IsPlaying())
		return;

	Pause();
}

bool AudioPlayer::IsPlaying()
{
	Boolean isRunning = FALSE;
	OSStatus result = AUGraphIsRunning(mAUGraph, &isRunning);

	if(noErr != result)
		ERR("AUGraphIsRunning failed: %i", result);
		
	return isRunning;
}

#pragma mark Seeking

#pragma mark Player Parameters

Float32 AudioPlayer::GetVolume()
{
	AudioUnit au = NULL;
	OSStatus auResult = AUGraphNodeInfo(mAUGraph, 
										mOutputNode, 
										NULL, 
										&au);

	if(noErr != auResult) {
		ERR("AUGraphNodeInfo failed: %i", auResult);
		
		return -1;
	}
	
	Float32 volume = -1;
	ComponentResult result = AudioUnitGetParameter(au,
												   kHALOutputParam_Volume,
												   kAudioUnitScope_Global,
												   0,
												   &volume);
	
	if(noErr != result)
		ERR("AudioUnitGetParameter (kHALOutputParam_Volume) failed: %i", result);
		
	return volume;
}

bool AudioPlayer::SetVolume(Float32 volume)
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

Float32 AudioPlayer::GetPreGain()
{
	if(false == PreGainIsEnabled())
		return 0.f;

	AudioUnit au = NULL;
	OSStatus auResult = AUGraphNodeInfo(mAUGraph, 
										mLimiterNode, 
										NULL, 
										&au);
	
	if(noErr != auResult) {
		ERR("AUGraphNodeInfo failed: %i", auResult);

		return -1;
	}

	Float32 preGain = -1;
	ComponentResult result = AudioUnitGetParameter(au, 
												   kLimiterParam_PreGain, 
												   kAudioUnitScope_Global, 
												   0,
												   &preGain);
	
	if(noErr != result)
		ERR("AudioUnitGetParameter (kLimiterParam_PreGain) failed: %i", result);
	
	return preGain;
}

bool AudioPlayer::SetPreGain(Float32 preGain)
{
	if(0.f == preGain)
		return EnablePreGain(false);
	
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mLimiterNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);

		return false;
	}
	
	AudioUnitParameter auParameter;
	
	auParameter.mAudioUnit		= au;
	auParameter.mParameterID	= kLimiterParam_PreGain;
	auParameter.mScope			= kAudioUnitScope_Global;
	auParameter.mElement		= 0;
	
	result	= AUParameterSet(NULL, 
							 NULL, 
							 &auParameter, 
							 preGain,
							 0);
	
	if(noErr != result) {
		ERR("AUParameterSet (kLimiterParam_PreGain) failed: %i", result);

		return false;
	}
	
	return true;
}

#pragma mark Playlist Management

bool AudioPlayer::Play(AudioDecoder *decoder)
{
	assert(NULL != decoder);

	d = decoder;
	
	OSStatus result = SetAUGraphFormat(decoder->GetFormat());

	if(noErr != result) {
		ERR("SetAUGraphFormat failed: %i", result);
		
		return false;
	}

	// Allocate enough space in the ring buffer for the new format
	mRingBuffer->Allocate(decoder->GetFormat().mChannelsPerFrame,
						  decoder->GetFormat().mBytesPerFrame,
						  RING_BUFFER_SIZE_FRAMES);

	// Launch the reader thread for this decoder
	pthread_t fileReaderThread;
		
	if(0 != pthread_create(&fileReaderThread, NULL, fileReaderEntry, this)) {
		return false;
	}
	
	return true;
}

bool AudioPlayer::Enqueue(AudioDecoder *decoder)
{
	assert(NULL != decoder);
	
	/*
	 AudioStreamBasicDescription		format				= [self format];
	 AudioStreamBasicDescription		nextFormat			= [decoder format];
	 
	 AudioChannelLayout				channelLayout		= [self channelLayout];
	 AudioChannelLayout				nextChannelLayout	= [decoder channelLayout];
	 
	 BOOL	formatsMatch			= (nextFormat.mSampleRate == format.mSampleRate && nextFormat.mChannelsPerFrame == format.mChannelsPerFrame);
	 BOOL	channelLayoutsMatch		= channelLayoutsAreEqual(&nextChannelLayout, &channelLayout);
	 
	 // The two files can be joined only if they have the same formats and channel layouts
	 if(NO == formatsMatch || NO == channelLayoutsMatch)
	 return NO;
	 */
	
	return false;
}

#pragma mark Callbacks

OSStatus AudioPlayer::Render(AudioUnitRenderActionFlags		*ioActionFlags,
							 const AudioTimeStamp			*inTimeStamp,
							 UInt32							inBusNumber,
							 UInt32							inNumberFrames,
							 AudioBufferList				*ioData)
{

#pragma unused(ioActionFlags)
#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)
	
	assert(NULL != ioData);
	
	// If the ring buffer doesn't contain any valid audio, skip some work
	UInt32 framesAvailableToRead = (UInt32)(mFramesDecoded - mFramesRendered);
	if(0 == framesAvailableToRead) {
		*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		
		size_t byteCountToZero = inNumberFrames * sizeof(float);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			memset(ioData->mBuffers[bufferIndex].mData, 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
		}
		
		return noErr;
	}

	// Restrict reads to valid decoded audio
	UInt32 framesToRead = framesAvailableToRead < inNumberFrames ? framesAvailableToRead : inNumberFrames;
	CARingBufferError rbResult = mRingBuffer->Fetch(ioData, framesToRead, mFramesRendered, false);
	if(kCARingBufferError_OK != rbResult) {
		ERR("CARingBuffer::Fetch() failed: %i", rbResult);

		return ioErr;
	}

	OSAtomicAdd64/*Barrier*/(framesToRead, &mFramesRendered);
	
	// If the ring buffer didn't contain as many frames as were requested, fill the remainder with silence
	if(framesToRead != inNumberFrames) {
		UInt32 framesOfSilence = inNumberFrames - framesToRead;
		size_t byteCountToZero = framesOfSilence * sizeof(float);
		for(UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; ++bufferIndex) {
			float *bufferAlias = static_cast<float *>(ioData->mBuffers[bufferIndex].mData);
			memset(bufferAlias + (framesToRead * sizeof(float)), 0, byteCountToZero);
			ioData->mBuffers[bufferIndex].mDataByteSize += (UInt32)byteCountToZero;
		}
	}
	
	// If there is adequate space in the ring buffer for another chunk, signal the reader thread
	UInt32 framesAvailableToWrite = (UInt32)(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));
	if(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES <= framesAvailableToWrite)
		semaphore_signal(mSemaphore);
	
	return noErr;
}

void * AudioPlayer::FileReaderThreadEntry()
{
	if(false == setThreadPolicy(FEEDER_THREAD_IMPORTANCE)) {
#if DEBUG
		perror("Couldn't set feeder thread importance");
#endif
	}
	
	AudioDecoder *decoder = d;
	
	// Allocate the buffer list which will serve as the transport between the decoder and the ring buffer
	AudioStreamBasicDescription formatDescription = decoder->GetFormat();
	AudioBufferList *bufferList = static_cast<AudioBufferList *>(calloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer) * (formatDescription.mChannelsPerFrame - 1)), 1));
	
	bufferList->mNumberBuffers = formatDescription.mChannelsPerFrame;
	
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		bufferList->mBuffers[i].mData = static_cast<void *>(calloc(RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES, sizeof(float)));
		bufferList->mBuffers[i].mDataByteSize = RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES * sizeof(float);
		bufferList->mBuffers[i].mNumberChannels = 1;
	}

	// Two seconds and zero nanoseconds
	mach_timespec_t timeout = { 2, 0 };
	
	SInt64 ringBufferOffset = 0;
	
	// Decode the audio file in the ring buffer until finished or cancelled
	bool finished = false;
	while(false == finished) {
		
		// Fill the ring buffer with as much data as possible
		for(;;) {
			
			// Determine how many frames are available in the ring buffer
			UInt32 framesAvailableToWrite = (UInt32)(RING_BUFFER_SIZE_FRAMES - (mFramesDecoded - mFramesRendered));
			
			// Force writes to the ring buffer to be at least RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES
			if(framesAvailableToWrite >= RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES) {
				SInt64 startingFrameNumber = decoder->CurrentFrame();
				
				// Read the input chunk
				UInt32 framesDecoded = decoder->ReadAudio(bufferList, RING_BUFFER_WRITE_CHUNK_SIZE_FRAMES);
				
				// If this is the first frame, decoding is just starting
				if(0 == startingFrameNumber) {
					LOG("Starting decoding");
					//				OSAtomicCompareAndSwap32(0, 1, &_decodingStarted);
					//				
					//				if(_owner && [_owner respondsToSelector:@selector(bufferStartedDecoding:)])
					//					[_owner performSelectorOnMainThread:@selector(bufferStartedDecoding:) withObject:self waitUntilDone:NO];
				}
				
				// Store the decoded audio
				if(0 != framesDecoded) {
#if USE_CAMUTEX
					CAMutex::Locker lock(*(PRIVATE_DATA->_mutex));
#endif
					
					// Copy the decoded audio to the ring buffer
					CARingBufferError rbResult = mRingBuffer->Store(bufferList, framesDecoded, startingFrameNumber + ringBufferOffset);
					if(kCARingBufferError_OK != rbResult)
						ERR("CARingBuffer::Store() failed: %i", rbResult);
					
					OSAtomicAdd64/*Barrier*/(framesDecoded, &mFramesDecoded);
				}
				
				// If no frames were returned, this is the end of stream
				if(0 == framesDecoded) {
					//				OSAtomicCompareAndSwap32(0, 1, &_decodingFinished);
					//				
					//				if(_owner && [_owner respondsToSelector:@selector(bufferFinishedDecoding:)])
					//					[_owner performSelectorOnMainThread:@selector(bufferFinishedDecoding:) withObject:self waitUntilDone:NO];

					finished = true;
					
					break;
				}
				
			}
			// Not enough space remains in the ring buffer to write an entire decoded chunk
			else
				break;
		}
		
		// Wait for the audio rendering thread to signal us that it could use more data, or for the timeout to happen
		semaphore_timedwait(mSemaphore, timeout);
	}
	
	if(bufferList) {
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			free(bufferList->mBuffers[bufferIndex].mData), bufferList->mBuffers[bufferIndex].mData = NULL;
		
		free(bufferList), bufferList = NULL;
	}
	
	return NULL;
}

#pragma mark AUGraph Utilities

OSStatus AudioPlayer::CreateAUGraph()
{
	OSStatus result = NewAUGraph(&mAUGraph);

	if(noErr != result) {
		ERR("NewAUGraph failed: %i", result);

		return result;
	}
	
	// The graph will look like:
	// Peak Limiter -> Effects -> Output
	ComponentDescription desc;

	// Set up the peak limiter node
	desc.componentType			= kAudioUnitType_Effect;
	desc.componentSubType		= kAudioUnitSubType_PeakLimiter;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	
	result = AUGraphAddNode(mAUGraph, &desc, &mLimiterNode);

	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);

		return result;
	}
	
	// Set up the output node
	desc.componentType			= kAudioUnitType_Output;
	desc.componentSubType		= kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= 0;
	desc.componentFlagsMask		= 0;
	
	result = AUGraphAddNode(mAUGraph, &desc, &mOutputNode);

	if(noErr != result) {
		ERR("AUGraphAddNode failed: %i", result);

		return result;
	}
	
	result = AUGraphConnectNodeInput(mAUGraph, mLimiterNode, 0, mOutputNode, 0);

	if(noErr != result) {
		ERR("AUGraphConnectNodeInput failed: %i", result);

		return result;
	}
	
	// Install the input callback
	AURenderCallbackStruct cbs = { myAURenderCallback, this };
	result = AUGraphSetNodeInputCallback(mAUGraph, mLimiterNode, 0, &cbs);

	if(noErr != result) {
		ERR("AUGraphSetNodeInputCallback failed: %i", result);

		return result;
	}
	
	// Open the graph
	result = AUGraphOpen(mAUGraph);

	if(noErr != result) {
		ERR("AUGraphOpen failed: %i", result);

		return result;
	}
	
	// Initialize the graph
	result = AUGraphInitialize(mAUGraph);

	if(noErr != result) {
		ERR("AUGraphInitialize failed: %i", result);

		return result;
	}
	
	// TODO: Install a render callback on the output node for more accurate tracking?
	
	return noErr;
}

OSStatus AudioPlayer::DisposeAUGraph()
{
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
	
	result = AUGraphClose(mAUGraph);

	if(noErr != result) {
		ERR("AUGraphClose failed: %i", result);

		return result;
	}
	
	result = ::DisposeAUGraph(mAUGraph);

	if(noErr != result) {
		ERR("DisposeAUGraph failed: %i", result);

		return result;
	}
	
	mAUGraph = NULL;
	
	return noErr;
}

OSStatus AudioPlayer::ResetAUGraph()
{
	UInt32 nodeCount = 0;
	OSStatus result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		ERR("AUGraphGetNodeCount failed: %i", result);

		return result;
	}
	
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node = 0;
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
		
		result = AudioUnitReset(au, kAudioUnitScope_Global, 0);
		if(noErr != result) {
			ERR("AudioUnitReset failed: %i", result);

			return result;
		}
	}
	
	return noErr;
}

Float64 AudioPlayer::GetAUGraphLatency()
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

Float64 AudioPlayer::GetAUGraphTailTime()
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

OSStatus AudioPlayer::SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize)
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
			
// IO must be enabled for this to work
/*			err = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Output, 1, propertyData, propertyDataSize);

			if(noErr != err)
				return err;*/
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

OSStatus AudioPlayer::SetAUGraphFormat(AudioStreamBasicDescription format)
{
	AUNodeInteraction *interactions = NULL;
	
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
	
	interactions = static_cast<AUNodeInteraction *>(calloc(interactionCount, sizeof(AUNodeInteraction)));
	if(NULL == interactions)
		return memFullErr;
	
	for(UInt32 i = 0; i < interactionCount; ++i) {
		result = AUGraphGetInteractionInfo(mAUGraph, i, &interactions[i]);

		if(noErr != result) {
			ERR("AUGraphGetInteractionInfo failed: %i", result);

			free(interactions);

			return result;
		}
	}
	
	result = AUGraphClearConnections(mAUGraph);

	if(noErr != result) {
		ERR("AUGraphClearConnections failed: %i", result);

		free(interactions);
		
		return result;
	}
	
	// ========================================
	// Attempt to set the new stream format
	result = SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &format, sizeof(format));

	if(noErr != result) {
		
		// If the new format could not be set, restore the old format to ensure a working graph
		OSStatus newErr = SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &mAUGraphFormat, sizeof(mAUGraphFormat));

		if(noErr != newErr)
			ERR("Unable to restore AUGraph format: %i", result);

		// Do not free connections here, so graph can be rebuilt
		result = newErr;
	}
	else
		mAUGraphFormat = format;

	
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

					free(interactions), interactions = NULL;
					
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

					free(interactions), interactions = NULL;
					
					return result;
				}
				
				break;
			}				
		}
	}
	
	free(interactions), interactions = NULL;
	
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

OSStatus AudioPlayer::SetAUGraphChannelLayout(AudioChannelLayout /*channelLayout*/)
{
	/*
	 // Attempt to set the new channel layout
	 //	OSStatus err = [self setPropertyOnAUGraphNodes:kAudioUnitProperty_AudioChannelLayout data:&channelLayout dataSize:sizeof(channelLayout)];
	 OSStatus err = AudioUnitSetProperty(_outputUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &channelLayout, sizeof(channelLayout));

	 if(noErr != err) {
	 // If the new format could not be set, restore the old format to ensure a working graph
	 channelLayout = [self channelLayout];
	 //		OSStatus newErr = [self setPropertyOnAUGraphNodes:kAudioUnitProperty_AudioChannelLayout data:&channelLayout dataSize:sizeof(channelLayout)];
	 OSStatus newErr = AudioUnitSetProperty(_outputUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &channelLayout, sizeof(channelLayout));

	 if(noErr != newErr)
	 NSLog(@"AudioPlayer error: Unable to restore AUGraph channel layout: %i", newErr);
	 
	 return err;
	 }
	 */
	return noErr;
}

bool AudioPlayer::EnablePreGain(UInt32 flag)
{
	if(flag && PreGainIsEnabled())
		return true;
	else if(!flag && false == PreGainIsEnabled())
		return true;
	
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mLimiterNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);

		return false;
	}
	
	result = AudioUnitSetProperty(au, 
								  kAudioUnitProperty_BypassEffect,
								  kAudioUnitScope_Global, 
								  0, 
								  &flag, 
								  sizeof(flag));
	
	if(noErr != result) {
		ERR("AudioUnitSetProperty (kAudioUnitProperty_BypassEffect) failed: %i", result);

		return false;
	}
	
	return true;
}

bool AudioPlayer::PreGainIsEnabled()
{
	AudioUnit au = NULL;
	OSStatus result = AUGraphNodeInfo(mAUGraph, 
									  mLimiterNode, 
									  NULL, 
									  &au);
	
	if(noErr != result) {
		ERR("AUGraphNodeInfo failed: %i", result);

		return false;
	}
	
	UInt32 bypassed	= FALSE;
	UInt32 dataSize	= sizeof(bypassed);
	
	result = AudioUnitGetProperty(au, 
								  kAudioUnitProperty_BypassEffect, 
								  kAudioUnitScope_Global, 
								  0,
								  &bypassed,
								  &dataSize);
	
	if(noErr != result) {
		ERR("AudioUnitGetProperty (kAudioUnitProperty_BypassEffect) failed: %i", result);

		return false;
	}
	
	return bypassed;
}
