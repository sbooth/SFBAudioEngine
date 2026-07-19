//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "AudioPlayer.h"

#import "SFBACLDescription.h"
#import "SFBASBDFormatDescription.h"
#import "SFBAudioDecoder.h"
#import "SFBAudioPlayer+Internal.h"
#import "SFBCStringForOSType.h"
#import "host_time.hpp"

#import <AVFAudioExtensions/AVFAudioExtensions.h>

#import <AudioToolbox/AudioFormat.h>

#import <objc/runtime.h>

#import <algorithm>
#import <atomic>
#import <cmath>
#import <concepts>
#import <limits>
#import <optional>
#import <ranges>

namespace {

/// The default audio ring buffer capacity in frames
constexpr std::size_t audioBufferCapacity = 16384;
/// The default audio metadata buffer capacity in bytes
constexpr std::size_t metadataBufferCapacity = 2048;
/// The minimum number of frames to write to the audio ring buffer
constexpr AVAudioFrameCount ringBufferChunkSize = 2048;

/// The number of nanoseconds in one second
constexpr uint64_t nanosecondsPerSecond = 1'000'000'000;
/// The number of nanoseconds in one millisecond
constexpr uint64_t nanosecondsPerMillisecond = 1'000'000;

/// Objective-C associated object key indicating if a decoder has been canceled
constexpr char decoderIsCanceledKey = '\0';

void audioEngineConfigurationChangeNotificationCallback([[maybe_unused]] CFNotificationCenterRef center, void *observer,
                                                        [[maybe_unused]] CFNotificationName name, const void *object,
                                                        CFDictionaryRef userInfo) {
    auto *that = static_cast<sfb::AudioPlayer *>(observer);
    that->handleAudioEngineConfigurationChange((__bridge AVAudioEngine *)object, (__bridge NSDictionary *)userInfo);
}

#if TARGET_OS_IPHONE
void audioSessionInterruptionNotificationCallback([[maybe_unused]] CFNotificationCenterRef center, void *observer,
                                                  [[maybe_unused]] CFNotificationName name,
                                                  [[maybe_unused]] const void *object, CFDictionaryRef userInfo) {
    auto that = static_cast<sfb::AudioPlayer *>(observer);
    that->handleAudioSessionInterruption((__bridge NSDictionary *)userInfo);
}
#endif /* TARGET_OS_IPHONE */

#if !TARGET_OS_IPHONE
/// Returns the name of `audioUnit.deviceID`
///
/// This is the value of `kAudioObjectPropertyName` in the output scope on the main element
NSString *_Nullable audioDeviceName(AUAudioUnit *_Nonnull audioUnit) noexcept {
#if DEBUG
    assert(audioUnit != nil);
#endif /* DEBUG */

    AudioObjectPropertyAddress address = {.mSelector = kAudioObjectPropertyName,
                                          .mScope = kAudioObjectPropertyScopeOutput,
                                          .mElement = kAudioObjectPropertyElementMain};
    CFStringRef name = nullptr;
    UInt32 dataSize = sizeof(name);
    const auto result = AudioObjectGetPropertyData(audioUnit.deviceID, &address, 0, nullptr, &dataSize, &name);
    if (result != noErr) {
        os_log_error(sfb::AudioPlayer::log_,
                     "AudioObjectGetPropertyData (kAudioObjectPropertyName, kAudioObjectPropertyScopeOutput, "
                     "kAudioObjectPropertyElementMain) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return nil;
    }
    return (__bridge_transfer NSString *)name;
}
#endif /* !TARGET_OS_IPHONE */

/// Returns true if two AudioChannelLayout structures are equivalent.
///
/// Audio channel layouts are considered equivalent if:
/// 1) Both are null.
/// 2) One is null and the other has a mono or stereo layout tag.
/// 3) kAudioFormatProperty_AreChannelLayoutsEquivalent is true.
/// @note Two equivalent channel layouts may not be equal.
/// @return true if the AudioChannelLayout structs are equivalent, false if not.
bool channelLayoutsAreEquivalent(const AudioChannelLayout *lhs, const AudioChannelLayout *rhs) noexcept {
    if (lhs == nullptr && rhs == nullptr) {
        return true;
    }

    if (lhs != nullptr && rhs == nullptr) {
        if (const auto tag = lhs->mChannelLayoutTag;
            tag == kAudioChannelLayoutTag_Mono || tag == kAudioChannelLayoutTag_Stereo) {
            return true;
        }
    } else if (lhs == nullptr && rhs != nullptr) {
        if (const auto tag = rhs->mChannelLayoutTag;
            tag == kAudioChannelLayoutTag_Mono || tag == kAudioChannelLayoutTag_Stereo) {
            return true;
        }
    }

    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }

    const AudioChannelLayout *layouts[] = {
            lhs,
            rhs,
    };
    UInt32 layoutsEquivalent = 0;
    UInt32 propertySize = sizeof layoutsEquivalent;
    OSStatus status = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof layouts,
                                             static_cast<const void *>(layouts), &propertySize, &layoutsEquivalent);
    if (status != noErr) {
        return false;
    }

    return layoutsEquivalent != 0;
}

/// Returns a string describing `format`
NSString *stringDescribingAVAudioFormat(AVAudioFormat *_Nullable format, bool includeChannelLayout = true) noexcept {
    if (format == nil) {
        return nil;
    }

    NSString *formatDescription = SFBASBDFormatDescription(format.streamDescription);
    if (includeChannelLayout) {
        NSString *layoutDescription = SFBACLDescription(format.channelLayout.layout);
        if (layoutDescription == nil) {
            return [NSString stringWithFormat:@"<AVAudioFormat %p: %@ [no channel layout]>", (__bridge void *)format,
                                              formatDescription];
        }
        return [NSString stringWithFormat:@"<AVAudioFormat %p: %@ [%@]>", (__bridge void *)format, formatDescription,
                                          layoutDescription];
    }
    return [NSString stringWithFormat:@"<AVAudioFormat %p: %@>", (__bridge void *)format, formatDescription];
}

/// Performs a generic atomic read-modify-write (RMW) operation
/// - returns: The value before the operation
template <typename T, typename Func>
    requires std::atomic<T>::is_always_lock_free && std::is_trivially_copyable_v<T> && std::invocable<Func, T> &&
             std::convertible_to<std::invoke_result_t<Func, T>, T>
T fetchUpdate(std::atomic<T> &atom, Func &&func,
              std::memory_order order = std::memory_order_seq_cst) noexcept(std::is_nothrow_invocable_v<Func, T> &&
                                                                            std::is_nothrow_copy_constructible_v<T>) {
    T expected = atom.load(std::memory_order_relaxed);
    while (true) {
        const T desired = func(expected);
        if (atom.compare_exchange_weak(expected, desired, order, std::memory_order_relaxed)) {
            return expected;
        }
    }
}

/// Returns the absolute difference between a and b
template <typename T>
    requires std::unsigned_integral<T>
constexpr T absoluteDifference(T a, T b) noexcept {
    return (a >= b) ? (a - b) : (b - a);
}

} /* namespace */

namespace sfb {

const os_log_t AudioPlayer::log_ = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

// MARK: - Decoder State

/// State for tracking/syncing decoding progress
struct AudioPlayer::DecoderState final {
    /// Next sequence number to use
    static std::atomic<uint64_t> sequenceCounter_;

    /// Monotonically increasing instance counter
    const uint64_t sequenceNumber_{sequenceCounter_.fetch_add(1, std::memory_order_relaxed)};

    /// Decodes audio from the source representation to PCM
    const Decoder decoder_{nil};

    /// Possible bits in `flags_`
    enum class Flags : unsigned int {
        /// Decoder state not initialized
        needsInitialization = 1u << 0,
        /// Decoding started
        decodingStarted = 1u << 1,
        /// Decoding complete
        decodingComplete = 1u << 2,
        /// Decoding was resumed after completion
        decodingResumed = 1u << 3,
        /// Decoding was suspended after starting
        decodingSuspended = 1u << 4,
        /// Rendering started
        renderingStarted = 1u << 5,
        /// Decoder cancelation requested
        cancelRequested = 1u << 6,
        /// Decoder canceled
        isCanceled = 1u << 7,
    };

    /// Flags
    std::atomic_uint flags_{bits::to_underlying(Flags::needsInitialization)};
    static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

    /// The number of frames decoded
    std::atomic_int64_t framesDecoded_{0};
    /// The number of frames rendered
    std::atomic_int64_t framesRendered_{0};
    /// The total number of audio frames
    AVAudioFramePosition frameLength_{SFBUnknownFrameLength};
    /// The requested frame
    std::atomic_int64_t requestedFrame_{SFBUnknownFramePosition};

    static_assert(std::atomic_int64_t::is_always_lock_free, "Lock-free std::atomic_int64_t required");

    /// Converts audio from the decoder's processing format to the equivalent standard format
    AVAudioConverter *converter_{nil};
    /// Buffer used internally for buffering during conversion
    AVAudioPCMBuffer *decodeBuffer_{nil};

    /// The sample rate of the audio converter's output format
    double sampleRate_{0};

    /// The error that caused decoding to abort, if any
    NSError *error_{nil};

    // Enable bitmask operations for `Flags`
    friend constexpr void is_bitmask_enum(Flags);

    // Hidden friend
    friend constexpr Flags operator|(Flags l, Flags r) noexcept { return bits::operator|(l, r); }

    /// Atomically loads `flags_` using the specified memory order and returns the result
    [[nodiscard]] Flags loadFlags(std::memory_order order = std::memory_order_acquire) const noexcept {
        return static_cast<Flags>(flags_.load(order));
    }

    /// Atomically sets flags using the specified memory order and returns the previous value
    Flags setFlags(Flags flags, std::memory_order order = std::memory_order_acq_rel) noexcept {
        return static_cast<Flags>(flags_.fetch_or(bits::to_underlying(flags), order));
    }

    /// Atomically clears flags using the specified memory order and returns the previous value
    Flags clearFlags(Flags flags, std::memory_order order = std::memory_order_acq_rel) noexcept {
        return static_cast<Flags>(flags_.fetch_and(~bits::to_underlying(flags), order));
    }

    DecoderState(Decoder _Nonnull decoder) noexcept;

    bool allocate(AVAudioFrameCount frameCapacity) noexcept;

    double sampleRate() const noexcept;

    AVAudioFramePosition framePosition() const noexcept;
    AVAudioFramePosition frameLength() const noexcept;

    bool decodeAudio(AVAudioPCMBuffer *_Nonnull buffer, NSError **error) noexcept;

