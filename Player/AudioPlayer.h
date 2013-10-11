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

#pragma once

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>

#include <vector>
#include <map>
#include <utility>

#include "AudioDecoder.h"
#include "Mutex.h"
#include "Semaphore.h"

// ========================================
// Forward declarations
// ========================================
class RingBuffer;

// ========================================
// Constants
// ========================================
#define kActiveDecoderArraySize 8

// ========================================
// An audio player class
//
// The player primarily uses two threads:
//  1) A decoding thread, which reads audio via an AudioDecoder instance and stores it in the ring buffer.
//     The audio is stored in the canonical Core Audio format (kAudioFormatFlagsAudioUnitCanonical)-
//     deinterleaved, normalized [-1, 1) native floating point data in 32 bits (AKA floats) on Mac OS X and 8.24 fixed point on iOS
//  2) A rendering thread, which reads audio from the ring buffer and hands it off to the output AU.
//
// Since decoding and rendering are distinct operations performed in separate threads, there is an additional thread
// used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
// rendering is complete, which cannot occur until after decoding is complete.  An alternative garbage collection
// method would be hazard pointers.
//
// The player supports block-based callbacks for the following events:
//  1) Decoding started
//  2) Decoding finished
//  3) Rendering started
//  4) Rendering finished
//  5) Pre- and post- audio rendering
//  6) Audio format mismatches preventing gapless playback
//
// The decoding callbacks will be performed from the decoding thread.  Although not a real time thread,
// lengthy operations should be avoided to prevent audio glitching.
//
// The rendering callbacks will be performed from the realtime rendering thread.  Execution of this thread must not be blocked!  
// Examples of prohibited actions that could cause problems:
//  - Memory allocation
//  - Objective-C messaging
//  - File IO
// ========================================
class AudioPlayer
{
	
public:
	// ========================================
	// Block callback types
	typedef void (^AudioPlayerDecoderEventBlock)(const AudioDecoder *decoder);
	typedef void (^AudioPlayerRenderEventBlock)(AudioBufferList *data, UInt32 frameCount);
	typedef void (^AudioPlayerFormatMismatchBlock)(AudioStreamBasicDescription currentFormat, AudioStreamBasicDescription nextFormat);

	// ========================================
	// Creation/Destruction
	AudioPlayer();
	~AudioPlayer();
	
	// This class is non-copyable
	AudioPlayer(const AudioPlayer& rhs) = delete;
	AudioPlayer& operator=(const AudioPlayer& rhs) = delete;

	// ========================================
	// Playback Control
	bool Play();
	bool Pause();
	inline bool PlayPause()							{ return IsPlaying() ? Pause() : Play(); }
	bool Stop();

	// ========================================
	// Player State
	enum class PlayerState {
		Playing,	// Audio is being sent to the output device
		Paused,		// An AudioDecoder has started rendering, but audio is not being sent to the output device
		Pending,	// An AudioDecoder has started decoding, but not yet started rendering
		Stopped		// An AudioDecoder has not started decoding, or the decoder queue is empty
	};

	PlayerState GetPlayerState() const;

	// Convenience methods
	inline bool IsPlaying() const					{ return (PlayerState::Playing == GetPlayerState()); }
	inline bool IsPaused() const					{ return (PlayerState::Paused == GetPlayerState()); }
	inline bool IsPending() const					{ return (PlayerState::Pending == GetPlayerState()); }
	inline bool IsStopped() const					{ return (PlayerState::Stopped == GetPlayerState()); }

	// ========================================
	// Information on the decoder that is currently rendering
	CFURLRef GetPlayingURL() const;
	void * GetPlayingRepresentedObject() const;

	// ========================================
	// Block-based callback support
	void SetDecodingStartedBlock(AudioPlayerDecoderEventBlock block);		// Called from the decoding thread before the first audio frame is decoded
	void SetDecodingFinishedBlock(AudioPlayerDecoderEventBlock block);		// Called from the decoding thread after the last audio frame has been decoded
	void SetRenderingStartedBlock(AudioPlayerDecoderEventBlock block);		// Called from the real-time rendering thread before the first audio frame is rendered
	void SetRenderingFinishedBlock(AudioPlayerDecoderEventBlock block);		// Called from the real-time rendering thread after the last audio frame is rendered

	void SetPreRenderBlock(AudioPlayerRenderEventBlock block);				// Called from the real-time rendering thread before audio is rendered
	void SetPostRenderBlock(AudioPlayerRenderEventBlock block);				// Called from the real-time rendering thread after audio is rendered

	void SetFormatMismatchBlock(AudioPlayerFormatMismatchBlock block);		// Called from the decoding thread when the player's sample rate or channel count will change

	// ========================================
	// Playback Properties
	bool GetCurrentFrame(SInt64& currentFrame) const;
	bool GetTotalFrames(SInt64& totalFrames) const;
	bool GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const;

	bool GetCurrentTime(CFTimeInterval& currentTime) const;
	bool GetTotalTime(CFTimeInterval& totalTime) const;
	bool GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;

	bool GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;

	// ========================================
	// Seeking
	bool SeekForward(CFTimeInterval secondsToSkip = 3);
	bool SeekBackward(CFTimeInterval secondsToSkip = 3);

	bool SeekToTime(CFTimeInterval timeInSeconds);
	bool SeekToFrame(SInt64 frame);
	
	bool SupportsSeeking() const;

	// ========================================
	// Player Parameters
	// volume should be in the range [0, 1] (linear)
	bool GetVolume(Float32& volume) const;
	bool SetVolume(Float32 volume);

