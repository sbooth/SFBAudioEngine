//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayer.h"

#import <CXXCoreAudio/AudioRingBuffer.hpp>
#import <CXXRingBuffer/RingBuffer.hpp>
#import <CXXUnfairLock/UnfairLock.hpp>

#import <AVFAudio/AVFAudio.h>

#import <os/log.h>

#import <atomic>
#import <cassert>
#import <deque>
#import <memory>
#import <mutex>
#import <stop_token>
#import <thread>
#import <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

namespace sfb {

// MARK: - AudioPlayer

/// SFBAudioPlayer implementation
class AudioPlayer final {
  public:
    using unique_ptr = std::unique_ptr<AudioPlayer>;
    using Decoder = id<SFBPCMDecoding>;

    /// The shared log for all `AudioPlayer` instances
    static const os_log_t log_;

    /// Weak reference to owning `SFBAudioPlayer` instance
    __weak SFBAudioPlayer *player_{nil};

  private:
    struct DecoderState;

    using DecoderStateVector = std::vector<std::unique_ptr<DecoderState>>;

    /// Ring buffer transferring audio between the decoding thread and the render block
    CXXCoreAudio::AudioRingBuffer audioRingBuffer_;

    /// Active decoders and associated state
    DecoderStateVector activeDecoders_;
    /// Lock protecting `activeDecoders_`
    mutable CXXUnfairLock::UnfairLock activeDecodersLock_;

    /// Decoders enqueued for playback that are not yet active
    std::deque<Decoder> queuedDecoders_;
    /// Lock protecting `queuedDecoders_`
    mutable CXXUnfairLock::UnfairLock queuedDecodersLock_;

    /// Thread used for decoding
    std::jthread decodingThread_;
    /// Dispatch semaphore used for communication with the decoding thread
    dispatch_semaphore_t decodingSemaphore_{nil};

    /// Thread used for event processing
    std::jthread eventThread_;
    /// Dispatch semaphore used for communication with the event processing thread
    dispatch_semaphore_t eventSemaphore_{nil};

    /// Ring buffer communicating events from the decoding thread to the event processing thread
    CXXRingBuffer::RingBuffer decodingEvents_;
    /// Ring buffer communicating events from the render block to the event processing thread
    CXXRingBuffer::RingBuffer renderingEvents_;

    /// The `AVAudioEngine` instance
    AVAudioEngine *engine_{nil};
    /// Source node driving the audio processing graph
    AVAudioSourceNode *sourceNode_{nil};
    /// Lock protecting playback state and processing graph configuration changes
    mutable CXXUnfairLock::UnfairLock engineLock_;

    /// Decoder currently rendering audio
    Decoder nowPlaying_{nil};
    /// Lock protecting `nowPlaying_`
    mutable CXXUnfairLock::UnfairLock nowPlayingLock_;

    /// Dispatch queue used for asynchronous render event notifications
    dispatch_queue_t eventQueue_{nil};

    /// Player flags
    std::atomic_uint flags_{0};
    static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

#if TARGET_OS_IPHONE
    /// Playback state before audio session interruption
    unsigned int preInterruptState_{0};
#endif /* TARGET_OS_IPHONE */

  public:
    AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;

    ~AudioPlayer() noexcept;

    // MARK: - Playlist Management

    bool enqueueDecoder(Decoder _Nonnull decoder, bool forImmediatePlayback, NSError **error) noexcept;

    bool formatWillBeGaplessIfEnqueued(AVAudioFormat *_Nonnull format) const noexcept;

    void clearDecoderQueue() noexcept;
    bool decoderQueueIsEmpty() const noexcept;

    // MARK: - Playback Control

    bool play(NSError **error) noexcept;
    bool pause() noexcept;
    bool resume() noexcept;
    void stop() noexcept;
    bool togglePlayPause(NSError **error) noexcept;

    void reset() noexcept;

    // MARK: - Player State

    bool engineIsRunning() const noexcept;

    SFBAudioPlayerPlaybackState playbackState() const noexcept;

    bool isPlaying() const noexcept;
    bool isPaused() const noexcept;
    bool isStopped() const noexcept;
    bool isReady() const noexcept;

