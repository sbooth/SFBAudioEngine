//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <atomic>
#import <cassert>
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

// MARK: - AudioPlayer

/// SFBAudioPlayer implementation
class AudioPlayer final {
public:
	using unique_ptr 	= std::unique_ptr<AudioPlayer>;
	using Decoder 		= id<SFBPCMDecoding>;

	/// The shared log for all `AudioPlayer` instances
	static const os_log_t log_;

	/// Weak reference to owning `SFBAudioPlayer` instance
	__weak SFBAudioPlayer 					*player_ 			{nil};

private:
	struct DecoderState;

	using DecoderStateVector = std::vector<std::unique_ptr<DecoderState>>;

	/// Ring buffer transferring audio between the decoding thread and the render block
	CXXCoreAudio::AudioRingBuffer 			audioRingBuffer_;

	/// Active decoders and associated state
	DecoderStateVector 						activeDecoders_;
	/// Lock protecting `activeDecoders_`
	mutable CXXUnfairLock::UnfairLock 		activeDecodersLock_;

	/// Decoders enqueued for playback that are not yet active
	std::deque<Decoder>						queuedDecoders_;
	/// Lock protecting `queuedDecoders_`
	mutable CXXUnfairLock::UnfairLock 		queuedDecodersLock_;

	/// Thread used for decoding
	std::jthread 							decodingThread_;
	/// Dispatch semaphore used for communication with the decoding thread
	dispatch_semaphore_t					decodingSemaphore_ 	{nil};

	/// Thread used for event processing
	std::jthread 							eventThread_;
	/// Dispatch semaphore used for communication with the event processing thread
	dispatch_semaphore_t					eventSemaphore_ 	{nil};

	/// Ring buffer communicating events from the decoding thread to the event processing thread
	CXXRingBuffer::RingBuffer				decodingEvents_;
	/// Ring buffer communicating events from the render block to the event processing thread
	CXXRingBuffer::RingBuffer				renderingEvents_;

	/// The `AVAudioEngine` instance
	AVAudioEngine 							*engine_ 			{nil};
	/// Source node driving the audio processing graph
	AVAudioSourceNode						*sourceNode_ 		{nil};
	/// Lock protecting playback state and processing graph configuration changes
	mutable CXXUnfairLock::UnfairLock 		engineLock_;

	/// Decoder currently rendering audio
	Decoder 								nowPlaying_ 		{nil};
	/// Lock protecting `nowPlaying_`
	mutable CXXUnfairLock::UnfairLock 		nowPlayingLock_;

	/// Dispatch queue used for asynchronous render event notifications
	dispatch_queue_t						eventQueue_ 		{nil};

	/// Player flags
	std::atomic_uint 						flags_ 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

#if TARGET_OS_IPHONE
	/// Playback state before audio session interruption
	unsigned int 							preInterruptState_ 	{0};
#endif /* TARGET_OS_IPHONE */

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

	void ClearDecoderQueue() noexcept;
	bool DecoderQueueIsEmpty() const noexcept;

	// MARK: - Playback Control

	bool Play(NSError **error) noexcept;
	bool Pause() noexcept;
	bool Resume() noexcept;
	void Stop() noexcept;
	bool TogglePlayPause(NSError **error) noexcept;

	void Reset() noexcept;

	// MARK: - Player State

	bool EngineIsRunning() const noexcept;

	SFBAudioPlayerPlaybackState PlaybackState() const noexcept;

	bool IsPlaying() const noexcept;
	bool IsPaused() const noexcept;
	bool IsStopped() const noexcept;
	bool IsReady() const noexcept;

	Decoder _Nullable CurrentDecoder() const noexcept;

	Decoder _Nullable NowPlaying() const noexcept;

private:
	void SetNowPlaying(Decoder _Nullable nowPlaying) noexcept;

public:
	// MARK: - Playback Properties

	SFBPlaybackPosition PlaybackPosition() const noexcept;
	SFBPlaybackTime PlaybackTime() const noexcept;
	bool GetPlaybackPositionAndTime(SFBPlaybackPosition * _Nullable playbackPosition, SFBPlaybackTime * _Nullable playbackTime) const noexcept;

	// MARK: - Seeking

	bool SeekInTime(NSTimeInterval secondsToSkip) noexcept;
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

	// MARK: - AVAudioEngine

	void ModifyProcessingGraph(void(^ _Nonnull block)(AVAudioEngine * _Nonnull engine)) const noexcept;

	AVAudioSourceNode * _Nonnull SourceNode() const noexcept;
	AVAudioMixerNode * _Nonnull MainMixerNode() const noexcept;
	AVAudioOutputNode * _Nonnull OutputNode() const noexcept;

	// MARK: - Debugging

	void LogProcessingGraphDescription(os_log_t _Nonnull log, os_log_type_t type) const noexcept;

private:
	/// Possible bits in `flags_`
	enum class Flags : unsigned int {
		/// Cached value of `engine_.isRunning`
		engineIsRunning				= 1u << 0,
		/// The render block should output audio
		isPlaying 					= 1u << 1,
		/// The render block should output silence
		isMuted 					= 1u << 2,
		/// The ring buffer needs to be drained during the next render cycle
		drainRequired 				= 1u << 3,
	};