    /// Sets the pending seek request to `frame`
    void requestSeekToFrame(AVAudioFramePosition frame) noexcept;
    /// Returns `true` if a seek is pending
    bool isSeekRequested() const noexcept;
    /// Performs the pending seek request
    std::optional<AVAudioFramePosition> performSeek(NSError **error) noexcept;
};

std::atomic<uint64_t> AudioPlayer::DecoderState::sequenceCounter_{1};

inline AudioPlayer::DecoderState::DecoderState(Decoder _Nonnull decoder) noexcept : decoder_{decoder} {
#if DEBUG
    assert(decoder != nil);
#endif /* DEBUG */
}

inline bool AudioPlayer::DecoderState::allocate(AVAudioFrameCount frameCapacity) noexcept {
#if DEBUG
    assert(decoder_.isOpen);
    assert(converter_ == nil);
    assert(decodeBuffer_ == nil);
    assert(bits::is_set(loadFlags(), Flags::needsInitialization));
    assert(frameCapacity != 0);
#endif /* DEBUG */

    auto format = decoder_.processingFormat;
    if (format == nil) {
        os_log_error(log_, "Decoder processing format is nil");
        return false;
    }

    if (const auto formatID = format.streamDescription->mFormatID; formatID != kAudioFormatLinearPCM) {
        os_log_error(log_, "Unsupported non-PCM processing format '%{public}.4s'", SFBCStringForOSType(formatID));
        return false;
    }

    const auto sampleRate = format.sampleRate;
    if (sampleRate <= 0 || !std::isfinite(sampleRate)) {
        os_log_error(log_, "Invalid sample rate %g", sampleRate);
        return false;
    }

    auto standardEquivalentFormat = format.standardEquivalent;
    if (standardEquivalentFormat == nil) {
        os_log_error(log_, "Error converting %{public}@ to standard equivalent format",
                     stringDescribingAVAudioFormat(format));
        return false;
    }

    // Convert to deinterleaved native-endian float, preserving the channel count and order
    converter_ = [[AVAudioConverter alloc] initFromFormat:format toFormat:standardEquivalentFormat];
    if (converter_ == nil) {
        os_log_error(log_, "Error creating AVAudioConverter converting from %{public}@ to %{public}@",
                     stringDescribingAVAudioFormat(format), stringDescribingAVAudioFormat(standardEquivalentFormat));
        return false;
    }

    decodeBuffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter_.inputFormat frameCapacity:frameCapacity];
    if (decodeBuffer_ == nil) {
        os_log_error(log_, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %u",
                     stringDescribingAVAudioFormat(converter_.inputFormat), frameCapacity);
        return false;
    }

    const auto framePosition = decoder_.framePosition;
    if (framePosition == SFBUnknownFramePosition) {
        os_log_error(log_, "Unknown frame position in %{public}@", decoder_);
        return false;
    }

    if (framePosition != 0) {
        framesDecoded_.store(framePosition, std::memory_order_release);
        framesRendered_.store(framePosition, std::memory_order_release);
    }

    // The sample rate and frame length do not need to be individually atomic because they are written only once
    // and access is guarded behind the atomic flag `Flags::needsInitialization`
    sampleRate_ = sampleRate;
    frameLength_ = decoder_.frameLength;

    clearFlags(Flags::needsInitialization);

    return true;
}

inline double AudioPlayer::DecoderState::sampleRate() const noexcept { return sampleRate_; }

inline AVAudioFramePosition AudioPlayer::DecoderState::framePosition() const noexcept {
    if (const auto requestedFrame = requestedFrame_.load(std::memory_order_acquire);
        requestedFrame != SFBUnknownFramePosition) {
        return requestedFrame;
    }
    return framesRendered_.load(std::memory_order_acquire);
}

inline AVAudioFramePosition AudioPlayer::DecoderState::frameLength() const noexcept { return frameLength_; }

inline bool AudioPlayer::DecoderState::decodeAudio(AVAudioPCMBuffer *_Nonnull buffer, NSError **error) noexcept {
#if DEBUG
    assert(buffer != nil);
    assert(buffer.frameCapacity == decodeBuffer_.frameCapacity);
#endif /* DEBUG */

    if (![decoder_ decodeIntoBuffer:decodeBuffer_ frameLength:decodeBuffer_.frameCapacity error:error]) {
        return false;
    }

    const auto framesDecoded = decodeBuffer_.frameLength;
    if (framesDecoded == 0) {
        setFlags(Flags::decodingComplete);

#if false
        // Some formats may not know the exact number of frames in advance
        // without processing the entire file, which is a potentially slow operation
        frameLength_.store(mDecoder.framePosition, std::memory_order_release);
#endif /* false */

        buffer.frameLength = 0;
        return true;
    }

    this->framesDecoded_.fetch_add(framesDecoded, std::memory_order_acq_rel);

    // Only PCM to PCM conversions are performed
    if (![converter_ convertToBuffer:buffer fromBuffer:decodeBuffer_ error:error]) {
        return false;
    }
#if DEBUG
    assert(framesDecoded == buffer.frameLength);
#endif /* DEBUG */

    // If `buffer` is not full but -decodeIntoBuffer:frameLength:error: returned `YES`
    // decoding is complete
    if (buffer.frameLength != buffer.frameCapacity) {
        setFlags(Flags::decodingComplete);
    }

    return true;
}

/// Sets the pending seek request to `frame`
inline void AudioPlayer::DecoderState::requestSeekToFrame(AVAudioFramePosition frame) noexcept {
#if DEBUG
    assert(frame != SFBUnknownFramePosition);
    assert(frame >= 0);
#endif /* DEBUG */
    requestedFrame_.store(frame, std::memory_order_release);
}

inline bool AudioPlayer::DecoderState::isSeekRequested() const noexcept {
    return requestedFrame_.load(std::memory_order_acquire) != SFBUnknownFramePosition;
}

/// Performs the pending seek request
inline std::optional<AVAudioFramePosition> AudioPlayer::DecoderState::performSeek(NSError **error) noexcept {
    const auto requestedFrame = requestedFrame_.load(std::memory_order_acquire);
#if DEBUG
    assert(requestedFrame != SFBUnknownFramePosition);
#endif /* DEBUG */

    os_log_debug(log_, "Seeking to frame %lld in %{public}@", requestedFrame, decoder_);

    const auto clearSeekRequest = [&]() noexcept {
        // Don't overwrite a newer seek request
        auto previousRequestedFrame = requestedFrame;
        requestedFrame_.compare_exchange_strong(previousRequestedFrame, SFBUnknownFramePosition,
                                                std::memory_order_acq_rel, std::memory_order_relaxed);
    };

    if (NSError *seekError = nil; ![decoder_ seekToFrame:requestedFrame error:&seekError]) {
        os_log_error(log_, "Error seeking to frame %lld in %{public}@", requestedFrame, decoder_);
        if (error != nullptr) {
            *error = seekError;
        }
        clearSeekRequest();
        return std::nullopt;
    }

    // Reset the converter to flush any buffers
    [converter_ reset];

    // Clear the seek request
    clearSeekRequest();

    const auto framePosition = decoder_.framePosition;
    if (framePosition == SFBUnknownFramePosition) {
        os_log_error(log_, "Unknown frame position in %{public}@ after seeking to frame %lld", decoder_,
                     requestedFrame);
        return std::nullopt;
    }
    if (framePosition != requestedFrame) {
        os_log_info(log_, "Inaccurate seek to frame %lld, got %lld", requestedFrame, framePosition);
    }

    return framePosition;
}

} /* namespace sfb */

// MARK: - AudioPlayer

sfb::AudioPlayer::AudioPlayer() {
    // ========================================
    // Rendering Setup

    // Start out with 44.1 kHz stereo
    AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
    if (format == nil) {
        os_log_error(log_, "Unable to create AVAudioFormat for 44.1 kHz stereo");
        throw std::runtime_error("Unable to create AVAudioFormat");
    }

    // Allocate the audio buffer carrying audio from the decoder thread to the render block
    if (!audioBuffer_.allocate(*(format.streamDescription), audioBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create audio buffer: spsc::AudioRingBuffer::allocate failed with format "
                     "%{public}@ and capacity %zu",
                     SFBASBDFormatDescription(format.streamDescription), audioBufferCapacity);
        throw std::runtime_error("spsc::AudioRingBuffer::allocate failed");
    }

    // Allocate the metadata buffer carrying decoded chunk descriptors from the decoder thread to the render block
    if (!audioMetadata_.allocate(metadataBufferCapacity)) {
        os_log_error(log_, "Unable to create metadata buffer: spsc::RingBuffer::allocate failed with capacity %zu",
                     metadataBufferCapacity);
        throw std::runtime_error("spsc::RingBuffer::allocate failed");
    }

    // ========================================
    // Event Processing Setup

    // Create the dispatch queue used for asynchronous event processing
    auto attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
    if (attr == nullptr) {
        os_log_error(log_, "dispatch_queue_attr_make_with_qos_class failed");
        throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
    }

    eventQueue_ = dispatch_queue_create_with_target("AudioPlayer.Events", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
    if (eventQueue_ == nullptr) {
        os_log_error(log_, "Unable to create event dispatch queue: dispatch_queue_create failed");
        throw std::runtime_error("dispatch_queue_create_with_target failed");
    }

    // Launch the decoding and event processing threads
    try {
        decodingThread_ = std::jthread(std::bind_front(&sfb::AudioPlayer::processDecoders, this));
        eventThread_ = std::jthread(std::bind_front(&sfb::AudioPlayer::sequenceAndProcessEvents, this));
    } catch (const std::exception &e) {
        os_log_error(log_, "Unable to create thread: %{public}s", e.what());
        throw;
    }

    // ========================================
    // Audio Processing Graph Setup

    engine_ = [[AVAudioEngine alloc] init];
    if (engine_ == nil) {
        os_log_error(log_, "Unable to create AVAudioEngine instance");
        throw std::runtime_error("Unable to create AVAudioEngine");
    }

    sourceNode_ = [[AVAudioSourceNode alloc]
            initWithRenderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                                          AVAudioFrameCount frameCount, AudioBufferList *outputData) {
                return render(*isSilence, *timestamp, frameCount, outputData);
            }];
    if (sourceNode_ == nil) {
        throw std::runtime_error("Unable to create AVAudioSourceNode instance");
    }

    [engine_ attachNode:sourceNode_];
    [engine_ connect:sourceNode_ to:engine_.mainMixerNode format:format];
    [engine_ prepare];

#if DEBUG
    logProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

    // Register for configuration change notifications
    auto *notificationCenter = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterAddObserver(notificationCenter, this, audioEngineConfigurationChangeNotificationCallback,
                                    (__bridge CFStringRef)AVAudioEngineConfigurationChangeNotification,
                                    (__bridge void *)engine_, CFNotificationSuspensionBehaviorDeliverImmediately);

#if TARGET_OS_IPHONE
    // Register for audio session interruption notifications
    CFNotificationCenterAddObserver(notificationCenter, this, audioSessionInterruptionNotificationCallback,
                                    (__bridge CFStringRef)AVAudioSessionInterruptionNotification,
                                    (__bridge void *)[AVAudioSession sharedInstance],
                                    CFNotificationSuspensionBehaviorDeliverImmediately);
#endif /* TARGET_OS_IPHONE */
}

