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

#import <CXXCoreAudio/AudioRingBuffer.hpp>
#import <CXXRingBuffer/RingBuffer.hpp>
#import <CXXUnfairLock/UnfairLock.hpp>

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

	SFBAudioPlayerNodeDecodingStartedBlock 				decodingStartedBlock_ 				{nil};
	SFBAudioPlayerNodeDecodingCompleteBlock 			decodingCompleteBlock_ 				{nil};
	SFBAudioPlayerNodeRenderingWillStartBlock 			renderingWillStartBlock_ 			{nil};
	SFBAudioPlayerNodeRenderingDecoderWillChangeBlock 	renderingDecoderWillChangeBlock_ 	{nil};
	SFBAudioPlayerNodeRenderingWillCompleteBlock 		renderingWillCompleteBlock_			{nil};
	SFBAudioPlayerNodeDecoderCanceledBlock 				decoderCanceledBlock_ 				{nil};
	SFBAudioPlayerNodeAsynchronousErrorBlock 			asynchronousErrorBlock_ 			{nil};

	/// The shared log for all `AudioPlayerNode` instances
	static const os_log_t log_;

	/// Unsafe reference to owning `SFBAudioPlayerNode` instance
	__unsafe_unretained SFBAudioPlayerNode 	*node_ 				{nil};

	/// The render block supplying audio
	AVAudioSourceNodeRenderBlock 			renderBlock_ 		{nullptr};

private:
	struct DecoderState;

	using DecoderStateVector = std::vector<std::unique_ptr<DecoderState>>;

	/// The format of the audio supplied by `renderBlock_`
	AVAudioFormat 							*renderingFormat_	{nil};

	/// Ring buffer used to transfer audio between the decoding thread and the render block
	CXXCoreAudio::AudioRingBuffer 			audioRingBuffer_ 	{};

	/// Active decoders and associated state
	DecoderStateVector 						activeDecoders_;
	/// Lock used to protect access to `activeDecoders_`
	mutable CXXUnfairLock::UnfairLock 		decoderLock_;

	/// Decoders enqueued for playback that are not yet active
	std::deque<Decoder>						queuedDecoders_ 	{};
	/// Lock used to protect access to `queuedDecoders_`
	mutable CXXUnfairLock::UnfairLock 		queueLock_;

	/// Thread used for decoding
	std::jthread 							decodingThread_;
	/// Dispatch semaphore used for communication with the decoding thread
	dispatch_semaphore_t					decodingSemaphore_ 	{};

	/// Thread used for event processing
	std::jthread 							eventThread_;
	/// Dispatch semaphore used for communication with the event processing thread
	dispatch_semaphore_t					eventSemaphore_ 	{};

	/// Ring buffer used to communicate events from the decoding thread
	CXXRingBuffer::RingBuffer				decodeEventRingBuffer_;
	/// Ring buffer used to communicate events from the render block
	CXXRingBuffer::RingBuffer				renderEventRingBuffer_;

	/// Flags
	std::atomic_uint 						flags_ 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

public:
	AudioPlayerNode(AVAudioFormat * _Nonnull format, uint32_t ringBufferSize);

	AudioPlayerNode(const AudioPlayerNode&) = delete;
	AudioPlayerNode& operator=(const AudioPlayerNode&) = delete;