    Decoder _Nullable currentDecoder() const noexcept;

    Decoder _Nullable nowPlaying() const noexcept;

  private:
    void setNowPlaying(Decoder _Nullable nowPlaying) noexcept;

  public:
    // MARK: - Playback Properties

    SFBPlaybackPosition playbackPosition() const noexcept;
    SFBPlaybackTime playbackTime() const noexcept;
    bool getPlaybackPositionAndTime(SFBPlaybackPosition *_Nullable playbackPosition,
                                    SFBPlaybackTime *_Nullable playbackTime) const noexcept;

    // MARK: - Seeking

    bool seekInTime(NSTimeInterval secondsToSkip) noexcept;
    bool seekToTime(NSTimeInterval timeInSeconds) noexcept;
    bool seekToPosition(double position) noexcept;
    bool seekToFrame(AVAudioFramePosition frame) noexcept;
    bool supportsSeeking() const noexcept;

#if !TARGET_OS_IPHONE
    // MARK: - Volume Control

    float volumeForChannel(AudioObjectPropertyElement channel) const noexcept;
    bool setVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept;

    // MARK: - Output Device

    AUAudioObjectID outputDeviceID() const noexcept;
    bool setOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept;
#endif /* !TARGET_OS_IPHONE */

    // MARK: - AVAudioEngine

    void modifyProcessingGraph(void (^_Nonnull block)(AVAudioEngine *_Nonnull engine)) const noexcept;

    AVAudioSourceNode *_Nonnull sourceNode() const noexcept;
    AVAudioMixerNode *_Nonnull mainMixerNode() const noexcept;
    AVAudioOutputNode *_Nonnull outputNode() const noexcept;

    // MARK: - Debugging

    void logProcessingGraphDescription(os_log_t _Nonnull log, os_log_type_t type) const noexcept;

  private:
    /// Possible bits in `flags_`
    enum class Flags : unsigned int {
        /// Cached value of `engine_.isRunning`
        engineIsRunning = 1U << 0,
        /// The render block should output audio
        isPlaying = 1U << 1,
        /// The render block should output silence
        isMuted = 1U << 2,
        /// The ring buffer needs to be drained during the next render cycle
        drainRequired = 1U << 3,
    };

    // MARK: - Decoding

    /// Dequeues and processes decoders from the decoder queue
    /// - note: This is the thread entry point for the decoding thread
    void processDecoders(std::stop_token stoken) noexcept;

    /// Writes an error event to `decodingEvents_` and signals `eventSemaphore_`
    void submitDecodingErrorEvent(NSError *error) noexcept;

    // MARK: - Rendering

    /// Render block implementation
    OSStatus render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount,
                    AudioBufferList *_Nonnull outputData) noexcept;

    // MARK: - Events

    /// Decoding thread events
    enum class DecodingEventCommand : uint32_t {
        /// Decoding started
        started = 1,
        /// Decoding complete
        complete = 2,
        /// Decoder canceled by user or aborted due to error
        canceled = 3,
        /// Decoding error
        error = 4,
    };

    /// Render block events
    enum class RenderingEventCommand : uint32_t {
        /// Audio frames rendered from ring buffer
        framesRendered = 1,
    };

    // MARK: - Event Processing

    /// Reads and sequences event headers from `decodingEvents_` and `renderingEvents_` for processing in order
    /// - note: This is the thread entry point for the event processing thread
    void sequenceAndProcessEvents(std::stop_token stoken) noexcept;

    /// Reads and processes an event payload from `decodingEvents_`
    bool processDecodingEvent(DecodingEventCommand command) noexcept;

    /// Reads and processes a decoding started event from `decodingEvents_`
    bool processDecodingStartedEvent() noexcept;

    /// Reads and processes a decoding complete event from `decodingEvents_`
    bool processDecodingCompleteEvent() noexcept;

    /// Reads and processes a decoder canceled event from `decodingEvents_`
    bool processDecoderCanceledEvent() noexcept;

    /// Reads and processes a decoding error event from `decodingEvents_`
    bool processDecodingErrorEvent() noexcept;