sfb::AudioPlayer::~AudioPlayer() noexcept {
    auto *notificationCenter = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterRemoveEveryObserver(notificationCenter, this);

    {
        std::lock_guard lock{engineMutex_};
        [engine_ stop];
        clearFlags(Flags::engineIsRunning | Flags::isPlaying);
    }

    clearDecoderQueue();
    cancelActiveDecoders();

    // Register a stop callback for the decoding thread
    std::stop_callback decodingThreadStopCallback(decodingThread_.get_stop_token(),
                                                  [this]() noexcept { decodingSemaphore_.signal(); });

    // Issue a stop request to the decoding thread and wait for it to exit
    decodingThread_.request_stop();
    try {
        decodingThread_.join();
    } catch (const std::exception &e) {
        os_log_error(log_, "Unable to join decoding thread: %{public}s", e.what());
    }

    // Register a stop callback for the event processing thread
    std::stop_callback eventThreadStopCallback(eventThread_.get_stop_token(),
                                               [this]() noexcept { eventSemaphore_.signal(); });

    // Issue a stop request to the event processing thread and wait for it to exit
    eventThread_.request_stop();
    try {
        eventThread_.join();
    } catch (const std::exception &e) {
        os_log_error(log_, "Unable to join event processing thread: %{public}s", e.what());
    }

    // Delete any remaining decoder state
    activeDecoders_.clear();

    os_log_debug(log_, "<AudioPlayer: %p> destroyed", this);
}

// MARK: - Playlist Management

bool sfb::AudioPlayer::enqueueDecoder(Decoder decoder, bool forImmediatePlayback, NSError **error) noexcept {
#if DEBUG
    assert(decoder != nil);
#endif /* DEBUG */

    // Ensure only one decoder can be enqueued at a time
    std::lock_guard lock{queuedDecodersMutex_};

    if (forImmediatePlayback) {
        queuedDecoders_.clear();
    }

    try {
        queuedDecoders_.push_back(decoder);
    } catch (const std::exception &e) {
        os_log_error(log_, "Error pushing %{public}@ to queuedDecoders_: %{public}s", decoder, e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return false;
    }

    os_log_info(log_, "Enqueued %{public}@", decoder);

    if (forImmediatePlayback) {
        cancelActiveDecoders();
        // Mute until the decoder becomes active
        setFlags(Flags::isMuted);
    }

    decodingSemaphore_.signal();

    return true;
}

bool sfb::AudioPlayer::formatWillBeGaplessIfEnqueued(AVAudioFormat *format) const noexcept {
#if DEBUG
    assert(format != nil);
#endif /* DEBUG */
    // Gapless playback requires the same number of channels at the same sample rate with the same channel layout
    auto renderFormat = [sourceNode_ outputFormatForBus:0];
    return format.channelCount == renderFormat.channelCount && format.sampleRate == renderFormat.sampleRate &&
           channelLayoutsAreEquivalent(format.channelLayout.layout, renderFormat.channelLayout.layout);
}

// MARK: - Playback Control

bool sfb::AudioPlayer::play(NSError **error) noexcept {
    auto didStartEngine = false;
    auto wasPlaying = false;
    {
        std::lock_guard lock{engineMutex_};
        if (didStartEngine = !engine_.isRunning; didStartEngine) {
            if (NSError *startError = nil; ![engine_ startAndReturnError:&startError]) {
                os_log_error(log_, "Error starting AVAudioEngine: %{public}@", startError);
                clearFlags(Flags::engineIsRunning | Flags::isPlaying);
                if (error != nullptr) {
                    *error = startError;
                }
                return false;
            }
        }

        const auto prevFlags = setFlags(Flags::engineIsRunning | Flags::isPlaying);
        wasPlaying = bits::is_set(prevFlags, Flags::isPlaying);
#if DEBUG
        assert(!(didStartEngine && wasPlaying));
#endif /* DEBUG */
    }

    if (didStartEngine || !wasPlaying) {
        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
        }
    }

    return true;
}

bool sfb::AudioPlayer::pause() noexcept {
    auto wasPlaying = false;
    {
        std::lock_guard lock{engineMutex_};
        if (!engine_.isRunning) {
            return false;
        }
        const auto prevFlags = clearFlags(Flags::isPlaying);
        wasPlaying = bits::is_set(prevFlags, Flags::isPlaying);
    }

    if (wasPlaying) {
        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
        }
    }

    return true;
}

bool sfb::AudioPlayer::resume() noexcept {
    auto wasPaused = false;
    {
        std::lock_guard lock{engineMutex_};
        if (!engine_.isRunning) {
            return false;
        }
        const auto prevFlags = setFlags(Flags::isPlaying);
        wasPaused = bits::is_clear(prevFlags, Flags::isPlaying);
    }

    if (wasPaused) {
        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
        }
    }

    return true;
}

void sfb::AudioPlayer::stop() noexcept {
    const auto didStopEngine = stopEngineIfRunning();

    clearDecoderQueue();
    cancelActiveDecoders();

    if (didStopEngine) {
        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
        }
    }
}

bool sfb::AudioPlayer::togglePlayPause(NSError **error) noexcept {
    SFBAudioPlayerPlaybackState playbackState;
    {
        std::lock_guard lock{engineMutex_};

        // Currently stopped, transition to playing
        if (!engine_.isRunning) {
            if (NSError *startError = nil; ![engine_ startAndReturnError:&startError]) {
                os_log_error(log_, "Error starting AVAudioEngine: %{public}@", startError);
                clearFlags(Flags::engineIsRunning | Flags::isPlaying);
                if (error != nullptr) {
                    *error = startError;
                }
                return false;
            }

            [[maybe_unused]] const auto prevFlags = setFlags(Flags::engineIsRunning | Flags::isPlaying);
#if DEBUG
            assert(bits::is_clear(prevFlags, Flags::isPlaying));
#endif /* DEBUG */

            playbackState = SFBAudioPlayerPlaybackStatePlaying;
        } else {
            // Toggle playing/paused
            const auto prevFlags = toggleFlags(Flags::isPlaying);
            if (bits::is_set(prevFlags, Flags::isPlaying)) {
                playbackState = SFBAudioPlayerPlaybackStatePaused;
            } else {
                playbackState = SFBAudioPlayerPlaybackStatePlaying;
            }
        }
    }

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
        [delegate audioPlayer:player_ playbackStateChanged:playbackState];
    }

    return true;
}

void sfb::AudioPlayer::reset() noexcept {
    {
        std::lock_guard lock{engineMutex_};
        [engine_ reset];
    }
    clearDecoderQueue();
    cancelActiveDecoders();
}

// MARK: - Player State

bool sfb::AudioPlayer::engineIsRunning() const noexcept {
    const auto isRunning = engine_.isRunning;
#if DEBUG
    assert(bits::is_set(loadFlags(), Flags::engineIsRunning) == isRunning &&
           "Cached value for engine_.isRunning invalid");
#endif /* DEBUG */
    return isRunning;
}

sfb::AudioPlayer::Decoder sfb::AudioPlayer::currentDecoder() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};
    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        return nil;
    }
    return decoderState->decoder_;
}

void sfb::AudioPlayer::setNowPlaying(Decoder nowPlaying) noexcept {
    {
        std::lock_guard lock{nowPlayingMutex_};
        if (nowPlaying_ == nowPlaying) {
            return;
        }
        nowPlaying_ = nowPlaying;
    }

    os_log_debug(log_, "Now playing changed to %{public}@", nowPlaying);

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:)]) {
        [delegate audioPlayer:player_ nowPlayingChanged:nowPlaying];
    }
}

// MARK: - Playback Properties

SFBPlaybackPosition sfb::AudioPlayer::playbackPosition() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};
    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        return SFBInvalidPlaybackPosition;
    }
    return {.framePosition = decoderState->framePosition(), .frameLength = decoderState->frameLength()};
}

SFBPlaybackTime sfb::AudioPlayer::playbackTime() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        return SFBInvalidPlaybackTime;
    }

    SFBPlaybackTime playbackTime = SFBInvalidPlaybackTime;

    const auto framePosition = decoderState->framePosition();
    const auto frameLength = decoderState->frameLength();

    const auto sampleRate = decoderState->sampleRate();
    if (framePosition != SFBUnknownFramePosition) {
        playbackTime.currentTime = framePosition / sampleRate;
    }
    if (frameLength != SFBUnknownFrameLength) {
        playbackTime.totalTime = frameLength / sampleRate;
    }

    return playbackTime;
}

bool sfb::AudioPlayer::getPlaybackPositionAndTime(SFBPlaybackPosition *playbackPosition,
                                                  SFBPlaybackTime *playbackTime) const noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        if (playbackPosition != nullptr) {
            *playbackPosition = SFBInvalidPlaybackPosition;
        }
        if (playbackTime != nullptr) {
            *playbackTime = SFBInvalidPlaybackTime;
        }
        return false;
    }

    SFBPlaybackPosition currentPlaybackPosition = {.framePosition = decoderState->framePosition(),
                                                   .frameLength = decoderState->frameLength()};
    if (playbackPosition != nullptr) {
        *playbackPosition = currentPlaybackPosition;
    }

    if (playbackTime != nullptr) {
        SFBPlaybackTime currentPlaybackTime = SFBInvalidPlaybackTime;
        const auto sampleRate = decoderState->sampleRate();
        if (currentPlaybackPosition.framePosition != SFBUnknownFramePosition) {
            currentPlaybackTime.currentTime = currentPlaybackPosition.framePosition / sampleRate;
        }
        if (currentPlaybackPosition.frameLength != SFBUnknownFrameLength) {
            currentPlaybackTime.totalTime = currentPlaybackPosition.frameLength / sampleRate;
        }
        *playbackTime = currentPlaybackTime;
    }

    return true;
}

// MARK: - Seeking

bool sfb::AudioPlayer::seekInTime(NSTimeInterval secondsToSkip) noexcept {
    if (!std::isfinite(secondsToSkip)) {
        return false;
    }

    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    if (secondsToSkip == 0) {
        return true;
    }

    const auto framesToSkip = secondsToSkip * decoderState->sampleRate();
    if (framesToSkip >= static_cast<double>(std::numeric_limits<AVAudioFramePosition>::max()) ||
        framesToSkip <= static_cast<double>(std::numeric_limits<AVAudioFramePosition>::min())) {
        return false;
    }

    return performClampingSeekToFrame(decoderState, static_cast<AVAudioFramePosition>(framesToSkip), true);
}

bool sfb::AudioPlayer::seekToTime(NSTimeInterval timeInSeconds) noexcept {
    if (timeInSeconds < 0 || !std::isfinite(timeInSeconds)) {
        return false;
    }

    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    const auto requestedFrame = timeInSeconds * decoderState->sampleRate();
    if (requestedFrame >= static_cast<double>(std::numeric_limits<AVAudioFramePosition>::max())) {
        return false;
    }

    return performClampingSeekToFrame(decoderState, static_cast<AVAudioFramePosition>(requestedFrame), false);
}

bool sfb::AudioPlayer::seekToPosition(double position) noexcept {
    if (!std::isfinite(position)) {
        return false;
    }

    position = std::clamp(position, 0.0, std::nextafter(1.0, 0.0));

    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    const auto frameLength = decoderState->frameLength();
    if (frameLength == SFBUnknownFrameLength || frameLength < 1) {
        return false;
    }

    const auto targetFrame = static_cast<AVAudioFramePosition>(frameLength * position);

    decoderState->requestSeekToFrame(targetFrame);
    decodingSemaphore_.signal();

    return true;
}

