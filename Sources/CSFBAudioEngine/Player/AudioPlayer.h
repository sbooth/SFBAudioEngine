//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayer.h"
#import "bitmask_enum.hpp"

#import <dsema/Semaphore.hpp>
#import <mpsc/MessageQueue.hpp>
#import <mtx/UnfairMutex.hpp>
#import <spsc/AudioRingBuffer.hpp>

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
    spsc::AudioRingBuffer audioRingBuffer_;

    /// Active decoders and associated state
    DecoderStateVector activeDecoders_;
    /// Mutex protecting `activeDecoders_`
    mutable mtx::UnfairMutex activeDecodersMutex_;

    /// Decoders enqueued for playback that are not yet active
    std::deque<Decoder> queuedDecoders_;
    /// Mutex protecting `queuedDecoders_`
    mutable mtx::UnfairMutex queuedDecodersMutex_;

    /// Thread used for decoding
    std::jthread decodingThread_;
    /// Dispatch semaphore used for communication with the decoding thread
    dsema::Semaphore decodingSemaphore_{0};

    /// Thread used for event processing
    std::jthread eventThread_;
    /// Dispatch semaphore used for communication with the event processing thread
    dsema::Semaphore eventSemaphore_{0};

    /// Message queue communicating events to the event processing thread
    mpsc::MessageQueue<256, 32> events_;

    /// The `AVAudioEngine` instance
    AVAudioEngine *engine_{nil};
    /// Source node driving the audio processing graph
    AVAudioSourceNode *sourceNode_{nil};
    /// Mutex protecting playback state and processing graph configuration changes
    mutable mtx::UnfairMutex engineMutex_;

    /// Decoder currently rendering audio
    Decoder nowPlaying_{nil};
    /// Mutex protecting `nowPlaying_`
    mutable mtx::UnfairMutex nowPlayingMutex_;

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

    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;

    AudioPlayer(AudioPlayer &&) = delete;
    AudioPlayer &operator=(AudioPlayer &&) = delete;

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

  private:
    bool performClampingSeekToFrame(DecoderState *_Nonnull decoderState, AVAudioFramePosition frame,
                                    bool isRelative) noexcept;

  public:
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
        engineIsRunning = 1u << 0,
        /// The render block should output audio
        isPlaying = 1u << 1,
        /// The render block should output silence
        isMuted = 1u << 2,
        /// The ring buffer needs to be drained during the next render cycle
        drainRequired = 1u << 3,
        /// The render event ring buffer had insufficient space to record an event
        renderEventDropped = 1u << 4,
    };

    // Enable bitmask operations for `Flags`
    friend constexpr void is_bitmask_enum(Flags);

    // Hidden friends
    friend constexpr Flags operator|(Flags l, Flags r) noexcept { return bits::operator|(l, r); }
    friend constexpr Flags operator&(Flags l, Flags r) noexcept { return bits::operator&(l, r); }

    /// Atomically loads the value of `flags_` using the specified memory order and returns the result
    [[nodiscard]] Flags loadFlags(std::memory_order order = std::memory_order_acquire) const noexcept {
        return static_cast<Flags>(flags_.load(order));
    }

    /// Atomically sets flags using the specified memory order and returns the previous value
    Flags setFlags(Flags flags, std::memory_order order = std::memory_order_acq_rel) noexcept {
        return static_cast<Flags>(flags_.fetch_or(bits::to_underlying(flags), order));
    }

    /// Atomically toggles flags using the specified memory order and returns the previous value
    Flags toggleFlags(Flags flags, std::memory_order order = std::memory_order_acq_rel) noexcept {
        return static_cast<Flags>(flags_.fetch_xor(bits::to_underlying(flags), order));
    }

    /// Atomically clears flags using the specified memory order and returns the previous value
    Flags clearFlags(Flags flags, std::memory_order order = std::memory_order_acq_rel) noexcept {
        return static_cast<Flags>(flags_.fetch_and(~bits::to_underlying(flags), order));
    }

    // MARK: - Decoding

    /// Dequeues and processes decoders from the decoder queue
    /// - note: This is the thread entry point for the decoding thread
    void processDecoders(std::stop_token stoken) noexcept;

    // MARK: - Rendering

    /// Render block implementation
    OSStatus render(BOOL &isSilence, const AudioTimeStamp &timestamp, AVAudioFrameCount frameCount,
                    AudioBufferList *_Nonnull outputData) noexcept;

    // MARK: - Events

    /// Event commands
    enum class EventCommand : uint32_t {
        /// Decoding started
        decodingStarted = 1,
        /// Decoding complete
        decodingComplete = 2,
        /// Seek
        seek = 3,
        /// Decoder canceled by user or aborted due to error
        decoderCanceled = 4,
        /// Allocation failure
        allocationFailure = 5,
        /// Audio frames rendered from ring buffer
        framesRendered = 6,
        /// Ring buffer contained fewer audio frames than requested
        renderBufferUnderrun = 7,
    };

    // MARK: - Event Processing

    /// Peeks and identifies event commands in `events_` for processing
    /// - note: This is the thread entry point for the event processing thread
    void sequenceAndProcessEvents(std::stop_token stoken) noexcept;

    /// Dequeues and processes a decoding started event from `events_`
    bool processDecodingStartedEvent() noexcept;

    /// Dequeues and processes a decoding complete event from `events_`
    bool processDecodingCompleteEvent() noexcept;

    /// Dequeues and processes a decoder seek event from `events_`
    bool processDecoderSeekEvent() noexcept;

    /// Dequeues and processes a decoder canceled event from `events_`
    bool processDecoderCanceledEvent() noexcept;

    /// Dequeues and processes an allocation failure event from `events_`
    bool processAllocationFailureEvent() noexcept;

    /// Dequeues and processes a frames rendered event from `events_`
    bool processFramesRenderedEvent() noexcept;

    /// Reads and processes a render buffer underrun event from `events_`
    bool processRenderBufferUnderrunEvent() noexcept;

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
    std::lock_guard lock{queuedDecodersMutex_};
    queuedDecoders_.clear();
}