	// MARK: - Decoding

	/// Dequeues and processes decoders from the decoder queue
	/// - note: This is the thread entry point for the decoding thread
	void ProcessDecoders(std::stop_token stoken) noexcept;

	/// Writes an error event to `decodingEvents_` and signals `eventSemaphore_`
	void SubmitDecodingErrorEvent(NSError *error) noexcept;

	// MARK: - Rendering

	/// Render block implementation
	OSStatus Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList * _Nonnull outputData) noexcept;

	// MARK: - Events

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

	/// Render block events
	enum class RenderingEventCommand : uint32_t {
		/// Audio frames rendered from ring buffer
		framesRendered 	= 1,
	};

	// MARK: - Event Processing

	/// Reads and sequences event headers from `decodingEvents_` and `renderingEvents_` for processing in order
	/// - note: This is the thread entry point for the event processing thread
	void SequenceAndProcessEvents(std::stop_token stoken) noexcept;

	/// Reads and processes an event payload from `decodingEvents_`
	bool ProcessDecodingEvent(DecodingEventCommand command) noexcept;

	/// Reads and processes a decoding started event from `decodingEvents_`
	bool ProcessDecodingStartedEvent() noexcept;

	/// Reads and processes a decoding complete event from `decodingEvents_`
	bool ProcessDecodingCompleteEvent() noexcept;

	/// Reads and processes a decoder canceled event from `decodingEvents_`
	bool ProcessDecoderCanceledEvent() noexcept;

	/// Reads and processes a decoding error event from `decodingEvents_`
	bool ProcessDecodingErrorEvent() noexcept;

	/// Reads and processes an event payload from `renderingEvents_`
	bool ProcessRenderingEvent(RenderingEventCommand command) noexcept;

	/// Reads and processes a frames rendered event from `renderingEvents_`
	bool ProcessFramesRenderedEvent() noexcept;

	/// Called when the first audio frame from a decoder will render.
	void HandleRenderingWillStartEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

	/// Called when the final audio frame from a decoder will render.
	void HandleRenderingWillCompleteEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

	// MARK: - Active Decoder Management

	/// Cancels all active decoders in sequence
	void CancelActiveDecoders() noexcept;

	/// Returns the first decoder state in `activeDecoders_` that has not been canceled
	DecoderState * const _Nullable FirstActiveDecoderState() const noexcept;

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

	/// Configures the player to render audio with `format`
	/// - parameter format: The desired audio format
	/// - parameter error: An optional pointer to an `NSError` object to receive error information
	/// - returns: `true` if the player was successfully configured
	bool ConfigureProcessingGraphAndRingBufferForFormat(AVAudioFormat * _Nonnull format, NSError **error) noexcept;
};

// MARK: - Implementation -

inline void AudioPlayer::ClearDecoderQueue() noexcept
{
	std::lock_guard lock{queuedDecodersLock_};
	queuedDecoders_.clear();
}

inline bool AudioPlayer::DecoderQueueIsEmpty() const noexcept
{
	std::lock_guard lock{queuedDecodersLock_};
	return queuedDecoders_.empty();
}

inline SFBAudioPlayerPlaybackState AudioPlayer::PlaybackState() const noexcept
{
	const auto flags = flags_.load(std::memory_order_acquire);
	constexpr auto mask = static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
	const auto state = flags & mask;
	assert(state != static_cast<unsigned int>(Flags::isPlaying));
	return static_cast<SFBAudioPlayerPlaybackState>(state);
}

inline bool AudioPlayer::IsPlaying() const noexcept
{
	const auto flags = flags_.load(std::memory_order_acquire);
	constexpr auto mask = static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
	return (flags & mask) == mask;
}

inline bool AudioPlayer::IsPaused() const noexcept
{
	const auto flags = flags_.load(std::memory_order_acquire);
	constexpr auto mask = static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
	return (flags & mask) == static_cast<unsigned int>(Flags::engineIsRunning);
}

inline bool AudioPlayer::IsStopped() const noexcept
{
	const auto flags = flags_.load(std::memory_order_acquire);
	return !(flags & static_cast<unsigned int>(Flags::engineIsRunning));
}

inline bool AudioPlayer::IsReady() const noexcept
{
	std::lock_guard lock{activeDecodersLock_};
	return FirstActiveDecoderState() != nullptr;
}

inline AudioPlayer::Decoder _Nullable AudioPlayer::NowPlaying() const noexcept
{
	std::lock_guard lock{nowPlayingLock_};
	return nowPlaying_;
}

inline AVAudioSourceNode * _Nonnull AudioPlayer::SourceNode() const noexcept
{
	return sourceNode_;
}

inline AVAudioMixerNode * _Nonnull AudioPlayer::MainMixerNode() const noexcept
{
	return engine_.mainMixerNode;
}

inline AVAudioOutputNode * _Nonnull AudioPlayer::OutputNode() const noexcept
{
	return engine_.outputNode;
}

} /* namespace SFB */

#pragma clang diagnostic pop