bool sfb::AudioPlayer::seekToFrame(AVAudioFramePosition frame) noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    return performClampingSeekToFrame(decoderState, frame, false);
}

bool sfb::AudioPlayer::supportsSeeking() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};
    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        return false;
    }
    return decoderState->decoder_.supportsSeeking;
}

bool sfb::AudioPlayer::performClampingSeekToFrame(DecoderState *decoderState, AVAudioFramePosition frame,
                                                  bool isRelative) noexcept {
#if DEBUG
    activeDecodersMutex_.assertIsOwner();
    assert(decoderState != nullptr);
#endif /* DEBUG */

    const auto framePosition = decoderState->framePosition();
    if (framePosition == SFBUnknownFramePosition) {
        return false;
    }

    // Require a valid frame length even though not strictly required for seeking in general
    const auto frameLength = decoderState->frameLength();
    if (frameLength == SFBUnknownFrameLength || frameLength < 1) {
        return false;
    }

    if (isRelative) {
        if (frame > 0 && framePosition > std::numeric_limits<AVAudioFramePosition>::max() - frame) {
            return false;
        }

        if (frame < 0 && framePosition < std::numeric_limits<AVAudioFramePosition>::min() - frame) {
            return false;
        }

        frame += framePosition;
    }

    frame = std::clamp(frame, 0LL, frameLength - 1);

    if (framePosition != frame) {
        decoderState->requestSeekToFrame(frame);
        decodingSemaphore_.signal();
    }

    return true;
}

#if !TARGET_OS_IPHONE

// MARK: - Volume Control

float sfb::AudioPlayer::volumeForChannel(AudioObjectPropertyElement channel) const noexcept {
    AudioUnitParameterValue volume;
    const auto result = AudioUnitGetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume,
                                              kAudioUnitScope_Global, channel, &volume);
    if (result != noErr) {
        os_log_error(
                log_,
                "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'",
                channel, result, SFBCStringForOSType(result));
        return std::nanf("1");
    }

    return volume;
}

bool sfb::AudioPlayer::setVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept {
    os_log_info(log_, "Setting volume for channel %u to %g", channel, volume);

    const auto result = AudioUnitSetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume,
                                              kAudioUnitScope_Global, channel, volume, 0);
    if (result != noErr) {
        os_log_error(
                log_,
                "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'",
                channel, result, SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        }
        return false;
    }

    return true;
}

// MARK: - Output Device

AUAudioObjectID sfb::AudioPlayer::outputDeviceID() const noexcept { return engine_.outputNode.AUAudioUnit.deviceID; }

bool sfb::AudioPlayer::setOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept {
    os_log_info(log_, "Setting <AudioPlayer: %p> output device to 0x%x", this, outputDeviceID);

    if (NSError *err = nil; ![engine_.outputNode.AUAudioUnit setDeviceID:outputDeviceID error:&err]) {
        os_log_error(log_, "Error setting output device: %{public}@", err);
        if (error != nullptr) {
            *error = err;
        }
        return false;
    }

    return true;
}

#endif /* !TARGET_OS_IPHONE */

// MARK: - AVAudioEngine

void sfb::AudioPlayer::modifyProcessingGraph(void (^block)(AVAudioEngine *engine)) const noexcept {
#if DEBUG
    assert(block != nil);
#endif /* DEBUG */

    std::lock_guard lock{engineMutex_};
    block(engine_);

    assert([engine_ inputConnectionPointForNode:engine_.outputNode inputBus:0].node == engine_.mainMixerNode &&
           "Illegal AVAudioEngine configuration");
}

// MARK: - Debugging

void sfb::AudioPlayer::logProcessingGraphDescription(os_log_t log, os_log_type_t type) const noexcept {
    if (!os_log_type_enabled(log, type)) {
        return;
    }

    NSMutableString *string = [NSMutableString
            stringWithFormat:@"<AudioPlayer: %p> audio processing graph:\n", static_cast<const void *>(this)];

    const auto engine = engine_;
    const auto sourceNode = sourceNode_;

    AVAudioFormat *inputFormat = nil;
    AVAudioFormat *outputFormat = [sourceNode outputFormatForBus:0];
    [string appendFormat:@"→ %@\n    %@\n", sourceNode, stringDescribingAVAudioFormat(outputFormat)];

    AVAudioConnectionPoint *connectionPoint = [[engine outputConnectionPointsForNode:sourceNode
                                                                           outputBus:0] firstObject];
    while (connectionPoint.node != engine.mainMixerNode) {
        inputFormat = [connectionPoint.node inputFormatForBus:connectionPoint.bus];
        outputFormat = [connectionPoint.node outputFormatForBus:connectionPoint.bus];
        if (![outputFormat isEqual:inputFormat]) {
            [string appendFormat:@"→ %@\n    %@\n", connectionPoint.node, stringDescribingAVAudioFormat(outputFormat)];
        } else {
            [string appendFormat:@"→ %@\n", connectionPoint.node];
        }

        connectionPoint = [[engine outputConnectionPointsForNode:connectionPoint.node outputBus:0] firstObject];
    }

    inputFormat = [engine.mainMixerNode inputFormatForBus:0];
    outputFormat = [engine.mainMixerNode outputFormatForBus:0];
    if (![outputFormat isEqual:inputFormat]) {
        [string appendFormat:@"→ %@\n    %@\n", engine.mainMixerNode, stringDescribingAVAudioFormat(outputFormat)];
    } else {
        [string appendFormat:@"→ %@\n", engine.mainMixerNode];
    }

    inputFormat = [engine.outputNode inputFormatForBus:0];
    outputFormat = [engine.outputNode outputFormatForBus:0];
    if (![outputFormat isEqual:inputFormat]) {
        [string appendFormat:@"→ %@\n    %@]", engine.outputNode, stringDescribingAVAudioFormat(outputFormat)];
    } else {
        [string appendFormat:@"→ %@", engine.outputNode];
    }

#if !TARGET_OS_IPHONE
    [string appendFormat:@"\n↓ \"%@\"", audioDeviceName(engine.outputNode.AUAudioUnit)];
#endif /* !TARGET_OS_IPHONE */

    os_log_with_type(log, type, "%{public}@", string);
}

// MARK: - Decoding

