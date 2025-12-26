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

#import <os/log.h>

#import <AVFAudio/AVFAudio.h>

#import <CXXCoreAudio/AudioRingBuffer.hpp>
#import <CXXRingBuffer/RingBuffer.hpp>
#import <CXXUnfairLock/UnfairLock.hpp>

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayer.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

namespace SFB {

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept;

// MARK: - AudioPlayer

/// SFBAudioPlayer implementation
class AudioPlayer final {
public:
	using unique_ptr 	= std::unique_ptr<AudioPlayer>;
	using Decoder 		= id<SFBPCMDecoding>;

	/// The shared log for all `AudioPlayer` instances
	static const os_log_t log_;

	/// Unsafe reference to owning `SFBAudioPlayer` instance
	__unsafe_unretained SFBAudioPlayer 		*player_ 			{nil};

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

	/// The underlying `AVAudioEngine` instance
	AVAudioEngine 							*engine_ 			{nil};
	/// The source node driving the audio processing graph
	AVAudioSourceNode						*sourceNode_ 		{nil};
	/// The lock used to protect engine processing graph configuration changes
	mutable CXXUnfairLock::UnfairLock 		engineLock_;

	/// The currently rendering decoder
	id <SFBPCMDecoding> 					nowPlaying_ 		{nil};
	/// The lock used to protect access to `nowPlaying_`
	mutable CXXUnfairLock::UnfairLock 		nowPlayingLock_;

	/// The dispatch queue used for asynchronous events
	dispatch_queue_t						eventQueue_ 		{nil};

	/// Flags
	std::atomic_uint 						flags_ 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

public:
	AudioPlayer();

	AudioPlayer(const AudioPlayer&) = delete;
	AudioPlayer& operator=(const AudioPlayer&) = delete;

//	AudioPlayer(AudioPlayer&&) = delete;
//	AudioPlayer& operator=(AudioPlayer&&) = delete;

	~AudioPlayer() noexcept;

	// MARK: - Playlist Management

	bool EnqueueDecoder(Decoder _Nonnull decoder, bool forImmediatePlayback, NSError **error) noexcept;

	bool FormatWillBeGaplessIfEnqueued(AVAudioFormat * _Nonnull format) const noexcept;

	void ClearDecoderQueue() noexcept
	{
		std::lock_guard lock{queueLock_};
		queuedDecoders_.clear();
	}

	bool DecoderQueueIsEmpty() const noexcept
	{
		std::lock_guard lock{queueLock_};
		return queuedDecoders_.empty();
	}

	// MARK: - Playback Control

	bool Play(NSError **error) noexcept;
	void Pause() noexcept;
	void Resume() noexcept;
	void Stop() noexcept;
	bool TogglePlayPause(NSError **error) noexcept;

	void Reset() noexcept;

	// MARK: - Player State

	bool EngineIsRunning() const noexcept;

	SFBAudioPlayerPlaybackState PlaybackState() const noexcept
	{
		if(const auto flags = flags_.load(std::memory_order_acquire); flags & static_cast<unsigned int>(Flags::engineIsRunning)) {
			if(flags & static_cast<unsigned int>(Flags::isPlaying))
				return SFBAudioPlayerPlaybackStatePlaying;
			else
				return SFBAudioPlayerPlaybackStatePaused;
		} else
			return SFBAudioPlayerPlaybackStateStopped;
	}

	bool IsPlaying() const noexcept
	{
		const auto flags = flags_.load(std::memory_order_acquire);
		return (flags & static_cast<unsigned int>(Flags::engineIsRunning)) && (flags & static_cast<unsigned int>(Flags::isPlaying));
	}

	bool IsPaused() const noexcept
	{
		const auto flags = flags_.load(std::memory_order_acquire);
		return (flags & static_cast<unsigned int>(Flags::engineIsRunning)) && !(flags & static_cast<unsigned int>(Flags::isPlaying));
	}

	bool IsStopped() const noexcept
	{
		return !(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning));
	}

	bool IsReady() const noexcept
	{
		std::lock_guard lock{decoderLock_};
		return FirstDecoderStateWithRenderingNotComplete() != nullptr;
	}

