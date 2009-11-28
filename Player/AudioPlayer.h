/*
 *  Copyright (C) 2006, 2007, 2008, 2009 Stephen F. Booth <me@sbooth.org>
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

#include <deque>
#include <AudioToolbox/AudioToolbox.h>


// ========================================
// Forward declarations
// ========================================
class AudioDecoder;
class CARingBuffer;
class DecoderStateData;


// ========================================
// Constants
// ========================================
const UInt32 kActiveDecoderArraySize = 32;


// ========================================
// An audio player class
// ========================================
class AudioPlayer
{
	
public:
	
	// ========================================
	// Creation/Destruction
	AudioPlayer();
	~AudioPlayer();
	
	// ========================================
	// Playback Control
	void Play();
	void Pause();
	inline void PlayPause()					{ IsPlaying() ? Pause() : Play(); }
	void Stop();
	
	bool IsPlaying();

	// ========================================
	// UI properties
	SInt64 GetCurrentFrame();
	SInt64 GetTotalFrames();
	SInt64 GetRemainingFrames()				{ return GetTotalFrames() - GetCurrentFrame(); }
	
	Float64 GetCurrentTime();
	Float64 GetTotalTime();
	inline Float64 GetRemainingTime()		{ return GetTotalTime() - GetCurrentTime(); }

	// ========================================
	// Seeking
	bool SeekForward(UInt32 secondsToSkip = 3);
	bool SeekBackward(UInt32 secondsToSkip = 3);

	bool SeekToTime(Float64 timeInSeconds);
	bool SeekToFrame(SInt64 frame);
	
	// ========================================
	// Player Parameters
	Float32 GetVolume();
	bool SetVolume(Float32 volume);

	Float32 GetPreGain();
	bool SetPreGain(Float32 preGain);

	bool AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask);
	
	// ========================================
	// Device Management
	CFStringRef CreateOutputDeviceUID();
	bool SetOutputDeviceUID(CFStringRef deviceUID);

	Float64 GetOutputDeviceSampleRate();
	bool SetOutputDeviceSampleRate(Float64 sampleRate);

	bool OutputDeviceIsHogged();
	bool StartHoggingOutputDevice();
	inline bool StopHoggingOutputDevice()	{ return StartHoggingOutputDevice(); }

	// ========================================
	// Playlist Management
	// The player will take ownership of decoder
	bool Play(CFURLRef url);
	bool Play(AudioDecoder *decoder);
	
	bool Enqueue(CFURLRef url);
	bool Enqueue(AudioDecoder *decoder);
	
private:
	
	// ========================================
	// AUGraph Utilities
	OSStatus CreateAUGraph();
	OSStatus DisposeAUGraph();
	
	OSStatus ResetAUGraph();
	
	Float64 GetAUGraphLatency();
	Float64 GetAUGraphTailTime();
	
	OSStatus SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize);
	
	OSStatus SetAUGraphFormat(AudioStreamBasicDescription format);
	OSStatus SetAUGraphChannelLayout(AudioChannelLayout channelLayout);
	
	// ========================================
	// PreGain Utilities
	bool EnablePreGain(UInt32 flag);
	bool IsPreGainEnabled();
	
	// ========================================
	// Other Utilities
	void StopActiveDecoders();
	DecoderStateData * GetCurrentDecoderState();

	// ========================================
	// Data Members
	AUGraph								mAUGraph;
	
	AudioStreamBasicDescription			mAUGraphFormat;
	AudioChannelLayout					mAUGraphChannelLayout;
	
	AUNode								mLimiterNode;
	AUNode								mOutputNode;
	
	std::deque<AudioDecoder *>			mDecoderQueue;
	DecoderStateData					*mActiveDecoders [kActiveDecoderArraySize];

	CARingBuffer						*mRingBuffer;
	pthread_mutex_t						mMutex;
	semaphore_t							mDecoderSemaphore;
	semaphore_t							mCollectorSemaphore;
	
	pthread_t							mCollectorThread;
	bool								mKeepCollecting;
	
	SInt64								mFramesDecoded;
	SInt64								mFramesRendered;
	SInt64								mNextDecoderStartingTimeStamp;
	UInt32								mFramesRenderedLastPass;

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
	
	void * FileReaderThreadEntry();
	void * CollectorThreadEntry();

};
