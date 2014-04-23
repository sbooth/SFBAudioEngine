/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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

#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <map>
#include <utility>

#include "AudioOutput.h"
#include "AudioDecoder.h"
#include "AudioRingBuffer.h"
#include "AudioChannelLayout.h"
#include "Semaphore.h"
#include "RingBuffer.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// Constants
		// ========================================
		/*! @brief The length of the array containing active audio decoders */
#define kActiveDecoderArraySize 8

		namespace ASIO {

			/*! @brief An audio player for ASIO interfaces */
			class Player {

				// For access to mMutex
				// FIXME: Is this a good idea?
				friend class SFB::Audio::Output;

			public:
				// ========================================
				/*! @name Block callback types */
				//@{

				/*!
				 * @brief A block called when an event occurs on a \c Decoder
				 * @param decoder The \c AudioDecoder on which the event occurred
				 */
				typedef void (^AudioPlayerDecoderEventBlock)(const Decoder& decoder);

				/*!
				 * @brief A block called when a \c Player render event occurs
				 * @param data The audio data
				 * @param frameCount The number of frames in \c data
				 */
				typedef void (^AudioPlayerRenderEventBlock)(AudioBufferList *data, UInt32 frameCount);

				/*!
				 * @brief A block called when the audio format of the next \c AudioDecoder does not match the current format
				 * @param currentFormat The current audio format
				 * @param nextFormat The next audio format
				 */
				typedef void (^AudioPlayerFormatMismatchBlock)(const AudioFormat& currentFormat, const AudioFormat& nextFormat);

				//@}


				// ========================================
				/*! @name Creation and Destruction */
				//@{

				// ASIO only supports a single driver at a time
				/*!
				 * @brief Use this instead of creating an instance directly
				 */
				static Player * GetInstance();

				/*!
				 * @brief Create a new \c Player for the default output device
				 * @throws std::bad_alloc
				 * @throws std::runtime_error
				 */
				Player();

				/*! @brief Destroy the \c Player and release all associated resources. */
				~Player();

				/*! @cond */

				/*! @internal This class is non-copyable */
				Player(const Player& rhs) = delete;

				/*! @internal This class is non-assignable */
				Player& operator=(const Player& rhs) = delete;

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
					Paused,		/*!< a \c Decoder has started rendering, but audio is not being sent to the output device */
					Pending,	/*!< a \c Decoder has started decoding, but not yet started rendering */
					Stopped		/*!< a \c Decoder has not started decoding, or the decoder queue is empty */
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
				 * @return \c true if a \c Decoder has started rendering but audio is not being sent to the device, \c false otherwise
				 */
				inline bool IsPaused() const					{ return (PlayerState::Paused == GetPlayerState()); }

				/*!
				 * @brief Determine if the player is pending
				 * @return \c true if a \c Decoder has started decoding but not yet started rendering, \c false otherwise
				 */
				inline bool IsPending() const					{ return (PlayerState::Pending == GetPlayerState()); }

				/*!
				 * @brief Determine if the player is stopped
				 * @return \c true if a \c Decoder has not started decoding or the decoder queue is empty, \c false otherwise
				 */
				inline bool IsStopped() const					{ return (PlayerState::Stopped == GetPlayerState()); }

				//@}


				// ========================================
				/*! @name Information on the decoder that is currently rendering */
				//@{

				/*! @brief Get the URL of the \c AudioDecoder that is currently rendering, or \c nullptr if none */
				CFURLRef GetPlayingURL() const;

				/*! @brief Get the represented object belonging to the \c Decoder that is currently rendering, or \c nullptr if none */
				void * GetPlayingRepresentedObject() const;

				//@}


				// ========================================
				/*! @name Block-based callback support */
				//@{

				/*!
				 * @brief Set the block to be invoked when a \c Decoder starts decoding
				 * @note The block is invoked from the decoding thread before the first audio frame is decoded
				 * @param block The block to invoke when decoding starts
				 */
				void SetDecodingStartedBlock(AudioPlayerDecoderEventBlock block);

				/*!
				 * @brief Set the block to be invoked when a \c Decoder finishes decoding
				 * @note The block is invoked from the decoding thread after the last audio frame has been decoded
				 * @param block The block to invoke when decoding finishes
				 */
				void SetDecodingFinishedBlock(AudioPlayerDecoderEventBlock block);

				/*!
				 * @brief Set the block to be invoked when a \c Decoder starts rendering
				 * @note This block is invoked from the real-time rendering thread before the first audio frame is rendered
				 * @param block The block to invoke when rendering starts
				 */
				void SetRenderingStartedBlock(AudioPlayerDecoderEventBlock block);

				/*!
				 * @brief Set the block to be invoked when a \c Decoder finishes rendering
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

				/*! @brief Get the current frame of the active \c Decoder */
				bool GetCurrentFrame(SInt64& currentFrame) const;

				/*! @brief Get the total frames of the active \c Decoder */
				bool GetTotalFrames(SInt64& totalFrames) const;

				/*! @brief Get the playback position of the active \c Decoder */
				bool GetPlaybackPosition(SInt64& currentFrame, SInt64& totalFrames) const;


				/*! @brief Get the current time of the active \c Decoder */
				bool GetCurrentTime(CFTimeInterval& currentTime) const;

				/*! @brief Get the total time of the active \c Decoder */
				bool GetTotalTime(CFTimeInterval& totalTime) const;

				/*! @brief Get the playback time of the active \c Decoder */
				bool GetPlaybackTime(CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;


				/*! @brief Get the playback position and time of the active \c Decoder */
				bool GetPlaybackPositionAndTime(SInt64& currentFrame, SInt64& totalFrames, CFTimeInterval& currentTime, CFTimeInterval& totalTime) const;

				//@}


				// ========================================
				/*!
				 * @name Seeking
				 * The \c Seek() methods return \c true on success, \c false otherwise.
				 */
				//@{

				/*! @brief Seek forward in the active \c Decoder by the specified number of seconds */
				bool SeekForward(CFTimeInterval secondsToSkip = 3);

				/*! @brief Seek backward in the active \c Decoder by the specified number of seconds */
				bool SeekBackward(CFTimeInterval secondsToSkip = 3);

				/*! @brief Seek to the specified time in the active \c Decoder */
				bool SeekToTime(CFTimeInterval timeInSeconds);

				/*! @brief Seek to the specified frame in the active \c Decoder */
				bool SeekToFrame(SInt64 frame);

				/*! @brief Determine whether the active \c Decoder supports seeking */
				bool SupportsSeeking() const;

				//@}


				// ========================================
				/*! @name Playlist Management */
				//@{

				/*!
				 * @brief Play a URL
				 * @note This will clear any enqueued decoders
				 * @param url The URL to play
				 * @return \c true on success, \c false otherwise
				 */
				bool Play(CFURLRef url);

				/*!
				 * @brief Start playback of a \c Decoder
				 * @note This will clear any enqueued decoders
				 * @note The player will take ownership of the decoder on success and may take ownership on failure
				 * @param decoder The \c Decoder to play
				 * @return \c true on success, \c false otherwise
				 */
				bool Play(Decoder::unique_ptr& decoder);


				/*!
				 * @brief Enqueue a URL for playback
				 * @param url The URL of the location to enqueue
				 * @return \c true on success, \c false otherwise
				 */
				bool Enqueue(CFURLRef url);

				/*!
				 * @brief Enqueue a \c Decoder for playback
				 * @note The player will take ownership of the decoder on success and may take ownership on failure
				 * @param decoder The \c Decoder to enqueue
				 * @return \c true on success, \c false otherwise
				 */
				bool Enqueue(Decoder::unique_ptr& decoder);


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

				/*! @brief Get the audio format  the player's internal ring buffer */
				inline const AudioFormat& GetRingBufferFormat() const { return mRingBufferFormat; }

				/*! @brief Get the channel layout of the player's internal ring buffer */
				inline const ChannelLayout& GetRingBufferChannelLayout() const	{ return mRingBufferChannelLayout; }

				
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
				 * @note This relates to the minimum read size from a \c Decoder, but may not equal the
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

				/*! Get the Output used by this Player */
				Output& GetOutput() const;

				/*!
				 * @brief Copy decoded audio into the specified buffer
				 * @param bufferList A buffer to receive the decoded audio
				 * @param frameCount The requested number of audio frames
				 * @return The actual number of frames read, or \c 0 on error
				 */
				UInt32 ProvideAudio(AudioBufferList *bufferList, UInt32 frameCount);

			private:

				// ========================================
				// Thread entry points
				void * DecoderThreadEntry();
				void * CollectorThreadEntry();

				// ========================================
				// Other Utilities
				void StopActiveDecoders();

				DecoderStateData * GetCurrentDecoderState() const;
				DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const;

				bool SetupOutputAndRingBufferForDecoder(Decoder& decoder);

				// ========================================
				// Data Members
				RingBuffer::unique_ptr					mRingBuffer;
				AudioFormat								mRingBufferFormat;
				ChannelLayout							mRingBufferChannelLayout;
				std::atomic_uint						mRingBufferCapacity;
				std::atomic_uint						mRingBufferWriteChunkSize;

				std::atomic_uint						mFlags;

				std::vector<Decoder::unique_ptr>		mDecoderQueue;
				std::atomic<DecoderStateData *>			mActiveDecoders [kActiveDecoderArraySize];

				std::mutex								mMutex;
				Semaphore								mSemaphore;

				std::thread								mDecoderThread;
				Semaphore								mDecoderSemaphore;
				
				std::thread								mCollectorThread;
				Semaphore								mCollectorSemaphore;
				
				std::atomic_llong						mFramesDecoded;
				std::atomic_llong						mFramesRendered;

				Output::unique_ptr						mOutput;
				
				// ========================================
				// Callbacks
				AudioPlayerDecoderEventBlock			mDecoderEventBlocks [4];
				AudioPlayerRenderEventBlock				mRenderEventBlocks [2];
				AudioPlayerFormatMismatchBlock			mFormatMismatchBlock;
			};

		}
	}
}
