//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <array>
#import <atomic>
#import <memory>
#import <queue>

#import <os/log.h>

#import <AVFAudio/AVFAudio.h>

#import <SFBAudioRingBuffer.hpp>
#import <SFBDispatchSemaphore.hpp>
#import <SFBRingBuffer.hpp>
#import <SFBUnfairLock.hpp>

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayerNode.h"

namespace SFB {

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept;

#pragma mark - AudioPlayerNode

/// SFBAudioPlayerNode implementation
struct AudioPlayerNode final {
	using unique_ptr = std::unique_ptr<AudioPlayerNode>;
	using DecoderQueue = std::deque<id <SFBPCMDecoding>>;

	struct DecoderState;

	/// The shared log for all `AudioPlayerNode` instances
	static const os_log_t sLog;

	/// Unsafe reference to owning `SFBAudioPlayerNode` instance
	__unsafe_unretained SFBAudioPlayerNode *mNode 			= nil;

	/// The render block supplying audio
	AVAudioSourceNodeRenderBlock 	mRenderBlock 			= nullptr;

private:
	static constexpr size_t kDecoderStateArraySize = 8;
	using DecoderStateArray = std::array<std::atomic<DecoderState *>, kDecoderStateArraySize>;

	/// The format of the audio supplied by `mRenderBlock`
	AVAudioFormat 					*mRenderingFormat		= nil;

	/// Ring buffer used to transfer audio between the decoding dispatch queue and the render block
	SFB::AudioRingBuffer			mAudioRingBuffer 		= {};

	/// Active decoders and associated state
	DecoderStateArray 				*mActiveDecoders 		= nullptr;

	/// Decoders enqueued for playback that are not yet active
	DecoderQueue 					mQueuedDecoders 		= {};
	/// Lock used to protect access to `mQueuedDecoders`
	mutable SFB::UnfairLock			mQueueLock;

	/// Dispatch queue used for decoding
	dispatch_queue_t				mDecodingQueue 			= nullptr;
	/// Dispatch semaphore used for communication with the decoding queue
	SFB::DispatchSemaphore			mDecodingSemaphore 		{0};
	/// Dispatch group used to track decoding tasks
	dispatch_group_t 				mDecodingGroup			= nullptr;

	/// Ring buffer used to communicate events from the decoding queue
	SFB::RingBuffer					mDecodeEventRingBuffer;
	/// Ring buffer used to communicate events from the render block
	SFB::RingBuffer					mRenderEventRingBuffer;

public:
	/// Dispatch queue used for event processing and delegate messaging
	dispatch_queue_t				mEventProcessingQueue	= nullptr;
private:
	/// Dispatch source initiating event processing by the render block
	dispatch_source_t				mEventProcessingSource 	= nullptr;
	/// Dispatch group used to track event processing initiated by the decoding queue
	dispatch_group_t 				mEventProcessingGroup	= nullptr;

	/// AudioPlayerNode flags
	std::atomic_uint 				mFlags 					= 0;
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

	/// Counter used for unique keys to `dispatch_queue_set_specific`
	std::atomic_uint64_t 			mDispatchKeyCounter 	= 1;

	enum AudioPlayerNodeFlags : unsigned int {
		eFlagIsPlaying 				= 1u << 0,
		eFlagIsMuted 				= 1u << 1,
		eFlagMuteRequested 			= 1u << 2,
		eFlagRingBufferNeedsReset 	= 1u << 3,
	};

public:
	AudioPlayerNode(AVAudioFormat * _Nonnull format, uint32_t ringBufferSize);
	~AudioPlayerNode();

	AudioPlayerNode(const AudioPlayerNode&) = delete;
	AudioPlayerNode& operator=(const AudioPlayerNode&) = delete;
	AudioPlayerNode(const AudioPlayerNode&&) = delete;
	AudioPlayerNode& operator=(const AudioPlayerNode&&) = delete;

#pragma mark - Playback Control

	void Play() noexcept
	{
		mFlags.fetch_or(eFlagIsPlaying, std::memory_order_acq_rel);
	}

	void Pause() noexcept
	{
		mFlags.fetch_and(~eFlagIsPlaying, std::memory_order_acq_rel);
	}

	void Stop() noexcept
	{
		mFlags.fetch_and(~eFlagIsPlaying, std::memory_order_acq_rel);
		Reset();
	}

