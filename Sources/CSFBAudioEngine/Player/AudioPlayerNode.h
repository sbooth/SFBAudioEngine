//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <atomic>
#import <deque>
#import <memory>
#import <mutex>
#import <stop_token>
#import <thread>
#import <vector>

#import <dispatch/dispatch.h>
#import <os/log.h>

#import <AVFAudio/AVFAudio.h>

#import <CAAudioRingBuffer.hpp>
#import <SFBOSUnfairLock.hpp>
#import <SFBRingBuffer.hpp>

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayerNode.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

namespace SFB {

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept;

// MARK: - AudioPlayerNode

/// SFBAudioPlayerNode implementation
class AudioPlayerNode final {
public:
	using unique_ptr 	= std::unique_ptr<AudioPlayerNode>;
	using Decoder 		= id<SFBPCMDecoding>;

	SFBAudioPlayerNodeDecodingStartedBlock 				mDecodingStartedBlock 				{nil};
	SFBAudioPlayerNodeDecodingCompleteBlock 			mDecodingCompleteBlock 				{nil};
	SFBAudioPlayerNodeRenderingWillStartBlock 			mRenderingWillStartBlock 			{nil};
	SFBAudioPlayerNodeRenderingDecoderWillChangeBlock 	mRenderingDecoderWillChangeBlock 	{nil};
	SFBAudioPlayerNodeRenderingWillCompleteBlock 		mRenderingWillCompleteBlock			{nil};
	SFBAudioPlayerNodeDecoderCanceledBlock 				mDecoderCanceledBlock 				{nil};
	SFBAudioPlayerNodeAsynchronousErrorBlock 			mAsynchronousErrorBlock 			{nil};

	/// The shared log for all `AudioPlayerNode` instances
	static const os_log_t sLog;

	/// Unsafe reference to owning `SFBAudioPlayerNode` instance
	__unsafe_unretained SFBAudioPlayerNode *mNode 			{nil};

	/// The render block supplying audio
	AVAudioSourceNodeRenderBlock 		mRenderBlock 		{nullptr};

private:
	struct DecoderState;

	using DecoderQueue 				= std::deque<Decoder>;
	using DecoderStateVector 		= std::vector<std::unique_ptr<DecoderState>>;

	/// The format of the audio supplied by `mRenderBlock`
	AVAudioFormat 						*mRenderingFormat	{nil};

	/// Ring buffer used to transfer audio between the decoding thread and the render block
	CXXCoreAudio::CAAudioRingBuffer 	mAudioRingBuffer 	{};

	/// Active decoders and associated state
	DecoderStateVector 					mActiveDecoders;
	/// Lock used to protect access to `mActiveDecoders`
	mutable SFB::OSUnfairLock			mDecoderLock;

	/// Decoders enqueued for playback that are not yet active
	DecoderQueue 						mQueuedDecoders 	{};
	/// Lock used to protect access to `mQueuedDecoders`
	mutable SFB::OSUnfairLock			mQueueLock;

	/// Thread used for decoding
	std::jthread 						mDecodingThread;
	/// Dispatch semaphore used for communication with the decoding thread
	dispatch_semaphore_t				mDecodingSemaphore 	{};

	/// Thread used for event processing
	std::jthread 						mEventThread;
	/// Dispatch semaphore used for communication with the event processing thread
	dispatch_semaphore_t				mEventSemaphore 	{};

	/// Ring buffer used to communicate events from the decoding thread
	SFB::RingBuffer						mDecodeEventRingBuffer;
	/// Ring buffer used to communicate events from the render block
	SFB::RingBuffer						mRenderEventRingBuffer;

	/// Flags
	std::atomic_uint 					mFlags 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

public:
	AudioPlayerNode(AVAudioFormat * _Nonnull format, uint32_t ringBufferSize);
	~AudioPlayerNode() noexcept;

	AudioPlayerNode(const AudioPlayerNode&) = delete;
	AudioPlayerNode(AudioPlayerNode&&) = delete;
	AudioPlayerNode& operator=(const AudioPlayerNode&) = delete;
	AudioPlayerNode& operator=(AudioPlayerNode&&) = delete;

	// MARK: - Queue Management

	bool EnqueueDecoder(Decoder _Nonnull decoder, bool reset, NSError * _Nullable * _Nullable error) noexcept;

	/// Pops the next decoder from the decoder queue
	Decoder _Nullable DequeueDecoder() noexcept;

	bool RemoveDecoderFromQueue(Decoder _Nonnull decoder) noexcept;

	void ClearQueue() noexcept
	{
		std::lock_guard lock(mQueueLock);
		mQueuedDecoders.clear();
	}

	bool QueueIsEmpty() const noexcept
	{
		std::lock_guard lock(mQueueLock);
		return mQueuedDecoders.empty();
	}

	Decoder _Nullable CurrentDecoder() const noexcept;
	void CancelActiveDecoders(bool cancelAllActive) noexcept;

	void Reset() noexcept
	{
		ClearQueue();
		CancelActiveDecoders(true);
	}

	// MARK: - Playback Control

	void Play() noexcept
	{
		mFlags.fetch_or(static_cast<unsigned int>(Flags::eIsPlaying), std::memory_order_acq_rel);
	}

