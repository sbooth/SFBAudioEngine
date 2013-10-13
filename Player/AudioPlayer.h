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

/*!
 * @brief An audio player class
 *
 * The player primarily uses two threads:
 *  1. A decoding thread, which reads audio via an AudioDecoder instance and stores it in the ring buffer.
 *     The audio is stored in the canonical Core Audio format (kAudioFormatFlagsAudioUnitCanonical)-
 *     deinterleaved, normalized [-1, 1) native floating point data in 32 bits (AKA floats) on Mac OS X and 8.24 fixed point on iOS
 *  2. A rendering thread, which reads audio from the ring buffer and hands it off to the output AU.
 *
 * Since decoding and rendering are distinct operations performed in separate threads, there is an additional thread
 * used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
 * rendering is complete, which cannot occur until after decoding is complete.  An alternative garbage collection
 * method would be hazard pointers.
 *
 * The player supports block-based callbacks for the following events:
 *  1. Decoding started
 *  2. Decoding finished
 *  3. Rendering started
 *  4. Rendering finished
 *  5. Pre- and post- audio rendering
 *  6. Audio format mismatches preventing gapless playback
 *
 * The decoding callbacks will be performed from the decoding thread.  Although not a real time thread,
 * lengthy operations should be avoided to prevent audio glitching.
 *
 * The rendering callbacks will be performed from the realtime rendering thread.  Execution of this thread must not be blocked!
 * Examples of prohibited actions that could cause problems:
 *  - Memory allocation
 *  - Objective-C messaging
 *  - File IO
 */
class AudioPlayer
{
	
public:
	// ========================================
	/*! @name Block callback types */
	//@{

	/*!
	 * @brief A block called when an event occurs on an \c AudioDecoder
	 * @param decoder The \c AudioDecoder on which the event occurred
	 */
	typedef void (^AudioPlayerDecoderEventBlock)(const AudioDecoder *decoder);

	/*!
	 * @brief A block called when an \c AudioPlayer render event occurs
	 * @param data The audio data
	 * @param frameCount The number of frames in \c data
	 */
	typedef void (^AudioPlayerRenderEventBlock)(AudioBufferList *data, UInt32 frameCount);

	/*!
	 * @brief A block called when the audio format of the next \c AudioDecoder does not match the current format
	 * @param currentFormat The current audio format
	 * @param nextFormat The next audio format
	 */
	typedef void (^AudioPlayerFormatMismatchBlock)(AudioStreamBasicDescription currentFormat, AudioStreamBasicDescription nextFormat);

	//@}


	// ========================================
	/*! @name Creation and Destruction */
	//@{

	/*!
	 * @brief Create a new \c AudioPlayer for the default output device
	 * @throws std::bad_alloc
	 * @throws std::runtime_error
	 */
	AudioPlayer();

	/*! @brief Destroy the \c AudioPlayer and release all associated resources. */
	~AudioPlayer();

	/*! @cond */

	/*! @internal This class is non-copyable */
	AudioPlayer(const AudioPlayer& rhs) = delete;

	/*! @internal This class is non-assignable */
	AudioPlayer& operator=(const AudioPlayer& rhs) = delete;

	/*! @endcond */

	//@}


	// ========================================
	/*!
	 * @name Playback Control
	 * These methods return \c true on success, \c false otherwise.
	 */
	//@{

	/*! @brief Start playback */
	bool Play();

	/*! @brief Pause playback */
	bool Pause();

	/*! @brief Start playback if paused, or pause playback if playing */
	inline bool PlayPause()							{ return IsPlaying() ? Pause() : Play(); }

	/*! @brief Stop playback */
	bool Stop();

	//@}


	// ========================================
	/*! @name Player State */
	//@{

	// ========================================
	/*! @brief Possible player states */
	enum class PlayerState {
		Playing,	/*!< Audio is being sent to the output device */
		Paused,		/*!< An \c AudioDecoder has started rendering, but audio is not being sent to the output device */
		Pending,	/*!< An \c AudioDecoder has started decoding, but not yet started rendering */
		Stopped		/*!< An \c AudioDecoder has not started decoding, or the decoder queue is empty */
	};

	/*! @brief Get the current player state */
	PlayerState GetPlayerState() const;

	// Convenience methods