void sfb::AudioPlayer::processDecoders(std::stop_token stoken) noexcept {
    pthread_setname_np("AudioPlayer.Decoding");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

    os_log_debug(log_, "<AudioPlayer: %p> decoding thread starting", this);

    // The buffer between the decoder state and the ring buffer
    AVAudioPCMBuffer *buffer = nil;
    // Whether there is a mismatch between the rendering format and the next decoder's processing format
    auto formatMismatch = false;

    /// Sets the decoder state's error and cancellation request flag
    const auto setErrorAndRequestCancel = [](DecoderState *_Nonnull decoderState, NSError *_Nonnull error) noexcept {
        decoderState->error_ = error;
        decoderState->setFlags(DecoderState::Flags::cancelRequested);
    };

    while (!stoken.stop_requested()) {
        // The decoder state being processed
        DecoderState *decoderState = nullptr;

        {
            std::lock_guard lock{activeDecodersMutex_};

            // Process cancellations
            auto signal = false;
            auto anyCanceled = false;

            for (const auto &decoderState : activeDecoders_) {
                const auto decoderFlags = decoderState->loadFlags();
                if (bits::is_set_or_is_clear(decoderFlags, DecoderState::Flags::isCanceled,
                                             DecoderState::Flags::cancelRequested)) {
                    continue;
                }

                if (decoderState->error_ == nil) {
                    os_log_debug(log_, "Canceling decoding for %{public}@", decoderState->decoder_);
                } else {
                    os_log_error(log_, "Aborting decoding for %{public}@ due to error", decoderState->decoder_);
                }

                if (bits::is_set(decoderFlags, DecoderState::Flags::decodingStarted)) {
                    // Drain the ring buffer since the decoder could have contributed stale frames
                    setFlags(Flags::drainRequired);

                    // Increment the playback epoch to expire any inflight events
                    playbackGeneration_.fetch_add(1, std::memory_order_acq_rel);
                }

                decoderState->setFlags(DecoderState::Flags::isCanceled);
                anyCanceled = true;

                // Submit the decoder canceled event
                if (events_.enqueue(EventCommand::decoderCanceled, decoderState->sequenceNumber_)) {
                    signal = true;
                } else {
                    os_log_fault(log_, "Error writing decoder canceled event");
                }
            }

            // Signal the event thread if any decoders were canceled
            if (signal) {
                eventSemaphore_.signal();
            }

            // Clear the format mismatch flag if any decoders were canceled
            if (anyCanceled && formatMismatch) {
                formatMismatch = false;
            }

            // Get the earliest decoder state that has not completed rendering
            decoderState = firstActiveDecoderState();
        }

        // Process pending seeks
        if (decoderState != nullptr && decoderState->isSeekRequested()) {
            NSError *seekError = nil;
            const auto framePosition = decoderState->performSeek(&seekError);
            if (!framePosition.has_value()) {
                setErrorAndRequestCancel(decoderState, seekError);
                continue;
            }

            // Mute until the seek is complete and the ring buffer is drained and refilled
            setFlags(Flags::isMuted | Flags::drainRequired);

            // Increment the playback epoch to expire any inflight events
            playbackGeneration_.fetch_add(1, std::memory_order_acq_rel);

            decoderState->framesDecoded_.store(framePosition.value(), std::memory_order_release);
            if (events_.enqueue(EventCommand::seek, decoderState->sequenceNumber_, framePosition.value())) {
                eventSemaphore_.signal();
            } else {
                os_log_fault(log_, "Error writing decoder seek event");
            }

            if (bits::is_set(decoderState->loadFlags(), DecoderState::Flags::decodingComplete)) {
                os_log_debug(log_, "Resuming decoding for %{public}@", decoderState->decoder_);

                // The decoder has not completed rendering so the ring buffer format and the decoder's format still
                // match. Clear the format mismatch flag so rendering can continue; the flag will be set again when
                // decoding completes.
                formatMismatch = false;

                fetchUpdate(
                        decoderState->flags_,
                        [](auto val) noexcept {
                            return (val & ~bits::to_underlying(DecoderState::Flags::decodingComplete)) |
                                   bits::to_underlying(DecoderState::Flags::decodingResumed);
                        },
                        std::memory_order_acq_rel);

                {
                    std::lock_guard lock{activeDecodersMutex_};

                    // Rewind ensuing decoder states if possible to avoid discarding frames
                    for (const auto &nextDecoderState : activeDecoders_) {
                        if (nextDecoderState->sequenceNumber_ <= decoderState->sequenceNumber_) {
                            continue;
                        }

                        const auto nextDecoderFlags = nextDecoderState->loadFlags();
                        if (bits::is_set(nextDecoderFlags, DecoderState::Flags::isCanceled)) {
                            continue;
                        }

                        if (bits::is_set(nextDecoderFlags, DecoderState::Flags::decodingStarted)) {
                            os_log_debug(log_, "Suspending decoding for %{public}@", nextDecoderState->decoder_);

                            // TODO: Investigate a per-state buffer to mitigate frame loss
                            if (nextDecoderState->decoder_.supportsSeeking) {
                                nextDecoderState->requestSeekToFrame(0);

                                NSError *seekError = nil;
                                const auto framePosition = nextDecoderState->performSeek(&seekError);
                                if (!framePosition.has_value()) {
                                    setErrorAndRequestCancel(decoderState, seekError);
                                    continue;
                                }

                                nextDecoderState->framesDecoded_.store(framePosition.value(),
                                                                       std::memory_order_release);
                                if (events_.enqueue(EventCommand::seek, nextDecoderState->sequenceNumber_,
                                                    framePosition.value())) {
                                    eventSemaphore_.signal();
                                } else {
                                    os_log_fault(log_, "Error writing decoder seek event");
                                }
                            } else {
                                os_log_error(log_, "Discarding %lld frames from %{public}@",
                                             nextDecoderState->framesDecoded_.load(std::memory_order_acquire),
                                             nextDecoderState->decoder_);
                            }

                            fetchUpdate(
                                    nextDecoderState->flags_,
                                    [](auto val) noexcept {
                                        return (val & ~bits::to_underlying(DecoderState::Flags::decodingStarted)) |
                                               bits::to_underlying(DecoderState::Flags::decodingSuspended);
                                    },
                                    std::memory_order_acq_rel);
                        }
                    }
                }
            }
        }

        // Get the earliest decoder state that has not completed decoding
        {
            std::lock_guard lock{activeDecodersMutex_};

            const auto iter = std::ranges::find_if(activeDecoders_, [](const auto &decoderState) noexcept {
                const auto decoderFlags = decoderState->loadFlags();
                return bits::has_none(decoderFlags,
                                      DecoderState::Flags::isCanceled | DecoderState::Flags::decodingComplete);
            });

            decoderState = iter != activeDecoders_.cend() ? iter->get() : nullptr;
        }

        // Dequeue the next decoder if there are no decoders that haven't completed decoding
        if (decoderState == nullptr) {
            {
                // Lock both mutexes to ensure a decoder doesn't momentarily "disappear"
                // when transitioning from queued to active
                std::scoped_lock lock{queuedDecodersMutex_, activeDecodersMutex_};

                if (!queuedDecoders_.empty()) {
                    // Remove the first decoder from the decoder queue
                    auto decoder = queuedDecoders_.front();
                    queuedDecoders_.pop_front();

                    // Create the decoder state and add it to the list of active decoders
                    try {
                        activeDecoders_.push_back(std::make_unique<DecoderState>(decoder));
#if DEBUG
                        assert(std::ranges::is_sorted(activeDecoders_, std::ranges::less{},
                                                      &DecoderState::sequenceNumber_));
#endif /* DEBUG */
                        decoderState = activeDecoders_.back().get();
                    } catch (const std::exception &e) {
                        os_log_error(log_, "Error allocating decoder state for %{public}@: %{public}s", decoder,
                                     e.what());
                        if (events_.enqueue(EventCommand::allocationFailure)) {
                            eventSemaphore_.signal();
                        } else {
                            os_log_fault(log_, "Error writing allocation failure event");
                        }
                        continue;
                    }
                }
            }

            if (decoderState != nullptr) {
                // Open the decoder if necessary
                if (!decoderState->decoder_.isOpen) {
                    if (NSError *error = nil; ![decoderState->decoder_ openReturningError:&error]) {
                        os_log_error(log_, "Error opening %{public}@: %{public}@", decoderState->decoder_, error);
                        setErrorAndRequestCancel(decoderState, error);
                        continue;
                    }

                    // Short-circuit processing if the decoder was canceled during open
                    if (bits::is_set(decoderState->loadFlags(), DecoderState::Flags::cancelRequested)) {
                        continue;
                    }
                }

                // Allocate decoder state internals
                if (!decoderState->allocate(ringBufferChunkSize)) {
                    os_log_error(log_,
                                 "Error allocating decoder state data: DecoderStateData::allocate failed with frame "
                                 "capacity %u",
                                 ringBufferChunkSize);
                    setErrorAndRequestCancel(decoderState, [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                               code:SFBAudioPlayerErrorCodeInternalError
                                                                           userInfo:nil]);
                    continue;
                }

                os_log_debug(log_, "Dequeued %{public}@", decoderState->decoder_);
            }
        }

        if (decoderState != nullptr) {
            // Before decoding starts determine the decoder and ring buffer format compatibility
            if (bits::is_clear(decoderState->loadFlags(), DecoderState::Flags::decodingStarted)) {
                // Start decoding immediately if the join will be gapless (same sample rate, channel count, and channel
                // layout)
                if (auto renderFormat = decoderState->converter_.outputFormat;
                    [renderFormat isEqual:[sourceNode_ outputFormatForBus:0]]) {
                    // Allocate the buffer that is the intermediary between the decoder state and the ring buffer
                    if (auto format = buffer.format; format.channelCount != renderFormat.channelCount ||
                                                     format.sampleRate != renderFormat.sampleRate) {
                        buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderFormat
                                                               frameCapacity:ringBufferChunkSize];
                        if (buffer == nil) {
                            os_log_error(log_,
                                         "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %u",
                                         stringDescribingAVAudioFormat(renderFormat), ringBufferChunkSize);
                            setErrorAndRequestCancel(decoderState,
                                                     [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                         code:SFBAudioPlayerErrorCodeInternalError
                                                                     userInfo:nil]);
                            continue;
                        }
                    }
                } else {
                    // If the next decoder cannot be gaplessly joined set the mismatch flag and wait;
                    // decoding can't start until the processing graph is reconfigured which occurs after
                    // all active decoders complete
                    formatMismatch = true;
                }
            }

            // If there is a format mismatch the processing graph requires reconfiguration before decoding can begin
            if (formatMismatch) {
                // Wait until all other decoders complete processing before reconfiguring the graph
                const auto okToReconfigure = [&]() noexcept {
                    std::lock_guard lock{activeDecodersMutex_};
                    return activeDecoders_.size() == 1;
                }();

                if (okToReconfigure) {
                    os_log_debug(log_, "Non-gapless join for %{public}@", decoderState->decoder_);

                    auto renderFormat = decoderState->converter_.outputFormat;
                    if (NSError *error = nil; !configureProcessingGraphAndRingBufferForFormat(renderFormat, &error)) {
                        setErrorAndRequestCancel(decoderState, error);
                        continue;
                    }

                    clearFlags(Flags::drainRequired);
                    formatMismatch = false;

                    // Allocate the buffer that is the intermediary between the decoder state and the ring buffer
                    if (auto format = buffer.format; format.channelCount != renderFormat.channelCount ||
                                                     format.sampleRate != renderFormat.sampleRate) {
                        buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderFormat
                                                               frameCapacity:ringBufferChunkSize];
                        if (buffer == nil) {
                            os_log_error(log_,
                                         "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %u",
                                         stringDescribingAVAudioFormat(renderFormat), ringBufferChunkSize);
                            setErrorAndRequestCancel(decoderState,
                                                     [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                         code:SFBAudioPlayerErrorCodeInternalError
                                                                     userInfo:nil]);
                            continue;
                        }
                    }
                } else {
                    decoderState = nullptr;
                }
            }
        }

        if (decoderState != nullptr) {
            if (const auto flags = loadFlags(); bits::is_clear(flags, Flags::drainRequired)) {
                // Decode and write chunks and metadata to the ring buffers
                while (audioBuffer_.freeSpace() >= ringBufferChunkSize &&
                       audioMetadata_.freeSpace() >= sizeof(detail::DecodedChunkDescriptor)) {

                    // The chunk descriptor for the chunk to be decoded
                    detail::DecodedChunkDescriptor descriptor{};
                    descriptor.playbackGeneration_ = playbackGeneration_.load(std::memory_order_acquire);
                    descriptor.sequenceNumber_ = decoderState->sequenceNumber_;

                    // Decoding started
                    if (const auto decoderFlags = decoderState->loadFlags();
                        bits::is_clear(decoderFlags, DecoderState::Flags::decodingStarted)) {
                        const auto suspended = bits::is_set(decoderFlags, DecoderState::Flags::decodingSuspended);

                        if (!suspended) {
                            os_log_debug(log_, "Decoding starting for %{public}@", decoderState->decoder_);
                        } else {
                            os_log_debug(log_, "Decoding starting after suspension for %{public}@",
                                         decoderState->decoder_);
                        }

                        decoderState->setFlags(DecoderState::Flags::decodingStarted);

                        // Submit the decoding started event for the initial start only
                        if (!suspended) {
                            if (events_.enqueue(EventCommand::decodingStarted, decoderState->sequenceNumber_)) {
                                eventSemaphore_.signal();
                            } else {
                                os_log_fault(log_, "Error writing decoding started event");
                            }
                        }
                    }

                    descriptor.framePosition_ = decoderState->framesDecoded_.load(std::memory_order_acquire);

                    // Decode audio into the buffer, converting to the rendering format in the process
                    if (NSError *error = nil; !decoderState->decodeAudio(buffer, &error)) {
                        setErrorAndRequestCancel(decoderState, error);
                        goto next_outer_iteration;
                    }

                    descriptor.frameLength_ = buffer.frameLength;

                    // Write the decoded chunk descriptor to the metadata buffer
                    if (!audioMetadata_.write(descriptor)) {
                        os_log_fault(
                                log_,
                                "Error writing audio chunk descriptor to ring buffer: spsc::RingBuffer::write failed");
                    }

                    // Write the decoded audio to the audio buffer for rendering
                    const auto framesWritten = audioBuffer_.write(buffer.audioBufferList, buffer.frameLength);
                    if (framesWritten != buffer.frameLength) {
                        os_log_fault(
                                log_,
                                "Error writing audio to ring buffer: spsc::AudioRingBuffer::write failed for %u frames",
                                buffer.frameLength);
                    }

                    // Decoding complete
                    if (const auto decoderFlags = decoderState->loadFlags();
                        bits::is_set(decoderFlags, DecoderState::Flags::decodingComplete)) {
                        const auto resumed = bits::is_set(decoderFlags, DecoderState::Flags::decodingResumed);

                        // Submit the decoding complete event for the first completion only
                        if (!resumed) {
                            if (events_.enqueue(EventCommand::decodingComplete, decoderState->sequenceNumber_)) {
                                eventSemaphore_.signal();
                            } else {
                                os_log_fault(log_, "Error writing decoding complete event");
                            }
                        }

                        if (!resumed) {
                            os_log_debug(log_, "Decoding complete for %{public}@", decoderState->decoder_);
                        } else {
                            os_log_debug(log_, "Decoding complete after resuming for %{public}@",
                                         decoderState->decoder_);
                        }

                        break;
                    }
                }

                // Clear the mute flag if needed now that the ring buffer is full
                if (bits::is_set(flags, Flags::isMuted)) {
                    clearFlags(Flags::isMuted);
                }
            }
        }

        int64_t deltaNanos;
        if (decoderState == nullptr) {
            if (formatMismatch) {
                // Shorter timeout if waiting on a decoder to complete rendering for a pending format change
                deltaNanos = 25 * NSEC_PER_MSEC;
            } else {
                // Idling
                deltaNanos = NSEC_PER_SEC / 2;
            }
        } else {
            // Determine timeout based on ring buffer free space
            // Attempt to keep the ring buffer 75% full
            const auto targetMaxFreeSpace = audioBuffer_.capacity() / 4;
            const auto freeSpace = audioBuffer_.freeSpace();

            if (freeSpace > targetMaxFreeSpace) {
                // Minimal timeout if the ring buffer has more free space than desired
                deltaNanos = static_cast<int64_t>(2.5 * NSEC_PER_MSEC);
            } else {
                const auto duration = (targetMaxFreeSpace - freeSpace) / audioBuffer_.format().mSampleRate;
                deltaNanos = static_cast<int64_t>(duration * NSEC_PER_SEC);
            }
        }

        // Wait for an event signal; ring buffer space availability is polled using the timeout
        decodingSemaphore_.wait(dispatch_time(DISPATCH_TIME_NOW, deltaNanos));

    next_outer_iteration:;
    }

    os_log_debug(log_, "<AudioPlayer: %p> decoding thread complete", this);
}