    /// Reads and processes an event payload from `renderingEvents_`
    bool processRenderingEvent(RenderingEventCommand command) noexcept;

    /// Reads and processes a frames rendered event from `renderingEvents_`
    bool processFramesRenderedEvent() noexcept;

    /// Called when the first audio frame from a decoder will render.
    void handleRenderingWillStartEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

    /// Called when the final audio frame from a decoder will render.
    void handleRenderingWillCompleteEvent(Decoder _Nonnull decoder, uint64_t hostTime) noexcept;

    // MARK: - Active Decoder Management

    /// Cancels all active decoders in sequence
    void cancelActiveDecoders() noexcept;

    /// Returns the first decoder state in `activeDecoders_` that has not been canceled
    DecoderState *_Nullable firstActiveDecoderState() const noexcept;

  public:
    // MARK: - AVAudioEngine Notification Handling

    /// Called to process `AVAudioEngineConfigurationChangeNotification`
    void handleAudioEngineConfigurationChange(AVAudioEngine *_Nonnull engine,
                                              NSDictionary *_Nullable userInfo) noexcept;

#if TARGET_OS_IPHONE
    /// Called to process `AVAudioSessionInterruptionNotification`
    void handleAudioSessionInterruption(NSDictionary *_Nullable userInfo) noexcept;
#endif /* TARGET_OS_IPHONE */

  private:
    // MARK: - Processing Graph Management

    /// Stops the AVAudioEngine if it is running and returns true if it was stopped
    bool stopEngineIfRunning() noexcept;

    /// Configures the player to render audio with `format`
    /// - parameter format: The desired audio format
    /// - parameter error: An optional pointer to an `NSError` object to receive error information
    /// - returns: `true` if the player was successfully configured
    bool configureProcessingGraphAndRingBufferForFormat(AVAudioFormat *_Nonnull format, NSError **error) noexcept;
};

// MARK: - Implementation -

inline void AudioPlayer::clearDecoderQueue() noexcept {
    std::lock_guard lock{queuedDecodersLock_};
    queuedDecoders_.clear();
}

inline bool AudioPlayer::decoderQueueIsEmpty() const noexcept {
    std::lock_guard lock{queuedDecodersLock_};
    return queuedDecoders_.empty();
}

inline SFBAudioPlayerPlaybackState AudioPlayer::playbackState() const noexcept {
    const auto flags = flags_.load(std::memory_order_acquire);
    constexpr auto mask =
          static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
    const auto state = flags & mask;
    assert(state != static_cast<unsigned int>(Flags::isPlaying));
    return static_cast<SFBAudioPlayerPlaybackState>(state);
}

inline bool AudioPlayer::isPlaying() const noexcept {
    const auto flags = flags_.load(std::memory_order_acquire);
    constexpr auto mask =
          static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
    return (flags & mask) == mask;
}

inline bool AudioPlayer::isPaused() const noexcept {
    const auto flags = flags_.load(std::memory_order_acquire);
    constexpr auto mask =
          static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying);
    return (flags & mask) == static_cast<unsigned int>(Flags::engineIsRunning);
}

inline bool AudioPlayer::isStopped() const noexcept {
    const auto flags = flags_.load(std::memory_order_acquire);
    return (flags & static_cast<unsigned int>(Flags::engineIsRunning)) == 0U;
}

inline bool AudioPlayer::isReady() const noexcept {
    std::lock_guard lock{activeDecodersLock_};
    return firstActiveDecoderState() != nullptr;
}

inline AudioPlayer::Decoder _Nullable AudioPlayer::nowPlaying() const noexcept {
    std::lock_guard lock{nowPlayingLock_};
    return nowPlaying_;
}

inline AVAudioSourceNode *_Nonnull AudioPlayer::sourceNode() const noexcept {
    return sourceNode_;
}

inline AVAudioMixerNode *_Nonnull AudioPlayer::mainMixerNode() const noexcept {
    return engine_.mainMixerNode;
}

inline AVAudioOutputNode *_Nonnull AudioPlayer::outputNode() const noexcept {
    return engine_.outputNode;
}

} /* namespace sfb */

#pragma clang diagnostic pop