	/*!
	 * @brief Determine if the player is playing
	 * @return \c true if audio is being sent to the device, \c false otherwise
	 */
	inline bool IsPlaying() const					{ return (PlayerState::Playing == GetPlayerState()); }

	/*!
	 * @brief Determine if the player is paused
	 * @return \c true if an \c AudioDecoder has started rendering but audio is not being sent to the device, \c false otherwise
	 */
	inline bool IsPaused() const					{ return (PlayerState::Paused == GetPlayerState()); }

	/*!
	 * @brief Determine if the player is pending
	 * @return \c true if an \c AudioDecoder has started decoding but not yet started rendering, \c false otherwise
	 */
	inline bool IsPending() const					{ return (PlayerState::Pending == GetPlayerState()); }

	/*!
	 * @brief Determine if the player is stopped
	 * @return \c true if an \c AudioDecoder has not started decoding or the decoder queue is empty, \c false otherwise
	 */
	inline bool IsStopped() const					{ return (PlayerState::Stopped == GetPlayerState()); }

	//@}


	// ========================================
	/*! @name Information on the decoder that is currently rendering */
	//@{

	/*! @brief Get the URL of the \c AudioDecoder that is currently rendering, or \c nullptr if none */
	CFURLRef GetPlayingURL() const;

	/*! @brief Get the represented object belonging to the \c AudioDecoder that is currently rendering, or \c nullptr if none */
	void * GetPlayingRepresentedObject() const;

	//@}


	// ========================================
	/*! @name Block-based callback support */
	//@{

	/*!
	 * @brief Set the block to be invoked when an \c AudioDecoder starts decoding
	 * @note The block is invoked from the decoding thread before the first audio frame is decoded
	 * @param block The block to invoke when decoding starts
	 */
	void SetDecodingStartedBlock(AudioPlayerDecoderEventBlock block);

	/*!
	 * @brief Set the block to be invoked when an \c AudioDecoder finishes decoding
	 * @note The block is invoked from the decoding thread after the last audio frame has been decoded
	 * @param block The block to invoke when decoding finishes
	 */
	void SetDecodingFinishedBlock(AudioPlayerDecoderEventBlock block);

	/*!
	 * @brief Set the block to be invoked when an \c AudioDecoder starts rendering
	 * @note This block is invoked from the real-time rendering thread before the first audio frame is rendered
	 * @param block The block to invoke when rendering starts
	 */
	void SetRenderingStartedBlock(AudioPlayerDecoderEventBlock block);

	/*!
	 * @brief Set the block to be invoked when an \c AudioDecoder finishes rendering
	 * @note This block is invoked from the real-time rendering thread after the last audio frame has been rendered
	 * @param block The block to invoke when rendering finishes
	 */
	void SetRenderingFinishedBlock(AudioPlayerDecoderEventBlock block);


	/*!
	 * @brief Set the block to be invoked before the player renders audio
	 * @note This block is invoked from the real-time rendering thread before audio is rendered
	 * @param block The block to invoke before audio rendering
	 */
	void SetPreRenderBlock(AudioPlayerRenderEventBlock block);

	/*!
	 * @brief Set the block to be invoked after the player renders audio
	 * @note This block is invoked from the real-time rendering thread after audio is rendered
	 * @param block The block to invoke after audio rendering
	 */
	void SetPostRenderBlock(AudioPlayerRenderEventBlock block);

	/*!
	 * @brief Set the block to be invoked when the player's sample rate or channel count will change
	 * @note This block is invoked from the decoding thread
	 * @param block The block to invoke when a format mismatch occurs
	 */
	void SetFormatMismatchBlock(AudioPlayerFormatMismatchBlock block);

	//@}

	// ========================================
	/*!
	 * @name Playback Properties
	 * These methods return \c true on success, \c false otherwise.
	 */
	//@{

	/*! @brief Get the current frame of the active \c AudioDecoder */
	bool GetCurrentFrame(SInt64& currentFrame) const;

	/*! @brief Get the total frames of the active \c AudioDecoder */
	bool GetTotalFrames(SInt64& totalFrames) const;

	/*! @brief Get the playback position of the active \c AudioDecoder */
	bool GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const;


	/*! @brief Get the current time of the active \c AudioDecoder */
	bool GetCurrentTime(CFTimeInterval& currentTime) const;

