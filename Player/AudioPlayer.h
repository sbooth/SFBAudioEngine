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

#pragma once

#include <deque>
#include <AudioToolbox/AudioToolbox.h>


// ========================================
// Forward declarations
// ========================================
class AudioDecoder;
class DecoderStateData;


// ========================================
// Constants
// ========================================
#define kActiveDecoderArraySize 8


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
	inline void PlayPause()							{ IsPlaying() ? Pause() : Play(); }
	void Stop();
	
	inline bool IsPlaying()							{ return mIsPlaying; }
	CFURLRef GetPlayingURL();

	// ========================================
	// Playback Properties
	SInt64 GetCurrentFrame();
	SInt64 GetTotalFrames();
	inline SInt64 GetRemainingFrames()				{ return GetTotalFrames() - GetCurrentFrame(); }
	
	CFTimeInterval GetCurrentTime();
	CFTimeInterval GetTotalTime();
	inline CFTimeInterval GetRemainingTime()		{ return GetTotalTime() - GetCurrentTime(); }
	
	// ========================================
	// Seeking
	bool SeekForward(CFTimeInterval secondsToSkip = 3);
	bool SeekBackward(CFTimeInterval secondsToSkip = 3);

	bool SeekToTime(CFTimeInterval timeInSeconds);
	bool SeekToFrame(SInt64 frame);
	
	bool SupportsSeeking();
	
	// ========================================
	// Player Parameters
	bool GetMasterVolume(Float32& volume);
	bool SetMasterVolume(Float32 volume);

	bool GetVolumeForChannel(UInt32 channel, Float32& volume);
	bool SetVolumeForChannel(UInt32 channel, Float32 volume);

	// ========================================
	// The format and channel layout of the audio being sent to the output device
	bool GetFormat(AudioStreamBasicDescription& format);
	bool GetChannelLayout(AudioChannelLayout& channelLayout);

	// ========================================
	// Device Management
	CFStringRef CreateOutputDeviceUID();
	bool SetOutputDeviceUID(CFStringRef deviceUID);
	
	AudioDeviceID GetOutputDeviceID()				{ return mOutputDeviceID; }
	bool SetOutputDeviceID(AudioDeviceID deviceID);

	bool GetOutputDeviceSampleRate(Float64& sampleRate);
	bool SetOutputDeviceSampleRate(Float64 sampleRate);

	bool StartHoggingOutputDevice();
	bool StopHoggingOutputDevice();

	AudioStreamID GetOutputStreamID()				{ return mOutputStreamID; }
	bool SetOutputStreamID(AudioStreamID streamID);
	
	// ========================================
	// Playlist Management
	// The player will take ownership of decoder
	bool Enqueue(CFURLRef url);
	bool Enqueue(AudioDecoder *decoder);
	
	bool ClearQueuedDecoders();

private:

	// ========================================
	// AudioHardware Utilities (for non-mixable audio)
	bool OpenOutput();
	bool CloseOutput();

	bool StartOutput();
	bool StopOutput();

	bool OutputIsRunning();
	bool ResetOutput();
	
	bool SetFormat(AudioStreamBasicDescription format);
	bool SetChannelLayout(AudioChannelLayout channelLayout);

	bool OutputDeviceIsHogged();

	// ========================================
	// Other Utilities
	void StopActiveDecoders();
	
	DecoderStateData * GetCurrentDecoderState();
	DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp);

	// ========================================
	// Data Members
	AudioDeviceID						mOutputDeviceID;
	AudioDeviceIOProcID					mOutputDeviceIOProcID;
	
	AudioStreamID						mOutputStreamID;

	AudioStreamBasicDescription			mFormat;
	UInt32								mFormatChanged;
	AudioChannelLayout					mChannelLayout;

	bool								mIsPlaying;

	std::deque<AudioDecoder *>			mDecoderQueue;
	DecoderStateData					*mActiveDecoders [kActiveDecoderArraySize];

	void								*mRingBuffer;
	pthread_mutex_t						mMutex;
	
	pthread_t							mDecoderThread;
	semaphore_t							mDecoderSemaphore;
	bool								mKeepDecoding;
	
	pthread_t							mCollectorThread;
	semaphore_t							mCollectorSemaphore;
	bool								mKeepCollecting;

public:

	// ========================================
	// Callbacks- for internal use only
	OSStatus Render(AudioDeviceID			inDevice,
					const AudioTimeStamp	*inNow,
					const AudioBufferList	*inInputData,
					const AudioTimeStamp	*inInputTime,
					AudioBufferList			*outOutputData,
					const AudioTimeStamp	*inOutputTime);
	
	OSStatus AudioObjectPropertyChanged(AudioObjectID						inObjectID,
										UInt32								inNumberAddresses,
										const AudioObjectPropertyAddress	inAddresses[]);

	// ========================================
	// Thread entry points
	void * DecoderThreadEntry();
	void * CollectorThreadEntry();

};