// MARK: - Rendering

OSStatus sfb::AudioPlayer::render(BOOL &isSilence, const AudioTimeStamp &timestamp, AVAudioFrameCount frameCount,
                                  AudioBufferList *outputData) noexcept {
    const auto flags = loadFlags();

    /// Sets the buffers in an AudioBufferList struct to zero.
    const auto zeroABL = [](AudioBufferList *abl) noexcept {
        for (UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
            std::memset(abl->mBuffers[i].mData, 0, abl->mBuffers[i].mDataByteSize);
        }
    };

    // Discard any stale frames in the ring buffer from a seek or decoder cancelation
    if (bits::is_set(flags, Flags::drainRequired)) {
        audioBuffer_.drain();
        audioMetadata_.drain();
        renderingChunk_ = {};
        clearFlags(Flags::drainRequired);
        zeroABL(outputData);
        isSilence = YES;
        return noErr;
    }

    // Output silence if muted or not playing
    if (bits::is_set_or_is_clear(flags, Flags::isMuted, Flags::isPlaying)) {
        zeroABL(outputData);
        isSilence = YES;
        return noErr;
    }

    /// Computes the event time for a given frame offset
    const auto eventTimeForFrameOffset = [&](uint32_t frameOffset) noexcept -> uint64_t {
        const auto deltaSeconds = frameOffset / audioBuffer_.format().mSampleRate;
        const auto scaledNanos = static_cast<uint64_t>(deltaSeconds * timestamp.mRateScalar * nanosecondsPerSecond);
        return timestamp.mHostTime + host_time::fromNanoseconds(scaledNanos);
    };

    // Read audio from the ring buffer
    const auto framesRead = static_cast<uint32_t>(audioBuffer_.read(outputData, frameCount));

    // Read and process chunk descriptors for the rendered audio
    if (framesRead > 0) {
        auto framesRemaining = framesRead;
        do {
            // Read the next chunk descriptor if needed
            if (renderingChunk_.framesRemaining() == 0) {
                if (!audioMetadata_.read(renderingChunk_.descriptor_)) {
                    setFlags(Flags::renderEventDropped);
                    break;
                }
                renderingChunk_.framesConsumed_ = 0;
            }

            const auto chunkFrames = std::min(renderingChunk_.framesRemaining(), framesRemaining);
            if (chunkFrames > 0) {
                const auto eventTime = eventTimeForFrameOffset(framesRead - framesRemaining);
                if (!events_.enqueue(EventCommand::framesRendered, eventTime,
                                     renderingChunk_.descriptor_.sequenceNumber_, chunkFrames,
                                     renderingChunk_.descriptor_.playbackGeneration_)) {
                    setFlags(Flags::renderEventDropped);
                    break;
                }

                renderingChunk_.framesConsumed_ += chunkFrames;
                framesRemaining -= chunkFrames;
            }
        } while (framesRemaining > 0);
    } else {
        isSilence = YES;
    }

    if (framesRead != frameCount) {
        if (!events_.enqueue(EventCommand::renderBufferUnderrun, timestamp.mHostTime, framesRead, frameCount)) {
            setFlags(Flags::renderEventDropped);
        }
    }

    return noErr;
}

// MARK: - Event Processing

void sfb::AudioPlayer::sequenceAndProcessEvents(std::stop_token stoken) noexcept {
    pthread_setname_np("AudioPlayer.Events");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

    os_log_debug(log_, "<AudioPlayer: %p> event processing thread starting", this);

    while (!stoken.stop_requested()) {

        // Process pending events
        EventCommand eventCommand;
        while (events_.peek(eventCommand)) {
            switch (eventCommand) {
            case EventCommand::decodingStarted:
                processDecodingStartedEvent();
                break;
            case EventCommand::decodingComplete:
                processDecodingCompleteEvent();
                break;
            case EventCommand::seek:
                processDecoderSeekEvent();
                break;
            case EventCommand::decoderCanceled:
                processDecoderCanceledEvent();
                break;
            case EventCommand::allocationFailure:
                processAllocationFailureEvent();
                break;
            case EventCommand::framesRendered:
                processFramesRenderedEvent();
                break;
            case EventCommand::renderBufferUnderrun:
                processRenderBufferUnderrunEvent();
                break;

            default:
#if DEBUG
                assert(false && "Unknown EventCommand");
#endif /* DEBUG */
                os_log_error(log_, "Unknown event command: %u", static_cast<uint32_t>(eventCommand));
                break;
            }
        }

        if (const auto prevFlags = clearFlags(Flags::renderEventDropped);
            bits::is_set(prevFlags, Flags::renderEventDropped)) {
            os_log_fault(log_, "Missing rendering event(s): event message queue overrun");
        }

        int64_t deltaNanos;
        {
            std::lock_guard lock{activeDecodersMutex_};
            if (firstActiveDecoderState() != nullptr) {
                deltaNanos = static_cast<int64_t>(7.5 * NSEC_PER_MSEC);
            } else {
                // Use a longer timeout when idle
                deltaNanos = NSEC_PER_SEC / 2;
            }
        }

        // Decoding events will be signaled; render events are polled using the timeout
        eventSemaphore_.wait(dispatch_time(DISPATCH_TIME_NOW, deltaNanos));
    }

    os_log_debug(log_, "<AudioPlayer: %p> event processing thread complete", this);
}

// MARK: Decoding Events

bool sfb::AudioPlayer::processDecodingStartedEvent() noexcept {
    EventCommand command;
    uint64_t sequenceNumber;
    if (!events_.dequeue(command, sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoding started event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::decodingStarted);
#endif /* DEBUG */

    Decoder decoder = nil;
    Decoder currentDecoder = nil;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto *decoderState = decoderStateWithSequenceNumber(sequenceNumber); decoderState != nullptr) {
            decoder = decoderState->decoder_;
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoding started event",
                         sequenceNumber);
            return false;
        }

        if (const auto *decoderState = firstActiveDecoderState(); decoderState != nullptr) {
            currentDecoder = decoderState->decoder_;
        }
    }

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)]) {
        [delegate audioPlayer:player_ decodingStarted:decoder];
    }

    if (bits::is_clear(loadFlags(), Flags::isPlaying) && decoder == currentDecoder) {
        setNowPlaying(decoder);
    }

    return true;
}

bool sfb::AudioPlayer::processDecodingCompleteEvent() noexcept {
    EventCommand command;
    uint64_t sequenceNumber;
    if (!events_.dequeue(command, sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoding complete event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::decodingComplete);
#endif /* DEBUG */

    Decoder decoder = nil;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto *decoderState = decoderStateWithSequenceNumber(sequenceNumber); decoderState != nullptr) {
            decoder = decoderState->decoder_;
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoding complete event",
                         sequenceNumber);
            return false;
        }
    }

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)]) {
        [delegate audioPlayer:player_ decodingComplete:decoder];
    }

    return true;
}

bool sfb::AudioPlayer::processDecoderSeekEvent() noexcept {
    EventCommand command;
    uint64_t sequenceNumber;
    int64_t frame;
    if (!events_.dequeue(command, sequenceNumber, frame)) {
        os_log_error(log_, "Missing decoder sequence number or frame position for decoder seek event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::seek);
#endif /* DEBUG */

    Decoder decoder = nil;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (auto *decoderState = decoderStateWithSequenceNumber(sequenceNumber); decoderState != nullptr) {
            decoderState->framesRendered_.store(frame, std::memory_order_release);
            if (bits::is_clear(decoderState->loadFlags(), DecoderState::Flags::renderingStarted)) {
                return true;
            }
            decoder = decoderState->decoder_;
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoder seek event",
                         sequenceNumber);
            return false;
        }
    }

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:didSeek:toFrame:)]) {
        [delegate audioPlayer:player_ didSeek:decoder toFrame:frame];
    }

    return true;
}

bool sfb::AudioPlayer::processDecoderCanceledEvent() noexcept {
    EventCommand command;
    uint64_t sequenceNumber;
    if (!events_.dequeue(command, sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoder canceled event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::decoderCanceled);
#endif /* DEBUG */

    Decoder decoder = nil;
    NSError *error = nil;
    AVAudioFramePosition framesRendered = 0;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto iter = std::ranges::find(activeDecoders_, sequenceNumber, &DecoderState::sequenceNumber_);
            iter != activeDecoders_.cend()) {
            decoder = (*iter)->decoder_;
            error = (*iter)->error_;
            framesRendered = (*iter)->framesRendered_.load(std::memory_order_acquire);

            os_log_debug(log_, "Deleting decoder state for %{public}@", (*iter)->decoder_);
            activeDecoders_.erase(iter);
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoder canceled event",
                         sequenceNumber);
            return false;
        }
    }

    // Mark the decoder as canceled for any scheduled render notifications
    objc_setAssociatedObject(decoder, &decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate; delegate != nil) {
        if (error == nil) {
            if ([delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)]) {
                [delegate audioPlayer:player_ decoderCanceled:decoder framesRendered:framesRendered];
            }
        } else {
            if ([delegate respondsToSelector:@selector(audioPlayer:decodingAborted:error:framesRendered:)]) {
                [delegate audioPlayer:player_ decodingAborted:decoder error:error framesRendered:framesRendered];
            }
        }
    }

    const auto hasNoDecoders = [&]() noexcept {
        std::scoped_lock lock{queuedDecodersMutex_, activeDecodersMutex_};
        return queuedDecoders_.empty() && activeDecoders_.empty();
    }();

    if (hasNoDecoders) {
        setNowPlaying(nil);

        const auto didStopEngine = stopEngineIfRunning();
        if (didStopEngine) {
            if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
                [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
            }
        }
    }

    return true;
}

bool sfb::AudioPlayer::processAllocationFailureEvent() noexcept {
    EventCommand command;
    if (!events_.dequeue(command)) {
        os_log_error(log_, "Missing command for allocation failure event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::allocationFailure);
#endif /* DEBUG */

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
        NSError *underlying = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                             code:SFBAudioPlayerErrorCodeInternalError
                                         userInfo:@{NSUnderlyingErrorKey : underlying}];
        [delegate audioPlayer:player_ encounteredError:error];
    }

    return true;
}

// MARK: Rendering Events