	/*! @brief Get the total time of the active \c AudioDecoder */
	bool GetTotalTime(CFTimeInterval& totalTime) const;

	/*! @brief Get the playback time of the active \c AudioDecoder */
	bool GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;


	/*! @brief Get the playback position and time of the active \c AudioDecoder */
	bool GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;

	//@}


	// ========================================
	/*!
	 * @name Seeking
	 * The \c Seek() methods return \c true on success, \c false otherwise.
	 */
	//@{

	/*! @brief Seek forward in the active \c AudioDecoder by the specified number of seconds */
	bool SeekForward(CFTimeInterval secondsToSkip = 3);

	/*! @brief Seek backward in the active \c AudioDecoder by the specified number of seconds */
	bool SeekBackward(CFTimeInterval secondsToSkip = 3);

	/*! @brief Seek to the specified time in the active \c AudioDecoder */
	bool SeekToTime(CFTimeInterval timeInSeconds);

	/*! @brief Seek to the specified frame in the active \c AudioDecoder */
	bool SeekToFrame(SInt64 frame);
	
	/*! @brief Determine whether the active \c AudioDecoder supports seeking */
	bool SupportsSeeking() const;

	//@}


	// ========================================
	/*! @name Player Parameters */
	//@{

	/*!
	 * @brief Get the player volume
	 *
	 * This corresponds to the property \c kHALOutputParam_Volume on element \c 0
	 * @note The volume is linear across the interval [0, 1]
	 * @param volume A \c Float32 to receive the volume
	 * @return \c true on success, \c false otherwise
	 */
	bool GetVolume(Float32& volume) const;

	/*!
	 * @brief Set the player volume
	 *
	 * This corresponds to the property \c kHALOutputParam_Volume on element \c 0
	 * @note The volume is linear and the value will be clamped to the interval [0, 1]
	 * @param volume The desired volume
	 * @return \c true on success, \c false otherwise
	 */
	bool SetVolume(Float32 volume);


	/*!
	 * @brief Get the volume for the specified channel
	 *
	 * This corresponds to the property \c kHALOutputParam_Volume on element \c channel
	 * @note The volume is linear across the interval [0, 1]
	 * @param channel The desired channel
	 * @param volume A \c Float32 to receive the channel's volume
	 * @return \c true on success, \c false otherwise
	 */
	bool GetVolumeForChannel(UInt32 channel, Float32& volume) const;

	/*!
	 * @brief Set the volume for the specified channel
	 *
	 * This corresponds to the property \c kHALOutputParam_Volume on element \c channel
	 * @note The volume is linear and the value will be clamped to the interval [0, 1]
	 * @param channel The desired channel
	 * @param volume The desired volume
	 * @return \c true on success, \c false otherwise
	 */
	bool SetVolumeForChannel(UInt32 channel, Float32 volume);


	/*!
	 * @brief Get the player pre-gain
	 *
	 * This corresponds to the property \c kMultiChannelMixerParam_Volume
	 * @note Pre-gain is linear across the interval [0, 1]
	 * @param preGain A \c Float32 to receive the pre-gain
	 * @return \c true on success, \c false otherwise
	 */
	bool GetPreGain(Float32& preGain) const;

	/*!
	 * @brief Set the player pre-gain
	 *
	 * This corresponds to the property \c kMultiChannelMixerParam_Volume
	 * @note The pre-gain is linear and the value will be clamped to the interval [0, 1]
	 * @param preGain The desired pre-gain
	 * @return \c true on success, \c false otherwise
	 */
	bool SetPreGain(Float32 preGain);


	/*! @brief Query whether the player is performing sample rate conversion */
	bool IsPerformingSampleRateConversion() const;

	/*!
	 * @brief Get the sample rate converter's complexity
	 *
	 * This corresponds to the property \c kAudioUnitProperty_SampleRateConverterComplexity
	 * @param complexity A \c UInt32 to receive the SRC complexity
	 * @return \c true on success, \c false otherwise
	 * @see kAudioUnitProperty_SampleRateConverterComplexity
	 */
	bool GetSampleRateConverterComplexity(UInt32& complexity) const;