	void TogglePlayPause() noexcept
	{
		mFlags.fetch_xor(eFlagIsPlaying, std::memory_order_acq_rel);
	}

#pragma mark - Playback State

	bool IsPlaying() const noexcept
	{
		return mFlags.load(std::memory_order_acquire) & eFlagIsPlaying;
	}

	bool IsReady() const noexcept
	{
		return GetActiveDecoderStateWithSmallestSequenceNumber() != nullptr;
	}

#pragma mark - Playback Properties

	SFBAudioPlayerNodePlaybackPosition PlaybackPosition() const noexcept;
	SFBAudioPlayerNodePlaybackTime PlaybackTime() const noexcept;
	bool GetPlaybackPositionAndTime(SFBAudioPlayerNodePlaybackPosition * _Nullable playbackPosition, SFBAudioPlayerNodePlaybackTime * _Nullable playbackTime) const noexcept;

#pragma mark - Seeking

	bool SeekForward(NSTimeInterval secondsToSkip) noexcept;
	bool SeekBackward(NSTimeInterval secondsToSkip) noexcept;
	bool SeekToTime(NSTimeInterval timeInSeconds) noexcept;
	bool SeekToPosition(double position) noexcept;
	bool SeekToFrame(AVAudioFramePosition frame) noexcept;
	bool SupportsSeeking() const noexcept;

#pragma mark - Format Information

	AVAudioFormat * _Nonnull RenderingFormat() const noexcept
	{
		return mRenderingFormat;
	}

	bool SupportsFormat(AVAudioFormat * _Nonnull format) const noexcept;

#pragma mark - Decoder Queue Management

	bool EnqueueDecoder(id <SFBPCMDecoding> _Nonnull decoder, bool reset, NSError **error) noexcept;

private:
	id <SFBPCMDecoding> _Nullable DequeueDecoder() noexcept;

public:
	id<SFBPCMDecoding> _Nullable CurrentDecoder() const noexcept;
	void CancelActiveDecoders() noexcept;

	void ClearQueue() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		mQueuedDecoders.resize(0);
	}

	bool QueueIsEmpty() const noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		return mQueuedDecoders.empty();
	}

	void Reset() noexcept
	{
		ClearQueue();
		CancelActiveDecoders();
	}

private:

#pragma mark - Decoding

	void DequeueAndProcessDecoder(bool unmuteNeeded) noexcept;

#pragma mark - Rendering

	OSStatus Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) noexcept;

#pragma mark - Events

	/// An event header consisting of an event command and event identification number
	template <typename T, typename = std::enable_if_t<std::is_same_v<std::underlying_type_t<T>, uint32_t>>>
	struct EventHeader {
		/// The event command
		T mCommand;
		/// The event identification number
		uint64_t mIdentificationNumber;

		/// Constructs an empty event header
		EventHeader() noexcept = default;

		/// Constructs an event header with the next available identification number
		/// - parameter command: The command for the event
		EventHeader(T command) noexcept
		: mCommand{command}, mIdentificationNumber{NextEventIdentificationNumber()}
		{}
	};

#pragma mark Decoding Events

	/// Decoding queue events
	enum class DecodingEventCommand : uint32_t {
		eStarted 	= 1,
		eComplete 	= 2,
		eCanceled 	= 3,
		eError 		= 4,
	};

	/// A decoding event header
	using DecodingEventHeader = EventHeader<DecodingEventCommand>;

#pragma mark Rendering Events

	/// Render block events
	enum class RenderingEventCommand : uint32_t {
		eStarted 		= 1,
		eDecoderChanged = 2,
		eComplete 		= 3,
	};

	/// A rendering event command and identification number
	using RenderingEventHeader = EventHeader<RenderingEventCommand>;

#pragma mark - Event Processing

	void ProcessPendingEvents() noexcept;
	void ProcessEvent(const DecodingEventHeader& header) noexcept;
	void ProcessEvent(const RenderingEventHeader& header) noexcept;

#pragma mark - Decoder State Array

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number that has not completed rendering
	DecoderState * _Nullable GetActiveDecoderStateWithSmallestSequenceNumber() const noexcept;

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number greater than `sequenceNumber` that has not completed rendering
	DecoderState * _Nullable GetActiveDecoderStateFollowingSequenceNumber(const uint64_t& sequenceNumber) const noexcept;

	/// Returns the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	DecoderState * _Nullable GetDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) const noexcept;

	/// Deletes the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	void DeleteDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) noexcept;

};

} /* namespace SFB */