bool sfb::AudioPlayer::processFramesRenderedEvent() noexcept {
    EventCommand command;
    // The event time calculated from the render cycle's host time and rate scalar
    uint64_t eventTime;
    // The decoder sequence number for the decoder providing the frames
    uint64_t sequenceNumber;
    // The number of valid frames rendered
    uint32_t frameCount;
    // The playback generation of the chunk containing the frames
    uint64_t playbackGeneration;
    if (!events_.dequeue(command, eventTime, sequenceNumber, frameCount, playbackGeneration)) {
        os_log_error(log_, "Missing event time, decoder sequence number, frame count, or playback generation for "
                           "frames rendered event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::framesRendered);
    assert(frameCount > 0);
#endif /* DEBUG */

    // If a frames rendered event was posted it means valid frames were rendered
    // during that render cycle.
    //
    // However, between the time the frames rendered event was queued and when it is processed
    // a decoder may have been canceled or a seek may have occurred, making the event stale.
    //
    // This is indicated by an increment in the transport epoch/playback generation.
    //
    // Discard stale events from previous playback generations.
    if (playbackGeneration != playbackGeneration_.load(std::memory_order_acquire)) {
        os_log_debug(log_, "Discarding stale frames rendered event");
        return true;
    }

    struct RenderingEventDetails {
        enum class Type {
            willStart,
            willComplete,
        };
        Type type_;
        Decoder _Nonnull decoder_;
        uint64_t time_;
    };

    // Queued events to be dispatched once the lock is released
    std::vector<RenderingEventDetails> queuedEvents;

    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto iter = std::ranges::find(activeDecoders_, sequenceNumber, &DecoderState::sequenceNumber_);
            iter != activeDecoders_.cend()) {
            const auto decoderFlags = (*iter)->loadFlags();

            // Rendering is starting
            if (bits::is_clear(decoderFlags, DecoderState::Flags::renderingStarted)) {
                (*iter)->setFlags(DecoderState::Flags::renderingStarted);

                try {
                    queuedEvents.push_back({RenderingEventDetails::Type::willStart, (*iter)->decoder_, eventTime});
                } catch (const std::exception &e) {
                    os_log_error(log_, "Error queuing rendering will start event for %{public}@: %{public}s",
                                 (*iter)->decoder_, e.what());
                }
            }

            const auto framesDecoded = (*iter)->framesDecoded_.load(std::memory_order_acquire);
            const auto framesRendered = (*iter)->framesRendered_.fetch_add(frameCount, std::memory_order_acq_rel);
            const auto framesRemaining = framesDecoded - framesRendered;
#if DEBUG
            assert(framesRemaining >= frameCount);
#endif /* DEBUG */

            // Rendering is complete
            if (bits::is_set(decoderFlags, DecoderState::Flags::decodingComplete) && frameCount == framesRemaining) {
                try {
                    queuedEvents.push_back({RenderingEventDetails::Type::willComplete, (*iter)->decoder_, eventTime});
                } catch (const std::exception &e) {
                    os_log_error(log_, "Error queuing rendering will complete event for %{public}@: %{public}s",
                                 (*iter)->decoder_, e.what());
                }

                os_log_debug(log_, "Deleting decoder state for %{public}@", (*iter)->decoder_);
                activeDecoders_.erase(iter);
            }
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for frames rendered event",
                         sequenceNumber);
            return false;
        }
    }

    // Call functions that notify the delegate after unlocking the lock
    for (const auto &event : queuedEvents) {
        switch (event.type_) {
        case RenderingEventDetails::Type::willStart:
            handleRenderingWillStartEvent(event.decoder_, event.time_);
            break;
        case RenderingEventDetails::Type::willComplete:
            handleRenderingWillCompleteEvent(event.decoder_, event.time_);
            break;
        default:
#if DEBUG
            assert(false && "Unknown RenderingEventDetails::Type");
#endif /* DEBUG */
            os_log_error(log_, "Unknown rendering event details type: %d", static_cast<int>(event.type_));
            break;
        }
    }

    return true;
}

bool sfb::AudioPlayer::processRenderBufferUnderrunEvent() noexcept {
    EventCommand command;
    // The host time from the render cycle's timestamp
    uint64_t hostTime;
    // The number of valid frames rendered
    uint32_t framesRendered;
    // The number of frames that were requested by the IOProc
    uint32_t framesRequested;
    if (!events_.dequeue(command, hostTime, framesRendered, framesRequested)) {
        os_log_error(log_, "Missing host time or frame count for render buffer underrun event");
        return false;
    }

#if DEBUG
    assert(command == EventCommand::renderBufferUnderrun);
#endif /* DEBUG */

    os_log_error(log_, "Audio ring buffer underrun: %u/%u frames rendered for host time %llu", framesRendered,
                 framesRequested, hostTime);

    return true;
}

void sfb::AudioPlayer::handleRenderingWillStartEvent(Decoder decoder, uint64_t hostTime) noexcept {
    const auto now = host_time::current();
    if (now > hostTime) {
        os_log_error(log_, "Rendering started event processed %.2f msec late for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(now - hostTime)) / nanosecondsPerMillisecond,
                     decoder);
    } else {
#if DEBUG
        os_log_debug(log_, "Rendering will start in %.2f msec for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(hostTime - now)) / nanosecondsPerMillisecond,
                     decoder);
#endif /* DEBUG */
    }

    // Since the rendering started notification is submitted for asynchronous execution,
    // store a weak reference to the owning SFBAudioPlayer to prevent use-after-free
    // in the event the player is deallocated before the closure is called
    __weak SFBAudioPlayer *weakPlayer = player_;

    // Schedule the rendering started notification at the expected host time
    dispatch_after(hostTime, eventQueue_, ^{
        // If weakPlayer is nil it means the SFBAudioPlayer instance was deallocated
        __strong SFBAudioPlayer *player = weakPlayer;
        if (player == nil) {
            os_log_debug(log_,
                         "Audio player deallocated between rendering will start and rendering started notifications");
            return;
        }

        if (NSNumber *isCanceled = objc_getAssociatedObject(decoder, &decoderIsCanceledKey); isCanceled.boolValue) {
            os_log_debug(log_, "%{public}@ canceled after rendering will start notification", decoder);
            return;
        }

        // Not this, but that: `this` is not safe to use within this closure
        auto &that = player->_player;

#if DEBUG
        const auto now = host_time::current();
        const auto delta = host_time::toNanoseconds(absoluteDifference(hostTime, now));
        const auto tolerance =
                static_cast<uint64_t>(nanosecondsPerSecond / [that->sourceNode_ outputFormatForBus:0].sampleRate);
        if (delta > tolerance) {
            os_log_debug(log_, "Rendering started notification arrived %.2f msec %s",
                         static_cast<double>(delta) / nanosecondsPerMillisecond, now > hostTime ? "late" : "early");
        }
#endif /* DEBUG */

        that->setNowPlaying(decoder);

        if (__strong id<SFBAudioPlayerDelegate> delegate = player.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)]) {
            [delegate audioPlayer:player renderingStarted:decoder];
        }
    });

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)]) {
        [delegate audioPlayer:player_ renderingWillStart:decoder atHostTime:hostTime];
    }
}

void sfb::AudioPlayer::handleRenderingWillCompleteEvent(Decoder decoder, uint64_t hostTime) noexcept {
    const auto now = host_time::current();
    if (now > hostTime) {
        os_log_error(log_, "Rendering complete event processed %.2f msec late for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(now - hostTime)) / nanosecondsPerMillisecond,
                     decoder);
    } else {
#if DEBUG
        os_log_debug(log_, "Rendering will complete in %.2f msec for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(hostTime - now)) / nanosecondsPerMillisecond,
                     decoder);
#endif /* DEBUG */
    }

    // Since the rendering complete notification is submitted for asynchronous execution,
    // store a weak reference to the owning SFBAudioPlayer to prevent use-after-free
    // in the event the player is deallocated before the closure is called
    __weak SFBAudioPlayer *weakPlayer = player_;

    // Schedule the rendering complete notification at the expected host time
    dispatch_after(hostTime, eventQueue_, ^{
        // If weakPlayer is nil it means the owning SFBAudioPlayer instance was deallocated
        __strong SFBAudioPlayer *player = weakPlayer;
        if (player == nil) {
            os_log_debug(
                    log_,
                    "Audio player deallocated between rendering will complete and rendering complete notifications");
            return;
        }

        if (NSNumber *isCanceled = objc_getAssociatedObject(decoder, &decoderIsCanceledKey); isCanceled.boolValue) {
            os_log_debug(log_, "%{public}@ canceled after rendering will complete notification", decoder);
            return;
        }

        // Not this, but that: `this` is not safe to use within this closure
        auto &that = player->_player;

#if DEBUG
        const auto now = host_time::current();
        const auto delta = host_time::toNanoseconds(absoluteDifference(hostTime, now));
        const auto tolerance =
                static_cast<uint64_t>(nanosecondsPerSecond / [that->sourceNode_ outputFormatForBus:0].sampleRate);
        if (delta > tolerance) {
            os_log_debug(log_, "Rendering complete notification arrived %.2f msec %s",
                         static_cast<double>(delta) / nanosecondsPerMillisecond, now > hostTime ? "late" : "early");
        }
#endif /* DEBUG */

        if (__strong id<SFBAudioPlayerDelegate> delegate = player.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)]) {
            [delegate audioPlayer:player renderingComplete:decoder];
        }

        const auto hasNoDecoders = [&]() noexcept {
            std::scoped_lock lock{that->queuedDecodersMutex_, that->activeDecodersMutex_};
            return that->queuedDecoders_.empty() && that->activeDecoders_.empty();
        }();

        // End of audio
        if (hasNoDecoders) {
#if DEBUG
            os_log_debug(log_, "End of audio reached");
#endif /* DEBUG */

            that->setNowPlaying(nil);
            auto shouldStop = true;

            if (__strong id<SFBAudioPlayerDelegate> delegate = player.delegate;
                delegate != nil && [delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)]) {
                [delegate audioPlayerEndOfAudio:player];
                shouldStop = false;
            }

            if (shouldStop) {
                const auto didStopEngine = stopEngineIfRunning();
                if (didStopEngine) {
                    if (__strong id<SFBAudioPlayerDelegate> delegate = player.delegate;
                        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
                        [delegate audioPlayer:player playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
                    }
                }
            }
        }
    });

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)]) {
        [delegate audioPlayer:player_ renderingWillComplete:decoder atHostTime:hostTime];
    }
}

// MARK: - Active Decoder Management

void sfb::AudioPlayer::cancelActiveDecoders() noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    // Cancel all active decoders
    auto signal = false;
    for (const auto &decoderState : activeDecoders_) {
        if (bits::is_clear(decoderState->loadFlags(), DecoderState::Flags::isCanceled)) {
            decoderState->setFlags(DecoderState::Flags::cancelRequested);
            signal = true;
        }
    }

    // Signal the decoding thread if any cancelations were requested
    if (signal) {
        decodingSemaphore_.signal();
    }
}