	/*!
	 * @brief Set the sample rate converter's complexity
	 *
	 * This corresponds to the property \c kAudioUnitProperty_SampleRateConverterComplexity
	 * @param complexity The desired SRC complexity
	 * @return \c true on success, \c false otherwise
	 * @see kAudioUnitProperty_SampleRateConverterComplexity
	 */
	bool SetSampleRateConverterComplexity(UInt32 complexity);

	//@}


	// ========================================
	/*! @name DSP Effects */
	//@{

	/*!
	 * @brief Add a DSP effect to the audio processing graph
	 * @param subType The \c AudioComponent subtype
	 * @param manufacturer The \c AudioComponent manufacturer
	 * @param flags The \c AudioComponent flags
	 * @param mask The \c AudioComponent mask
	 * @param effectUnit An optional pointer to an \c AudioUnit to receive the effect
	 * @return \c true on success, \c false otherwise
	 * @see AudioComponentDescription
	 */
	bool AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit = nullptr);

	/*!
	 * @brief Remove the specified DSP effect
	 * @param effectUnit The \c AudioUnit to remove from the processing graph
	 * @return \c true on success, \c false otherwise
	 */
	bool RemoveEffect(AudioUnit effectUnit);

	//@}


#if !TARGET_OS_IPHONE
	// ========================================
	/*! @name Hog Mode */
	//@{

	/*! @brief Query whether the output device hogged */
	bool OutputDeviceIsHogged() const;

	/*! 
	 * @brief Start hogging the output device
	 *
	 * This will attempt to set the property \c kAudioDevicePropertyHogMode
	 * @return \c true on success, \c false otherwise
	 */
	bool StartHoggingOutputDevice();

	/*!
	 * @brief Stop hogging the output device
	 *
	 * This will attempt to clear the property \c kAudioDevicePropertyHogMode
	 * @return \c true on success, \c false otherwise
	 */
	bool StopHoggingOutputDevice();

	//@}

	// ========================================
	/*! @name Device parameters */
	//@{

	/*!
	 * @brief Get the device's master volume
	 *
	 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c kAudioObjectPropertyElementMaster
	 * @param volume A \c Float32 to receive the master volume
	 * @return \c true on success, \c false otherwise
	 */
	bool GetDeviceMasterVolume(Float32& volume) const;

	/*!
	 * @brief Set the device's master volume
	 *
	 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c kAudioObjectPropertyElementMaster
	 * @param volume The desired master volume
	 * @return \c true on success, \c false otherwise
	 * @see kAudioDevicePropertyVolumeScalar
	 */
	bool SetDeviceMasterVolume(Float32 volume);


	/*!
	 * @brief Get the device's volume for the specified channel
	 *
	 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c channel
	 * @param channel The desired channel
	 * @param volume A \c Float32 to receive the volume
	 * @return \c true on success, \c false otherwise
	 */
	bool GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const;

	/*!
	 * @brief Set the device's volume for the specified channel
	 *
	 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c channel
	 * @param channel The desired channel
	 * @param volume The desired volume
	 * @return \c true on success, \c false otherwise
	 */
	bool SetDeviceVolumeForChannel(UInt32 channel, Float32 volume);


	/*!
	 * @brief Get the number of output channels on the device
	 * @param channelCount A \c UInt32 to receive the channel count
	 * @return \c true on success, \c false otherwise
	 */
	bool GetDeviceChannelCount(UInt32& channelCount) const;

	/*!
	 * @brief Get the device's preferred stereo channel
	 * @param preferredStereoChannels A \c std::pair to receive the channels
	 * @return \c true on success, \c false otherwise
	 */
	bool GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const;

	//@}


	// ========================================
	/*! @name Device Management */
	//@{

	/*!
	 * @brief Create the UID of the output device
	 * @note The returned string must be released by the caller
	 * @param deviceUID A \c CFStringRef to receive the UID
	 * @return \c true on success, \c false otherwise
	 * @see GetOutputDeviceID()
	 */
	bool CreateOutputDeviceUID(CFStringRef& deviceUID) const;

	/*!
	 * @brief Set the output device to the device matching the provided UID
	 * @param deviceUID The UID of the desired device
	 * @return \c true on success, \c false otherwise
	 * @see SetOutputDeviceID()
	 */
	bool SetOutputDeviceUID(CFStringRef deviceUID);


	/*!
	 * @brief Get the device ID of the output device
	 * @param deviceID An \c AudioDeviceID to receive the device ID
	 * @return \c true on success, \c false otherwise
	 * @see CreateOutputDeviceUID()
	 */
	bool GetOutputDeviceID(AudioDeviceID& deviceID) const;

	/*!
	 * @brief Set the output device to the device matching the provided ID
	 * @param deviceID The ID of the desired device
	 * @return \c true on success, \c false otherwise
	 * @see SetOutputDeviceUID()
	 */
	bool SetOutputDeviceID(AudioDeviceID deviceID);


	/*!
	 * @brief Get the sample rate of the output device
	 * @param sampleRate A \c Float64 to receive the sample rate
	 * @return \c true on success, \c false otherwise
	 */
	bool GetOutputDeviceSampleRate(Float64& sampleRate) const;

	/*!
	 * @brief Set the sample rate of the output device
	 * @param sampleRate The desired sample rate
	 * @return \c true on success, \c false otherwise
	 */
	bool SetOutputDeviceSampleRate(Float64 sampleRate);

	//@}