//	AudioPlayerNode(AudioPlayerNode&&) = delete;
//	AudioPlayerNode& operator=(AudioPlayerNode&&) = delete;

	~AudioPlayerNode() noexcept;

	// MARK: - Queue Management

	bool EnqueueDecoder(Decoder _Nonnull decoder, bool reset, NSError * _Nullable * _Nullable error) noexcept;

	/// Pops the next decoder from the decoder queue
	Decoder _Nullable DequeueDecoder() noexcept;

	bool RemoveDecoderFromQueue(Decoder _Nonnull decoder) noexcept;

	void ClearQueue() noexcept
	{
		std::lock_guard lock(queueLock_);
		queuedDecoders_.clear();
	}

	bool QueueIsEmpty() const noexcept
	{
		std::lock_guard lock(queueLock_);
		return queuedDecoders_.empty();
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
		flags_.fetch_or(static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
	}

	void Pause() noexcept
	{
		flags_.fetch_and(~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
	}

	void Stop() noexcept
	{
		flags_.fetch_and(~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
		Reset();
	}

	void TogglePlayPause() noexcept
	{
		flags_.fetch_xor(static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
	}

	// MARK: - Playback State

	bool IsPlaying() const noexcept
	{
		return flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::isPlaying);
	}

	bool IsReady() const noexcept
	{
		std::lock_guard lock(decoderLock_);
		return FirstDecoderStateWithRenderingNotComplete() != nullptr;
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
		return renderingFormat_;
	}

	bool SupportsFormat(AVAudioFormat * _Nonnull format) const noexcept;

private:
	// MARK: - Flags

	/// Possible bits in `flags_`
	enum class Flags : unsigned int {
		/// The render block is outputting audio
		isPlaying 				= 1u << 0,
		/// The render block is outputting silence
		isMuted 				= 1u << 1,
		/// The decoding thread requests that the render block drain the ring buffer during the next render cycle
		drainRequired 			= 1u << 2,
		/// The decoding thread should unmute after the next decoder is dequeued and becomes active
		unmuteAfterDequeue 		= 1u << 3,
	};

	// MARK: - Decoding

	/// Dequeues and processes decoders from the decoder queue
	/// - note: This is the thread entry point for the decoding thread
	void ProcessDecoders(std::stop_token stoken) noexcept;

	/// Writes an error event to `decodeEventRingBuffer_` and signals `eventSemaphore_`
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
		started 	= 1,
		/// Decoding complete
		complete 	= 2,
		/// Decoder canceled
		canceled 	= 3,
		/// Decoding error
		error 		= 4,
	};

	/// A decoding event header
	using DecodingEventHeader = EventHeader<DecodingEventCommand>;

	// MARK: Rendering Events

	/// Render block events
	enum class RenderingEventCommand : uint32_t {
		/// Timestamp and frames rendered
		framesRendered 	= 1,
	};

	/// A rendering event command and identification number
	using RenderingEventHeader = EventHeader<RenderingEventCommand>;

	// MARK: - Event Processing

	/// Sequences events from from `decodeEventRingBuffer_` and `renderEventRingBuffer_` for processing in order
	/// - note: This is the thread entry point for the event thread
	void SequenceAndProcessEvents(std::stop_token stoken) noexcept;

	/// Processes an event from `decodeEventRingBuffer_`
	void ProcessDecodingEvent(const DecodingEventHeader& header) noexcept;

	/// Processes an event from `renderEventRingBuffer_`
	void ProcessRenderingEvent(const RenderingEventHeader& header) noexcept;

	// MARK: - Active Decoder Management

	/// Returns the decoder state in `activeDecoders_` with the smallest sequence number that has not been canceled and has not completed decoding
	DecoderState * const _Nullable FirstDecoderStateWithDecodingNotComplete() const noexcept;

	/// Returns the decoder state in `activeDecoders_` with the smallest sequence number that has not been canceled and has not completed rendering
	DecoderState * const _Nullable FirstDecoderStateWithRenderingNotComplete() const noexcept;

	/// Returns the decoder state in `activeDecoders_` with the smallest sequence number greater than `sequenceNumber` that has not been canceled and has not completed rendering
	DecoderState * const _Nullable FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(const uint64_t sequenceNumber) const noexcept;

	/// Returns the decoder state in `activeDecoders_` with sequence number equal to `sequenceNumber`
	DecoderState * const _Nullable DecoderStateWithSequenceNumber(const uint64_t sequenceNumber) const noexcept;

	/// Removes the decoder state in `activeDecoders_` with sequence number equal to `sequenceNumber`
	bool DeleteDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) noexcept;
};

} /* namespace SFB */

#pragma clang diagnostic pop