	Decoder _Nullable CurrentDecoder() const noexcept;

	Decoder _Nullable NowPlaying() const noexcept
	{
		std::lock_guard lock{nowPlayingLock_};
		return nowPlaying_;
	}

private:
	void SetNowPlaying(Decoder _Nullable nowPlaying) noexcept;

public:
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

#if !TARGET_OS_IPHONE

	// MARK: - Volume Control

	float VolumeForChannel(AudioObjectPropertyElement channel) const noexcept;
	bool SetVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept;

	// MARK: - Output Device

	AUAudioObjectID OutputDeviceID() const noexcept;
	bool SetOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept;

#endif /* !TARGET_OS_IPHONE */

	// MARK: - AVAudioEngine Modification

	void WithEngine(void(^block)(AVAudioEngine * _Nonnull engine, AVAudioSourceNode * _Nonnull sourceNode)) const noexcept;

	// MARK: - Debugging

	void LogProcessingGraphDescription(os_log_t _Nonnull log, os_log_type_t type) const noexcept;

private:
	/// Possible bits in `flags_`
	enum class Flags : unsigned int {
		/// Cached value of `_audioEngine.isRunning`
		engineIsRunning				= 1u << 0,
		/// The render block is outputting audio
		isPlaying 					= 1u << 1,
		/// The render block is outputting silence
		isMuted 					= 1u << 2,
		/// The ring buffer needs to be drained during the next render cycle
		drainRequired 				= 1u << 3,
		/// The decoding thread should unmute after the next decoder is dequeued and becomes active
		unmuteAfterDequeue 			= 1u << 4,
		/// Mismatch between rendering format and decoder processing format
		formatMismatch 				= 1u << 5,
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

	/// Called before decoding the first frame of audio from a decoder.
	void HandleDecodingStartedEvent(Decoder _Nonnull decoder) noexcept;

	/// Called after decoding the final frame of audio from a decoder.
	void HandleDecodingCompleteEvent(Decoder _Nonnull decoder) noexcept;

	/// Called when the first audio frame from a decoder will render.
	void HandleRenderingWillStartEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

	/// Called when the final audio frame from a decoder will render.
	void HandleRenderingWillCompleteEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

	/// Called when the decoding and rendering process for a decoder has been canceled.
	void HandleDecoderCanceledEvent(Decoder _Nonnull decoder, AVAudioFramePosition framesRendered) noexcept;

	/// Called when an asynchronous error occurs.
	void HandleAsynchronousErrorEvent(NSError * _Nonnull error) noexcept;

	// MARK: - Active Decoder Management

	/// Cancels active decoders in sequence
	void CancelActiveDecoders(bool cancelAllActive) noexcept;

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

public:
	// MARK: - AVAudioEngine Notification Handling

	/// Called to process `AVAudioEngineConfigurationChangeNotification`
	void HandleAudioEngineConfigurationChange(AVAudioEngine * _Nonnull engine, NSDictionary * _Nullable userInfo) noexcept;

#if TARGET_OS_IPHONE
	/// Called to process `AVAudioSessionInterruptionNotification`
	void HandleAudioSessionInterruption(NSDictionary * _Nullable userInfo) noexcept;
#endif /* TARGET_OS_IPHONE */

private:
	// MARK: - Processing Graph Management

	/// Configures the player to render audio from `decoder`
	/// - parameter error: An optional pointer to an `NSError` object to receive error information
	/// - returns: `true` if the player was successfully configured
	bool ConfigureProcessingGraphAndRingBufferForDecoder(Decoder _Nonnull decoder, NSError **error) noexcept;

	/// Configures the audio processing graph for playback of audio with `format`, replacing the audio source node if `replaceSourceNode` is true
	/// - important: This stops the audio engine
	/// - parameter format: The desired audio format
	/// - parameter replaceSourceNode: Whether the audio source node driving the graph should be replaced
	/// - returns: `true` if the processing graph was successfully configured
	bool ConfigureProcessingGraph(AVAudioFormat * _Nonnull format, bool replaceSourceNode) noexcept;
};

} /* namespace SFB */

#pragma clang diagnostic pop