	bool GetVolumeForChannel(UInt32 channel, Float32& volume) const;
	bool SetVolumeForChannel(UInt32 channel, Float32 volume);
	
	// preGain should be in the range [0, 1] (linear)
	bool GetPreGain(Float32& preGain) const;
	bool SetPreGain(Float32 preGain);

	bool IsPerformingSampleRateConversion() const;
	bool GetSampleRateConverterComplexity(UInt32& complexity) const;
	bool SetSampleRateConverterComplexity(UInt32 complexity);

#if !TARGET_OS_IPHONE
	// ========================================
	// Hog Mode
	bool OutputDeviceIsHogged() const;
	bool StartHoggingOutputDevice();
	bool StopHoggingOutputDevice();

	// ========================================
	// Device parameters
	bool GetDeviceMasterVolume(Float32& volume) const;
	bool SetDeviceMasterVolume(Float32 volume);

	bool GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const;
	bool SetDeviceVolumeForChannel(UInt32 channel, Float32 volume);

	bool GetDeviceChannelCount(UInt32& channelCount) const;
	bool GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const;

	// ========================================
	// DSP Effects
	bool AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit = nullptr);
	bool RemoveEffect(AudioUnit effectUnit);

	// ========================================
	// Device Management
	bool CreateOutputDeviceUID(CFStringRef& deviceUID) const;
	bool SetOutputDeviceUID(CFStringRef deviceUID);

	bool GetOutputDeviceID(AudioDeviceID& deviceID) const;
	bool SetOutputDeviceID(AudioDeviceID deviceID);

	bool GetOutputDeviceSampleRate(Float64& sampleRate) const;
	bool SetOutputDeviceSampleRate(Float64 sampleRate);
#endif

	// ========================================
	// Playlist Management
	// The player will take ownership of decoder
	bool Enqueue(CFURLRef url);
	bool Enqueue(AudioDecoder *decoder);

	bool SkipToNextTrack();

	bool ClearQueuedDecoders();

	// ========================================
	// Ring Buffer Parameters
	// The ring buffer's capacity, in sample frames
	inline uint32_t GetRingBufferCapacity() const	{ return mRingBufferCapacity; }
	bool SetRingBufferCapacity(uint32_t bufferCapacity);

	// The minimum size of writes to the ring buffer, which implies the minimum read size from an AudioDecoder
	inline uint32_t GetRingBufferWriteChunkSize() const	{ return mRingBufferWriteChunkSize; }
	bool SetRingBufferWriteChunkSize(uint32_t chunkSize);

	// This class is exposed so it can be used inside C callbacks
	class DecoderStateData;

private:

	// ========================================
	// AUGraph Setup and Control
	bool OpenOutput();
	bool CloseOutput();

	bool StartOutput();
	bool StopOutput();

	bool OutputIsRunning() const;
	bool ResetOutput();

	// ========================================
	// AUGraph Utilities
	bool GetAUGraphLatency(Float64& latency) const;
	bool GetAUGraphTailTime(Float64& tailTime) const;

	bool SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize);

	bool SetAUGraphSampleRateAndChannelsPerFrame(Float64 sampleRate, UInt32 channelsPerFrame);

	bool SetOutputUnitChannelMap(AudioChannelLayout *channelLayout);

	// ========================================
	// Other Utilities
	void StopActiveDecoders();
	
	DecoderStateData * GetCurrentDecoderState() const;
	DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const;

	bool SetupAUGraphAndRingBufferForDecoder(AudioDecoder *decoder);

	// ========================================
	// Data Members
	AUGraph								mAUGraph;
	AUNode								mMixerNode;
	AUNode								mOutputNode;
	UInt32								mDefaultMaximumFramesPerSlice;

	RingBuffer							*mRingBuffer;
	AudioStreamBasicDescription			mRingBufferFormat;
	AudioChannelLayout					*mRingBufferChannelLayout;
	uint32_t							mRingBufferCapacity;
	uint32_t							mRingBufferWriteChunkSize;

	volatile uint32_t					mFlags;

	CFMutableArrayRef					mDecoderQueue;
	DecoderStateData					*mActiveDecoders [kActiveDecoderArraySize];

	Mutex								mMutex;
	Semaphore							mSemaphore;

	pthread_t							mDecoderThread;
	Semaphore							mDecoderSemaphore;
	bool								mKeepDecoding;

	pthread_t							mCollectorThread;
	Semaphore							mCollectorSemaphore;
	bool								mKeepCollecting;

	int64_t								mFramesDecoded;
	int64_t								mFramesRendered;
	int64_t								mFramesRenderedLastPass;

	// ========================================
	// Callbacks
	AudioPlayerDecoderEventBlock		mDecoderEventBlocks [4];
	AudioPlayerRenderEventBlock			mRenderEventBlocks [2];
	AudioPlayerFormatMismatchBlock		mFormatMismatchBlock;
	
public:

	// ========================================
	// Callbacks- for internal use only
	OSStatus Render(AudioUnitRenderActionFlags		*ioActionFlags,
					const AudioTimeStamp			*inTimeStamp,
					UInt32							inBusNumber,
					UInt32							inNumberFrames,
					AudioBufferList					*ioData);

	OSStatus DidRender(AudioUnitRenderActionFlags		*ioActionFlags,
					   const AudioTimeStamp				*inTimeStamp,
					   UInt32							inBusNumber,
					   UInt32							inNumberFrames,
					   AudioBufferList					*ioData);
	
	// ========================================
	// Thread entry points
	void * DecoderThreadEntry();
	void * CollectorThreadEntry();

};
