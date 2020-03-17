/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <dispatch/dispatch.h>

#include <CoreAudio/CoreAudioTypes.h>

#include "AudioChannelLayout.h"
#include "AudioDecoder.h"
#include "AudioOutput.h"
#include "AudioRingBuffer.h"
#include "Semaphore.h"

/*! @file AudioPlayer.h @brief Audio playback functionality */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief The audio player class
		 *
		 * A player decodes audio into a ring buffer and passes it to the selected Output on demand.
		 * The player supports seeking and playback control for all Decoder subclasses supported by the current Output.
		 *
		 * Decoding occurs in a high priority (non-realtime) thread which reads audio via a Decoder instance and stores it in the ring buffer.
		 * For the common case using CoreAudioOutput the audio is stored in the canonical Core Audio format (kAudioFormatFlagsAudioUnitCanonical)-
		 * deinterleaved, normalized [-1, 1) native floating point data in 32 bits (AKA floats) on Mac OS X and 8.24 fixed point on iOS.
		 * For ASIOOutput (on exaSound devices) the audio may be stored in either DSD or PCM format.
		 *
		 * Rendering occurs in a realtime thread when ProvideAudio() is called by the output.
		 *
		 * Since decoding and rendering are distinct operations performed in separate threads, a GCD timer on the background queue is
		 * used for garbage collection.  This is necessary because state data created in the decoding thread needs to live until
		 * rendering is complete, which cannot occur until after decoding is complete.
		 *
		 * The player supports block-based callbacks for the following events:
		 *  1. Decoding started
		 *  2. Decoding finished
		 *  3. Rendering started
		 *  4. Rendering finished
		 *  5. Pre- and post- audio rendering
		 *  6. %Audio format mismatches preventing gapless playback
		 *
		 * The decoding callbacks will be performed from the decoding thread.  Although not a real time thread,
		 * lengthy operations should be avoided to prevent audio glitching resulting from gaps in the ring buffer.
		 *
		 * The rendering callbacks will be performed from the realtime rendering thread.  Execution of this thread must not be blocked!
		 * Examples of prohibited actions that could cause problems:
		 *  - Memory allocation
		 *  - Objective-C messaging
		 *  - File IO
		 */
		class Player {

			/*! @brief The length of the array containing active audio decoders */
			static const size_t kActiveDecoderArraySize = 8;

		public:
			// ========================================
			/*! @name Block callback types */
			//@{

			/*!
			 * @brief A block called when an event occurs on a \c Decoder
			 * @param decoder The \c AudioDecoder on which the event occurred
			 */
			using DecoderEventBlock = void (^)(const Decoder& decoder);

			/*!
			 * @brief A block called when an error occurs on a \c Decoder
			 * @param decoder The \c AudioDecoder on which the error occurred
			 * @param error An optional description of the error
			 */
			using DecoderErrorBlock = void (^)(const Decoder& decoder, CFErrorRef error);

			/*!
			 * @brief A block called when a \c Player render event occurs
			 * @param data The audio data
			 * @param frameCount The number of frames in \c data
			 */
			using RenderEventBlock = void (^)(AudioBufferList *data, UInt32 frameCount);

			/*!
			 * @brief A block called when the audio format of the next \c AudioDecoder does not match the current format
			 * @param currentFormat The current audio format
			 * @param nextFormat The next audio format
			 */
			using FormatMismatchBlock = void (^)(const AudioFormat& currentFormat, const AudioFormat& nextFormat);

			/*!
			 * @brief A block called when an error occurs
			 * @param error An optional description of the error
			 */
			using ErrorBlock = void (^)(CFErrorRef error);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief A \c std::unique_ptr for \c Player objects */
			using unique_ptr = std::unique_ptr<Player>;

			/*!
			 * @brief Create a new \c Player for the default CoreAudioOutput device
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
			void SetDecodingStartedBlock(DecoderEventBlock block);

			/*!
			 * @brief Set the block to be invoked when a \c Decoder finishes decoding
			 * @note The block is invoked from the decoding thread after the last audio frame has been decoded
			 * @param block The block to invoke when decoding finishes
			 */
			void SetDecodingFinishedBlock(DecoderEventBlock block);

			/*!
			 * @brief Set the block to be invoked when a \c Decoder starts rendering
			 * @note This block is invoked from the real-time rendering thread before the first audio frame is rendered
			 * @param block The block to invoke when rendering starts
			 */
			void SetRenderingStartedBlock(DecoderEventBlock block);

			/*!
			 * @brief Set the block to be invoked when a \c Decoder finishes rendering
			 * @note This block is invoked from the real-time rendering thread after the last audio frame has been rendered
			 * @param block The block to invoke when rendering finishes
			 */
			void SetRenderingFinishedBlock(DecoderEventBlock block);

			/*!
			 * @brief Set the block to be invoked when a queued \c Decoder fails to open
			 * @note The block is invoked from the decoding thread
			 * @param block The block to invoke if an error occurs calling Decoder::Open
			 */
			void SetOpenDecoderErrorBlock(DecoderErrorBlock block);


			/*!
			 * @brief Set the block to be invoked before the player renders audio
			 * @note This block is invoked from the real-time rendering thread before audio is rendered
			 * @param block The block to invoke before audio rendering
			 */
			void SetPreRenderBlock(RenderEventBlock block);

			/*!
			 * @brief Set the block to be invoked after the player renders audio
			 * @note This block is invoked from the real-time rendering thread after audio is rendered
			 * @param block The block to invoke after audio rendering
			 */
			void SetPostRenderBlock(RenderEventBlock block);


			/*!
			 * @brief Set the block to be invoked when the player's sample rate or channel count will change
			 * @note This block is invoked from the decoding thread
			 * @param block The block to invoke when a format mismatch occurs
			 */
			void SetFormatMismatchBlock(FormatMismatchBlock block);

			/*!
			 * @brief Set the block to be invoked when a \c Decoder with an unsupported format is encountered
			 * @note The block is invoked from the decoding thread
			 * @param block The block to invoke when an error occurs
			 */
			void SetUnsupportedFormatBlock(ErrorBlock block);

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

			/*! @brief Seek to the specified position in the active \c Decoder */
			bool SeekToPosition(float position);

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
			/*! @name Output Management */
			//@{

			/*! @brief Get the Output used by this Player */
			Output& GetOutput() const;

			/*!
			 * @brief Set the Output used by this player
			 * @note The player will take ownership of the output on success and may take ownership on failure
			 * @param output The desired output
			 * @return \c true on success, \c false otherwise
			 */
			bool SetOutput(Output::unique_ptr output);

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

			/*!
			 * @internal
			 * @brief Copy decoded audio into the specified buffer
			 * @param bufferList A buffer to receive the decoded audio
			 * @param frameCount The requested number of audio frames
			 * @return \c true on success, \c false otherwise
			 */
			bool ProvideAudio(AudioBufferList *bufferList, UInt32 frameCount);

			/*! @endcond */

		private:

			// ========================================
			// Thread entry point
			void * DecoderThreadEntry();

			// ========================================
			// Other Utilities
			void StopActiveDecoders();

			DecoderStateData * GetCurrentDecoderState() const;
			DecoderStateData * GetDecoderStateStartingAfterTimeStamp(SInt64 timeStamp) const;

			bool SetupOutputAndRingBufferForDecoder(Decoder& decoder);

			// ========================================
			// Data Members
			RingBuffer::unique_ptr					mRingBuffer;
			std::atomic_uint						mRingBufferCapacity;
			std::atomic_uint						mRingBufferWriteChunkSize;

			std::atomic_uint						mFlags;

			std::vector<Decoder::unique_ptr>		mDecoderQueue;
			std::atomic<DecoderStateData *>			mActiveDecoders [kActiveDecoderArraySize];

			dispatch_queue_t						mQueue;
			Semaphore								mSemaphore;

			std::thread								mDecoderThread;
			Semaphore								mDecoderSemaphore;

			dispatch_source_t						mCollector;

			std::atomic_llong						mFramesDecoded;
			std::atomic_llong						mFramesRendered;

			Output::unique_ptr						mOutput;

			// ========================================
			// Callbacks
			DecoderEventBlock						mDecoderEventBlocks [4];
			DecoderErrorBlock						mDecoderErrorBlock;
			RenderEventBlock						mRenderEventBlocks [2];
			FormatMismatchBlock						mFormatMismatchBlock;
			ErrorBlock								mErrorBlock;
		};

	}
}
