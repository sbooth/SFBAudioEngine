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

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <map>
#include <utility>

// ========================================
// Forward declarations
// ========================================
class AudioDecoder;
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
	eAudioPlayerFlagIsPlaying				= 1u << 0,
	eAudioPlayerFlagMuteOutput				= 1u << 1,
	eAudioPlayerFlagStopRequested			= 1u << 2,
	eAudioPlayerFlagDigitalVolumeEnabled	= 1u << 3,
	eAudioPlayerFlagDigitalPreGainEnabled	= 1u << 4
};

// ========================================
// An audio player class
// The player primarily uses two threads:
//  1) A decoding thread, which reads audio via an AudioDecoder instance and stores it in the ring buffer.
//     The audio is stored as deinterleaved, normalized [-1, 1) native floating point data in 64 bits (AKA doubles)
//  2) A rendering thread, which reads audio from the ring buffer and performs conversion to the required output format.
//     Sample rate conversion is done using Apple's AudioConverter API.
//     Final conversion to the stream's format is done using PCMConverter.
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
	
	inline bool IsPlaying() const					{ return (eAudioPlayerFlagIsPlaying & mFlags); }
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

	bool GetChannelCount(UInt32& channelCount) const;
	bool GetPreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const;

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

	// Will return false if SRC is not being performed
	bool SetSampleRateConverterQuality(UInt32 srcQuality);
	bool SetSampleRateConverterComplexity(OSType srcComplexity);

	// ========================================
	// Device Management
	bool CreateOutputDeviceUID(CFStringRef& deviceUID) const;
	bool SetOutputDeviceUID(CFStringRef deviceUID);
	
	bool GetOutputDeviceID(AudioDeviceID& deviceID) const;
	bool SetOutputDeviceID(AudioDeviceID deviceID);

	bool GetOutputDeviceSampleRate(Float64& sampleRate) const;
	bool SetOutputDeviceSampleRate(Float64 sampleRate);

	bool OutputDeviceIsHogged() const;

	bool StartHoggingOutputDevice();
	bool StopHoggingOutputDevice();

	// ========================================
	// Stream Management
	bool GetOutputStreams(std::vector<AudioStreamID>& streams) const;
	
	bool GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat) const;
	bool SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat);

	bool GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat) const;
	bool SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat);

	// ========================================
	// Playlist Management
	// The player will take ownership of decoder
	bool Enqueue(CFURLRef url);
	bool Enqueue(AudioDecoder *decoder);

	bool SkipToNextTrack();

	bool ClearQueuedDecoders();

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
	// Other Utilities
	void StopActiveDecoders();
	
	DecoderStateData * GetCurrentDecoderState() const;
	DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const;

	bool CreateConvertersAndSRCBuffer();
	
	bool AddVirtualFormatPropertyListeners();
	bool RemoveVirtualFormatPropertyListeners();

	// ========================================
	// Data Members
	AudioDeviceID						mOutputDeviceID;
	AudioDeviceIOProcID					mOutputDeviceIOProcID;
	UInt32								mOutputDeviceBufferFrameSize;
	std::vector<AudioStreamID>			mOutputDeviceStreamIDs;
	
	CARingBuffer						*mRingBuffer;
	AudioStreamBasicDescription			mRingBufferFormat;
	AudioChannelLayout					*mRingBufferChannelLayout;

	PCMConverter						**mOutputConverters;
	AudioConverterRef					mSampleRateConverter;
	AudioBufferList						*mSampleRateConversionBuffer;
	AudioBufferList						*mOutputBuffer;

	volatile uint32_t					mFlags;

	double								mDigitalVolume;
	double								mDigitalPreGain;

	CFMutableArrayRef					mDecoderQueue;
	DecoderStateData					*mActiveDecoders [kActiveDecoderArraySize];

	pthread_mutex_t						mMutex;
	
	pthread_t							mDecoderThread;
	semaphore_t							mDecoderSemaphore;
	bool								mKeepDecoding;
	
	pthread_t							mCollectorThread;
	semaphore_t							mCollectorSemaphore;
	bool								mKeepCollecting;

	int64_t								mFramesDecoded;
	int64_t								mFramesRendered;
	int64_t								mFramesRenderedLastPass;

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

	OSStatus FillSampleRateConversionBuffer(AudioConverterRef				inAudioConverter,
											UInt32							*ioNumberDataPackets,
											AudioBufferList					*ioData,
											AudioStreamPacketDescription	**outDataPacketDescription);
	
	// ========================================
	// Thread entry points
	void * DecoderThreadEntry();
	void * CollectorThreadEntry();

};