inline bool AudioPlayer::decoderQueueIsEmpty() const noexcept {
    std::lock_guard lock{queuedDecodersMutex_};
    return queuedDecoders_.empty();
}

inline SFBAudioPlayerPlaybackState AudioPlayer::playbackState() const noexcept {
    const auto flags = loadFlags();
    const auto state = flags & (Flags::engineIsRunning | Flags::isPlaying);
#if DEBUG
    assert(bits::is_set_or_is_clear(state, Flags::engineIsRunning, Flags::isPlaying));
#endif /* DEBUG */
    return static_cast<SFBAudioPlayerPlaybackState>(state);
}

inline bool AudioPlayer::isPlaying() const noexcept {
    const auto flags = loadFlags();
    return bits::has_all(flags, Flags::engineIsRunning | Flags::isPlaying);
}

inline bool AudioPlayer::isPaused() const noexcept {
    const auto flags = loadFlags();
    return bits::is_set_and_is_clear(flags, Flags::engineIsRunning, Flags::isPlaying);
}

inline bool AudioPlayer::isStopped() const noexcept {
    const auto flags = loadFlags();
    return bits::is_clear(flags, Flags::engineIsRunning);
}

inline bool AudioPlayer::isReady() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};
    return firstActiveDecoderState() != nullptr;
}

inline AudioPlayer::Decoder _Nullable AudioPlayer::nowPlaying() const noexcept {
    std::lock_guard lock{nowPlayingMutex_};
    return nowPlaying_;
}

inline AVAudioSourceNode *_Nonnull AudioPlayer::sourceNode() const noexcept { return sourceNode_; }

inline AVAudioMixerNode *_Nonnull AudioPlayer::mainMixerNode() const noexcept { return engine_.mainMixerNode; }

inline AVAudioOutputNode *_Nonnull AudioPlayer::outputNode() const noexcept { return engine_.outputNode; }

} /* namespace sfb */

#pragma clang diagnostic pop
