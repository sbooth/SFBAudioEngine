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

#pragma once

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>

#include <vector>
#include <map>
#include <utility>

#include "AudioDecoder.h"
#include "Guard.h"
#include "Semaphore.h"

// ========================================
// Forward declarations
// ========================================
class CARingBuffer;
class DecoderStateData;
class PCMConverter;

// ========================================
// Constants
// ========================================
#define kActiveDecoderArraySize 8

// ========================================
// Enums
// ========================================
enum {
	eAudioPlayerFlagMuteOutput				= 1u << 0,
	eAudioPlayerFlagDigitalVolumeEnabled	= 1u << 1,
	eAudioPlayerFlagDigitalPreGainEnabled	= 1u << 2
};

// ========================================
// An audio player class
//
// The player primarily uses two threads:
//  1) A decoding thread, which reads audio via an AudioDecoder instance and stores it in the ring buffer.
//     The audio is stored as deinterleaved, normalized [-1, 1) native floating point data in 64 bits (AKA doubles)
//  2) A rendering thread, which reads audio from the ring buffer and performs conversion to the required output format.
//     Sample rate conversion is done using Apple's AudioConverter API.
//     Final conversion to the stream's format is done using PCMConverter.
//
// Since decoding and rendering are distinct operations performed in separate threads, there is an additional thread
// used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
// rendering is complete, which cannot occur until after decoding is complete.  An alternative garbage collection
// method would be hazard pointers.
// ========================================
class iOSAudioPlayer
{
	
public:

	// ========================================
	// Creation/Destruction
	iOSAudioPlayer();
	~iOSAudioPlayer();
	
	// ========================================
	// Playback Control
	bool Play();
	bool Pause();
	inline bool PlayPause()							{ return IsPlaying() ? Pause() : Play(); }
	bool Stop();

	// ========================================
	// Player State
	enum PlayerState {
		ePlaying,	// Audio is being sent to the output device
		ePaused,	// An AudioDecoder has started rendering, but audio is not being sent to the output device
		ePending,	// An AudioDecoder has started decoding, but not yet started rendering
		eStopped	// An AudioDecoder has not started decoding, or the decoder queue is empty
	};

	PlayerState GetPlayerState() const;

	// Convenience methods
	inline bool IsPlaying() const					{ return (ePlaying == GetPlayerState()); }
	inline bool IsPaused() const					{ return (ePaused == GetPlayerState()); }
	inline bool IsPending() const					{ return (ePending == GetPlayerState()); }
	inline bool IsStopped() const					{ return (eStopped == GetPlayerState()); }

	CFURLRef GetPlayingURL() const;

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
	bool GetMasterVolume(Float32& volume) const;
	bool SetMasterVolume(Float32 volume);

	bool GetVolumeForChannel(UInt32 channel, Float32& volume) const;
	bool SetVolumeForChannel(UInt32 channel, Float32 volume);

	inline bool DigitalVolumeIsEnabled() const		{ return (eAudioPlayerFlagDigitalVolumeEnabled & mFlags); }
	void EnableDigitalVolume(bool enableDigitalVolume);

	// volume should be in the range [0, 1] (linear)
	bool GetDigitalVolume(double& volume) const;
	bool SetDigitalVolume(double volume);
	
	inline bool DigitalPreGainIsEnabled() const		{ return (eAudioPlayerFlagDigitalPreGainEnabled & mFlags); }
	void EnableDigitalPreGain(bool enableDigitalPreGain);

	// preGain should be in the range [-15, 15] (dB)
	bool GetDigitalPreGain(double& preGain) const;
	bool SetDigitalPreGain(double preGain);

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

private:

	// ========================================
	// AudioHardware Utilities (for non-mixable audio)
	bool OpenOutput();
	bool CloseOutput();

	bool StartOutput();
	bool StopOutput();

	bool OutputIsRunning() const;
	bool ResetOutput();

	// ========================================
	// AUGraph Utilities
	Float64 GetAUGraphLatency();
	Float64 GetAUGraphTailTime();

	bool SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize);

	bool SetAUGraphSampleRateAndChannelsPerFrame(Float64 sampleRate, UInt32 channelsPerFrame);
	bool SetAUGraphChannelLayout(AudioChannelLayout *channelLayout);

	// ========================================
	// Other Utilities
	void StopActiveDecoders();
	
	DecoderStateData * GetCurrentDecoderState() const;
	DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const;

	// ========================================
	// Data Members
	AUGraph								mAUGraph;
	AUNode								mOutputNode;

	CARingBuffer						*mRingBuffer;
	AudioStreamBasicDescription			mRingBufferFormat;
	AudioChannelLayout					*mRingBufferChannelLayout;
	uint32_t							mRingBufferCapacity;
	uint32_t							mRingBufferWriteChunkSize;

	volatile uint32_t					mFlags;

	double								mDigitalVolume;
	double								mDigitalPreGain;

	CFMutableArrayRef					mDecoderQueue;
	DecoderStateData					*mActiveDecoders [kActiveDecoderArraySize];

	Guard								mGuard;

	pthread_t							mDecoderThread;
	Semaphore							mDecoderSemaphore;
	bool								mKeepDecoding;

	pthread_t							mCollectorThread;
	Semaphore							mCollectorSemaphore;
	bool								mKeepCollecting;

	int64_t								mFramesDecoded;
	int64_t								mFramesRendered;
	int64_t								mFramesRenderedLastPass;

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