	void Pause() noexcept
	{
		mFlags.fetch_and(~static_cast<unsigned int>(Flags::eIsPlaying), std::memory_order_acq_rel);
	}

	void Stop() noexcept
	{
		mFlags.fetch_and(~static_cast<unsigned int>(Flags::eIsPlaying), std::memory_order_acq_rel);
		Reset();
	}

	void TogglePlayPause() noexcept
	{
		mFlags.fetch_xor(static_cast<unsigned int>(Flags::eIsPlaying), std::memory_order_acq_rel);
	}

	// MARK: - Playback State

	bool IsPlaying() const noexcept
	{
		return mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::eIsPlaying);
	}

	bool IsReady() const noexcept
	{
		std::lock_guard lock(mDecoderLock);
		return GetFirstDecoderStateWithRenderingNotComplete() != nullptr;
	}

	// MARK: - Playback Properties

	SFBPlaybackPosition PlaybackPosition() const noexcept;
	SFBPlaybackTime PlaybackTime() const noexcept;
	bool GetPlaybackPositionAndTime(SFBPlaybackPosition * _Nullable playbackPosition, SFBPlaybackTime * _Nullable playbackTime) const noexcept;

	// MARK: - Seeking

	bool SeekForward(NSTimeInterval secondsToSkip) noexcept;
	bool SeekBackward(NSTimeInterval secondsToSkip) noexcept;
	bool SeekToTime(NSTimeInterval timeInSeconds) noexcept;
	bool SeekToPosition(double position) noexcept;
	bool SeekToFrame(AVAudioFramePosition frame) noexcept;
	bool SupportsSeeking() const noexcept;

	// MARK: - Format Information

	AVAudioFormat * _Nonnull RenderingFormat() const noexcept
	{
		return mRenderingFormat;
	}

	bool SupportsFormat(AVAudioFormat * _Nonnull format) const noexcept;

private:
	// MARK: - Flags

	/// Possible bits in `mFlags`
	enum class Flags : unsigned int {
		/// The render block is outputting audio
		eIsPlaying 				= 1u << 0,
		/// The decoding thread requested that the render block set `eIsMuted` during the next render cycle
		eMuteRequested 			= 1u << 1,
		/// The render block is outputting silence
		eIsMuted 				= 1u << 2,
		/// The decoding thread should unmute after the next decoder is dequeued and becomes active
		eUmuteAfterDequeue 		= 1u << 3,
		/// The audio ring buffer requires a non-threadsafe reset
		eRingBufferNeedsReset 	= 1u << 4,
	};

	// MARK: - Decoding

	/// Dequeues and processes decoders from the decoder queue
	/// - note: This is the thread entry point for the decoding thread
	void ProcessDecoders(std::stop_token stoken) noexcept;

	/// Writes an error event to `mDecodeEventRingBuffer` and signals `mEventSemaphore`
	void SubmitDecodingErrorEvent(NSError *error) noexcept;

	// MARK: - Rendering

	/// Render block implementation
	OSStatus Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList * _Nonnull outputData) noexcept;

	// MARK: - Events

	/// An event header consisting of an event command and event identification number
	template <typename T> requires std::is_same_v<std::underlying_type_t<T>, uint32_t>
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

	// MARK: Decoding Events

	/// Decoding thread events
	enum class DecodingEventCommand : uint32_t {
		/// Decoding started
		eStarted 	= 1,
		/// Decoding complete
		eComplete 	= 2,
		/// Decoder canceled
		eCanceled 	= 3,
		/// Decoding error
		eError 		= 4,
	};

	/// A decoding event header
	using DecodingEventHeader = EventHeader<DecodingEventCommand>;

	// MARK: Rendering Events

	/// Render block events
	enum class RenderingEventCommand : uint32_t {
		/// Timestamp and frames rendered
		eFramesRendered 	= 1,
	};

	/// A rendering event command and identification number
	using RenderingEventHeader = EventHeader<RenderingEventCommand>;

	// MARK: - Event Processing

	/// Sequences events from from `mDecodeEventRingBuffer` and `mRenderEventRingBuffer` for processing in order
	/// - note: This is the thread entry point for the event thread
	void SequenceAndProcessEvents(std::stop_token stoken) noexcept;

	/// Processes an event from `mDecodeEventRingBuffer`
	void ProcessDecodingEvent(const DecodingEventHeader& header) noexcept;

	/// Processes an event from `mRenderEventRingBuffer`
	void ProcessRenderingEvent(const RenderingEventHeader& header) noexcept;

	// MARK: - Active Decoder Management

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number that has not been canceled and has not completed decoding
	DecoderState * const _Nullable GetFirstDecoderStateWithDecodingNotComplete() const noexcept;

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number that has not been canceled and has not completed rendering
	DecoderState * const _Nullable GetFirstDecoderStateWithRenderingNotComplete() const noexcept;

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number greater than `sequenceNumber` that has not been canceled and has not completed rendering
	DecoderState * const _Nullable GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(const uint64_t sequenceNumber) const noexcept;

	/// Returns the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	DecoderState * const _Nullable GetDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) const noexcept;

	/// Removes the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	bool DeleteDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) noexcept;

};

} /* namespace SFB */

#pragma clang diagnostic pop