#endif


	// ========================================
	/*! @name Playlist Management */
	//@{

	/*!
	 * @brief Enqueue a URL for playback
	 * @param url The URL of the location to enqueue
	 * @return \c true on success, \c false otherwise
	 */
	bool Enqueue(CFURLRef url);

	/*!
	 * @brief Enqueue an \c AudioDecoder for playback
	 * @note The player will take ownership of the decoder on success
	 * @param decoder The \c AudioDecoder to enqueue
	 * @return \c true on success, \c false otherwise
	 */
	bool Enqueue(AudioDecoder *decoder);


	/*!
	 * @brief Skip to the next enqueued decoder
	 * @return \c true on success, \c false otherwise
	 */
	bool SkipToNextTrack();


	/*!
	 * @brief Clear all queued decoders
	 * @return \c true on success, \c false otherwise
	 */
	bool ClearQueuedDecoders();

	//@}


	// ========================================
	/*! @name Ring Buffer Parameters */
	//@{

	/*! @brief Get the capacity, in frames, of the player's internal ring buffer */
	inline uint32_t GetRingBufferCapacity() const	{ return mRingBufferCapacity; }

	/*! 
	 * @brief Set the capacity of the player's internal ring buffer
	 * @param bufferCapacity The desired capacity, in frames, of the player's internal ring buffer
	 * @return \c true on success, \c false otherwise
	 */
	bool SetRingBufferCapacity(uint32_t bufferCapacity);


	/*!
	 * @brief Get the minimum size of writes to the player's internal ring buffer
	 * @note This relates to the minimum read size from an \c AudioDecoder, but may not equal the
	 * minimum read size because of sample rate conversion
	 * @return The minimum size, in frames, of writes to the player's internal ring buffer
	 */
	inline uint32_t GetRingBufferWriteChunkSize() const	{ return mRingBufferWriteChunkSize; }

	/*!
	 * @brief Set the minimum size of writes to the player's internal ring buffer
	 * @param chunkSize The desired minimum size, in frames, of writes to the player's internal ring buffer
	 * @return \c true on success, \c false otherwise
	 */
	bool SetRingBufferWriteChunkSize(uint32_t chunkSize);

	//@}


	/*! @cond */

	/*! @internal This class is exposed so it can be used inside C callbacks */
	class DecoderStateData;

	/*! @endcond */

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
	/*! @cond */

	/*! @internal AUGraph render callback */
	OSStatus Render(AudioUnitRenderActionFlags		*ioActionFlags,
					const AudioTimeStamp			*inTimeStamp,
					UInt32							inBusNumber,
					UInt32							inNumberFrames,
					AudioBufferList					*ioData);

	/*! @internal AUGraph postrender callback */
	OSStatus DidRender(AudioUnitRenderActionFlags		*ioActionFlags,
					   const AudioTimeStamp				*inTimeStamp,
					   UInt32							inBusNumber,
					   UInt32							inNumberFrames,
					   AudioBufferList					*ioData);
	
	// ========================================

	/*! @internal Decoder thread entry point */
	void * DecoderThreadEntry();

	/*! @internal Collector thread entry point */
	void * CollectorThreadEntry();

	/*! @endcond */
};