sfb::AudioPlayer::DecoderState *sfb::AudioPlayer::firstActiveDecoderState() const noexcept {
#if DEBUG
    activeDecodersMutex_.assertIsOwner();
#endif /* DEBUG */
    const auto iter = std::ranges::find_if(activeDecoders_, [](const auto &decoderState) noexcept {
        const auto decoderFlags = decoderState->loadFlags();
        return bits::has_none(decoderFlags, DecoderState::Flags::needsInitialization | DecoderState::Flags::isCanceled);
    });
    return iter != activeDecoders_.cend() ? iter->get() : nullptr;
}

auto sfb::AudioPlayer::decoderStateWithSequenceNumber(uint64_t sequenceNumber) const noexcept -> DecoderState * {
#if DEBUG
    activeDecodersMutex_.assertIsOwner();
#endif /* DEBUG */
    const auto iter = std::ranges::find(activeDecoders_, sequenceNumber, &DecoderState::sequenceNumber_);
    return iter != activeDecoders_.cend() ? iter->get() : nullptr;
}

// MARK: - AVAudioEngine Notification Handling

void sfb::AudioPlayer::handleAudioEngineConfigurationChange(AVAudioEngine *engine,
                                                            [[maybe_unused]] NSDictionary *userInfo) noexcept {
    if (engine != engine_) {
        os_log_error(log_,
                     "AVAudioEngineConfigurationChangeNotification received for incorrect AVAudioEngine instance");
        return;
    }

    // AVAudioEngine posts this notification from a dedicated internal dispatch queue
    os_log_debug(log_, "Received AVAudioEngineConfigurationChangeNotification");

    // The output hardware’s channel count or sample rate changed
    {
        std::unique_lock lock{engineMutex_};

        // AVAudioEngine stops itself when a configuration change occurs
        // Flags::engineIsRunning indicates if the engine was running before the configuration change
        const auto prevFlags = clearFlags(Flags::engineIsRunning | Flags::isPlaying);
        const auto prevState = prevFlags & (Flags::engineIsRunning | Flags::isPlaying);

        AVAudioOutputNode *outputNode = engine_.outputNode;
        AVAudioMixerNode *mixerNode = engine_.mainMixerNode;

        AVAudioFormat *outputNodeOutputFormat = [outputNode outputFormatForBus:0];
        AVAudioFormat *mixerNodeOutputFormat = [mixerNode outputFormatForBus:0];

        // The output node's output format tracks the hardware sample rate and channel count
        // To avoid format conversion in both the source-mixer and mixer-output connections,
        // set the format for the mixer-output connection to the output node's output format
        if (outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate ||
            outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount) {
#if DEBUG
            if (outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate) {
                os_log_debug(log_,
                             "Mismatch between main mixer → output node connection sample rate (%g Hz) and hardware "
                             "sample rate (%g Hz)",
                             mixerNodeOutputFormat.sampleRate, outputNodeOutputFormat.sampleRate);
            }
            if (outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount) {
                os_log_debug(log_,
                             "Mismatch between main mixer → output node connection channel count (%u) and hardware "
                             "channel count (%u)",
                             mixerNodeOutputFormat.channelCount, outputNodeOutputFormat.channelCount);
            }
            os_log_debug(log_, "Setting main mixer → output node connection format to %{public}@",
                         stringDescribingAVAudioFormat(outputNodeOutputFormat));
#endif /* DEBUG */

            // AVAudioEngine stops itself when a configuration change occurs but it could have been restarted
            // before the notification was delivered or the lock was acquired.
            // Disconnecting the main mixer node from the output node when the engine is running causes an exception
            // so ensure the engine is stopped before updating the bus format.
            if (engine_.isRunning) {
#if DEBUG
                assert(bits::is_set(prevState, Flags::engineIsRunning));
#endif /* DEBUG */
                [engine_ stop];
            }

            [engine_ disconnectNodeInput:outputNode bus:0];

            // Reconnect the mixer and output nodes using the output node's output format
            [engine_ connect:mixerNode to:outputNode format:outputNodeOutputFormat];

            [engine_ prepare];
        }

        // Restart AVAudioEngine if previously running
        if (bits::is_set(prevState, Flags::engineIsRunning)) {
            if (NSError *startError = nil; ![engine_ startAndReturnError:&startError]) {
                os_log_error(log_, "Error starting AVAudioEngine: %{public}@", startError);
                lock.unlock();
                if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                    delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                    [delegate audioPlayer:player_ encounteredError:startError];
                }
                return;
            }

            // Restore previous playback state
            setFlags(prevState);
        }
    }

    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:audioEngineConfigurationChange:)]) {
        [delegate audioPlayer:player_ audioEngineConfigurationChange:userInfo];
    }
}

#if TARGET_OS_IPHONE
void sfb::AudioPlayer::handleAudioSessionInterruption(NSDictionary *userInfo) noexcept {
    const auto interruptionType = [[userInfo objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    switch (interruptionType) {
    case AVAudioSessionInterruptionTypeBegan: {
        os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeBegan)");

        {
            std::lock_guard lock{engineMutex_};
            const auto prevFlags = clearFlags(Flags::engineIsRunning | Flags::isPlaying);
            preInterruptState_ = bits::to_underlying(prevFlags & (Flags::engineIsRunning | Flags::isPlaying));
        }

        if (preInterruptState_ != 0) {
            if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
                [delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
            }
        }

        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:audioSessionInterruption:)]) {
            [delegate audioPlayer:player_ audioSessionInterruption:userInfo];
        }

        break;
    }

    case AVAudioSessionInterruptionTypeEnded: {
        os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:audioSessionInterruption:)]) {
            [delegate audioPlayer:player_ audioSessionInterruption:userInfo];
        }

        if (const auto interruptionOption =
                    [[userInfo objectForKey:AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
            !(interruptionOption & AVAudioSessionInterruptionOptionShouldResume)) {
            return;
        }

        if (NSError *sessionError = nil; ![[AVAudioSession sharedInstance] setActive:YES error:&sessionError]) {
            os_log_error(log_, "Error activating AVAudioSession: %{public}@", sessionError);
            if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                [delegate audioPlayer:player_ encounteredError:sessionError];
            }
            return;
        }

        const auto preInterruptState = static_cast<Flags>(preInterruptState_);

        {
            std::unique_lock lock{engineMutex_};

            if (bits::is_set(preInterruptState, Flags::engineIsRunning)) {
                if (NSError *startError = nil; ![engine_ startAndReturnError:&startError]) {
                    os_log_error(log_, "Error starting AVAudioEngine: %{public}@", startError);
                    lock.unlock();
                    if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                        delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                        [delegate audioPlayer:player_ encounteredError:startError];
                    }
                    return;
                }
            }

            const auto prevFlags = setFlags(preInterruptState);
#if DEBUG
            assert(bits::is_set_or_is_clear(prevFlags, Flags::engineIsRunning, Flags::isPlaying));
#endif /* DEBUG */
        }

        if (preInterruptState_ != 0) {
            if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
                delegate != nil && [delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
                [delegate audioPlayer:player_
                        playbackStateChanged:static_cast<SFBAudioPlayerPlaybackState>(preInterruptState_)];
            }
        }

        break;
    }

    default:
        os_log_error(log_, "Unknown value %lu for AVAudioSessionInterruptionTypeKey",
                     static_cast<unsigned long>(interruptionType));
        break;
    }
}
#endif

// MARK: - Processing Graph Management

bool sfb::AudioPlayer::stopEngineIfRunning() noexcept {
    std::lock_guard lock{engineMutex_};
    if (!engine_.isRunning) {
        return false;
    }
    [engine_ stop];
    clearFlags(Flags::engineIsRunning | Flags::isPlaying);
    return true;
}

bool sfb::AudioPlayer::configureProcessingGraphAndRingBufferForFormat(AVAudioFormat *format, NSError **error) noexcept {
#if DEBUG
    assert(format != nil);
    assert(format.isStandard);
    assert(![[sourceNode_ outputFormatForBus:0] isEqual:format]);
#endif /* DEBUG */

    os_log_debug(log_, "Reconfiguring audio processing graph for %{public}@", stringDescribingAVAudioFormat(format));

    // Allocate a temporary ring buffer for the new format before touching the engine or graph
    spsc::AudioRingBuffer ringBuffer;
    if (!ringBuffer.allocate(*(format.streamDescription), audioBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create audio buffer: spsc::AudioRingBuffer::allocate failed with format "
                     "%{public}@ and capacity %zu",
                     SFBASBDFormatDescription(format.streamDescription), audioBufferCapacity);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                         code:SFBAudioPlayerErrorCodeInternalError
                                     userInfo:nil];
        }
        return false;
    }

    std::lock_guard lock{engineMutex_};

    // Even if the engine isn't running, call -stop to force release of any render resources
    // This is necessary when transitioning between formats with different channel counts
    [engine_ stop];

    // Attempt to preserve the playback state
    const auto prevFlags = clearFlags(Flags::engineIsRunning | Flags::isPlaying);
    const auto prevState = prevFlags & (Flags::engineIsRunning | Flags::isPlaying);

    // Reconfigure the processing graph
    AVAudioConnectionPoint *sourceNodeOutputConnectionPoint = [[engine_ outputConnectionPointsForNode:sourceNode_
                                                                                            outputBus:0] firstObject];
    [engine_ disconnectNodeOutput:sourceNode_];

    // Adopt the new ring buffer and reset the render state
    // These operations are not thread-safe but the engine is stopped
    audioBuffer_ = std::move(ringBuffer);
    audioMetadata_.drain();
    renderingChunk_ = {};

    // Reconnect the source node to the next node in the processing chain
    // This is the mixer node in the default configuration, but additional nodes may
    // have been inserted between the source and mixer nodes. In this case allow the delegate
    // to make any necessary adjustments based on the format change if desired.
    if (AVAudioMixerNode *mixerNode = engine_.mainMixerNode;
        sourceNodeOutputConnectionPoint != nil && sourceNodeOutputConnectionPoint.node != mixerNode) {
        if (__strong id<SFBAudioPlayerDelegate> delegate = player_.delegate;
            delegate != nil &&
            [delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
            AVAudioNode *node = [delegate audioPlayer:player_ reconfigureProcessingGraph:engine_ withFormat:format];
            // Ensure the delegate returned a valid node
            assert(node != nil && "nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
            assert([engine_ inputConnectionPointForNode:engine_.outputNode inputBus:0].node == mixerNode &&
                   "Illegal AVAudioEngine configuration");
            [engine_ connect:sourceNode_ to:node format:format];
        } else {
            [engine_ connect:sourceNode_ to:sourceNodeOutputConnectionPoint.node format:format];
        }
    } else {
        [engine_ connect:sourceNode_ to:mixerNode format:format];
    }

#if DEBUG
    logProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

    [engine_ prepare];

    // Restart AVAudioEngine and playback as appropriate
    if (bits::is_set(prevState, Flags::engineIsRunning)) {
        if (NSError *startError = nil; ![engine_ startAndReturnError:&startError]) {
            os_log_error(log_, "Error starting AVAudioEngine: %{public}@", startError);
            if (error != nullptr) {
                *error = startError;
            }
            // Engine failed to (re)start
            return false;
        }

        setFlags(prevState);
    }

    return true;
}
