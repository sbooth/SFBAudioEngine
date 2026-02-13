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
#import <ranges>

namespace {

/// The default ring buffer capacity
constexpr std::size_t ringBufferCapacity = 16384;
/// The minimum number of frames to write to the ring buffer
constexpr AVAudioFrameCount ringBufferChunkSize = 2048;

/// The default decoding event ring buffer capacity
constexpr std::size_t decodingEventRingBufferCapacity = 2048;
/// The default rendering event ring buffer capacity
constexpr std::size_t renderingEventRingBufferCapacity = 4096;

/// Objective-C associated object key indicating if a decoder has been canceled
constexpr char decoderIsCanceledKey = '\0';

void audioEngineConfigurationChangeNotificationCallback(CFNotificationCenterRef center, void *observer,
                                                        CFNotificationName name, const void *object,
                                                        CFDictionaryRef userInfo) {
#pragma unused(center)
#pragma unused(name)
    auto *that = static_cast<sfb::AudioPlayer *>(observer);
    that->handleAudioEngineConfigurationChange((__bridge AVAudioEngine *)object, (__bridge NSDictionary *)userInfo);
}

#if TARGET_OS_IPHONE
void audioSessionInterruptionNotificationCallback(CFNotificationCenterRef center, void *observer,
                                                  CFNotificationName name, const void *object,
                                                  CFDictionaryRef userInfo) {
#pragma unused(center)
#pragma unused(name)
#pragma unused(object)
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

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t nextEventIdentificationNumber() noexcept {
    static std::atomic_uint64_t nextIdentificationNumber = 1;
    static_assert(std::atomic_uint64_t::is_always_lock_free, "Lock-free std::atomic_uint64_t required");
    return nextIdentificationNumber.fetch_add(1, std::memory_order_relaxed);
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
    static uint64_t sequenceCounter_;

    /// Monotonically increasing instance counter
    const uint64_t sequenceNumber_{sequenceCounter_++};

    /// Decodes audio from the source representation to PCM
    const Decoder decoder_{nil};

    /// The sample rate of the audio converter's output format
    const double sampleRate_{0};

    /// Flags
    std::atomic_uint flags_{0};
    static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

    /// The number of frames decoded
    std::atomic_int64_t framesDecoded_{0};
    /// The number of frames converted
    std::atomic_int64_t framesConverted_{0};
    /// The number of frames rendered
    std::atomic_int64_t framesRendered_{0};
    /// The total number of audio frames
    std::atomic_int64_t frameLength_{0};
    /// The desired seek offset
    std::atomic_int64_t seekOffset_{SFBUnknownFramePosition};

    static_assert(std::atomic_int64_t::is_always_lock_free, "Lock-free std::atomic_int64_t required");

    /// Converts audio from the decoder's processing format to the equivalent standard format
    AVAudioConverter *converter_{nil};
    /// Buffer used internally for buffering during conversion
    AVAudioPCMBuffer *decodeBuffer_{nil};

    /// The error that caused decoding to abort, if any
    NSError *error_{nil};

    /// Possible bits in `flags_`
    enum class Flags : unsigned int {
        /// Decoding started
        decodingStarted = 1u << 0,
        /// Decoding complete
        decodingComplete = 1u << 1,
        /// Decoding was resumed after completion
        decodingResumed = 1u << 2,
        /// Decoding was suspended after starting
        decodingSuspended = 1u << 3,
        /// Rendering started
        renderingStarted = 1u << 4,
        /// A seek has been requested
        seekPending = 1u << 5,
        /// Decoder cancelation requested
        cancelRequested = 1u << 6,
        /// Decoder canceled
        isCanceled = 1u << 7,
    };

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

    AVAudioFramePosition framePosition() const noexcept;
    AVAudioFramePosition frameLength() const noexcept;

    bool decodeAudio(AVAudioPCMBuffer *_Nonnull buffer, NSError **error) noexcept;

    /// Sets the pending seek request to `frame`
    void requestSeekToFrame(AVAudioFramePosition frame) noexcept;
    /// Performs the pending seek request
    bool performSeek(NSError **error) noexcept;
};

uint64_t AudioPlayer::DecoderState::sequenceCounter_ = 1;

inline AudioPlayer::DecoderState::DecoderState(Decoder _Nonnull decoder) noexcept
    : decoder_{decoder}, sampleRate_{decoder.processingFormat.sampleRate}, frameLength_{decoder.frameLength} {
#if DEBUG
    assert(decoder != nil);
#endif /* DEBUG */
}

inline bool AudioPlayer::DecoderState::allocate(AVAudioFrameCount frameCapacity) noexcept {
    auto format = decoder_.processingFormat;
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

    // The logic in this class assumes no SRC is performed by converter_
    assert(converter_.inputFormat.sampleRate == converter_.outputFormat.sampleRate);

    decodeBuffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter_.inputFormat frameCapacity:frameCapacity];
    if (decodeBuffer_ == nil) {
        return false;
    }

    if (const auto framePosition = decoder_.framePosition; framePosition != 0) {
        framesDecoded_.store(framePosition, std::memory_order_release);
        framesConverted_.store(framePosition, std::memory_order_release);
        framesRendered_.store(framePosition, std::memory_order_release);
    }

    return true;
}

inline AVAudioFramePosition AudioPlayer::DecoderState::framePosition() const noexcept {
    const bool seekPending = bits::is_set(loadFlags(), Flags::seekPending);
    return seekPending ? seekOffset_.load(std::memory_order_acquire) : framesRendered_.load(std::memory_order_acquire);
}

inline AVAudioFramePosition AudioPlayer::DecoderState::frameLength() const noexcept {
    return frameLength_.load(std::memory_order_acquire);
}

inline bool AudioPlayer::DecoderState::decodeAudio(AVAudioPCMBuffer *_Nonnull buffer, NSError **error) noexcept {
#if DEBUG
    assert(buffer != nil);
    assert(buffer.frameCapacity == decodeBuffer_.frameCapacity);
#endif /* DEBUG */

    if (![decoder_ decodeIntoBuffer:decodeBuffer_ frameLength:decodeBuffer_.frameCapacity error:error]) {
        return false;
    }

    if (decodeBuffer_.frameLength == 0) {
        setFlags(Flags::decodingComplete);

#if false
        // Some formats may not know the exact number of frames in advance
        // without processing the entire file, which is a potentially slow operation
        frameLength_.store(mDecoder.framePosition, std::memory_order_release);
#endif /* false */

        buffer.frameLength = 0;
        return true;
    }

    this->framesDecoded_.fetch_add(decodeBuffer_.frameLength, std::memory_order_acq_rel);

    // Only PCM to PCM conversions are performed
    if (![converter_ convertToBuffer:buffer fromBuffer:decodeBuffer_ error:error]) {
        return false;
    }
    framesConverted_.fetch_add(buffer.frameLength, std::memory_order_acq_rel);

    // If `buffer` is not full but -decodeIntoBuffer:frameLength:error: returned `YES`
    // decoding is complete
    if (buffer.frameLength != buffer.frameCapacity) {
        setFlags(Flags::decodingComplete);
    }

    return true;
}

/// Sets the pending seek request to `frame`
inline void AudioPlayer::DecoderState::requestSeekToFrame(AVAudioFramePosition frame) noexcept {
    seekOffset_.store(frame, std::memory_order_release);
    setFlags(Flags::seekPending);
}

/// Performs the pending seek request
inline bool AudioPlayer::DecoderState::performSeek(NSError **error) noexcept {
#if DEBUG
    assert(bits::is_set(loadFlags(), Flags::seekPending));
#endif /* DEBUG */

    auto seekOffset = seekOffset_.load(std::memory_order_acquire);
    os_log_debug(log_, "Seeking to frame %lld in %{public}@ ", seekOffset, decoder_);

    if (NSError *seekError = nil; ![decoder_ seekToFrame:seekOffset error:&seekError]) {
        os_log_error(log_, "Error seeking to frame %lld in %{public}@", seekOffset, decoder_);
        if (error != nullptr) {
            *error = seekError;
        }
        return false;
    }

    // Reset the converter to flush any buffers
    [converter_ reset];

    const auto newFrame = decoder_.framePosition;
    if (newFrame != seekOffset) {
        os_log_info(log_, "Inaccurate seek to frame %lld, got %lld", seekOffset, newFrame);
        seekOffset = newFrame;
    }

    // Clear the seek request
    clearFlags(Flags::seekPending);

    // Update the frame counters accordingly
    // A seek is handled in essentially the same way as initial playback
    if (newFrame != SFBUnknownFramePosition) {
        framesDecoded_.store(newFrame, std::memory_order_release);
        framesConverted_.store(seekOffset, std::memory_order_release);
        framesRendered_.store(seekOffset, std::memory_order_release);
    }

    return newFrame != SFBUnknownFramePosition;
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

    // Allocate the audio ring buffer moving audio from the decoder queue to the render block
    if (!audioRingBuffer_.allocate(*(format.streamDescription), ringBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create audio ring buffer: spsc::AudioRingBuffer::allocate failed with format "
                     "%{public}@ and capacity %zu",
                     SFBASBDFormatDescription(format.streamDescription), ringBufferCapacity);
        throw std::runtime_error("spsc::AudioRingBuffer::allocate failed");
    }

    // ========================================
    // Event Processing Setup

    // The decoding event ring buffer is written to by the decoding thread and read from by the event queue
    if (!decodingEvents_.allocate(decodingEventRingBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create decoding event ring buffer: sfb::RingBuffer::allocate failed with capacity %zu",
                     decodingEventRingBufferCapacity);
        throw std::runtime_error("spsc::RingBuffer::allocate failed");
    }

    // The rendering event ring buffer is written to by the render block and read from by the event queue
    if (!renderingEvents_.allocate(renderingEventRingBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create rendering event ring buffer: sfb::RingBuffer::allocate failed with capacity %zu",
                     renderingEventRingBufferCapacity);
        throw std::runtime_error("spsc::RingBuffer::allocate failed");
    }

    // Create the dispatch queue used for event processing
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
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
        decodingThread_ = std::jthread(std::bind_front(&sfb::AudioPlayer::processDecoders, this));
        eventThread_ = std::jthread(std::bind_front(&sfb::AudioPlayer::sequenceAndProcessEvents, this));
#else
        decodingThread_ = std::thread(&sfb::AudioPlayer::processDecoders, this);
        eventThread_ = std::thread(&sfb::AudioPlayer::sequenceAndProcessEvents, this);
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */
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

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
    // Register a stop callback for the decoding thread
    std::stop_callback decodingThreadStopCallback(decodingThread_.get_stop_token(),
                                                  [this] { decodingSemaphore_.signal(); });

    // Issue a stop request to the decoding thread and wait for it to exit
    decodingThread_.request_stop();
#else
    // Stop the decoding thread
    flags_.fetch_or(static_cast<unsigned int>(Flags::stopDecodingThread), std::memory_order_acq_rel);
    dispatch_semaphore_signal(decodingSemaphore_);
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */

    try {
        decodingThread_.join();
    } catch (const std::exception &e) {
        os_log_error(log_, "Unable to join decoding thread: %{public}s", e.what());
    }

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
    // Register a stop callback for the event processing thread
    std::stop_callback eventThreadStopCallback(eventThread_.get_stop_token(), [this] { eventSemaphore_.signal(); });

    // Issue a stop request to the event processing thread and wait for it to exit
    eventThread_.request_stop();
#else
    // Stop the event processing thread
    flags_.fetch_or(static_cast<unsigned int>(Flags::stopEventThread), std::memory_order_acq_rel);
    dispatch_semaphore_signal(eventSemaphore_);
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */

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

    // Open the decoder if necessary
    if (!decoder.isOpen && ![decoder openReturningError:error]) {
        return false;
    }

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
        assert(!(didStartEngine && wasPlaying));
    }

    if ((didStartEngine || !wasPlaying) && [player_.delegate respondsToSelector:@selector(audioPlayer:
                                                                                        playbackStateChanged:)]) {
        [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
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

    if (wasPlaying && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
        [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
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

    if (wasPaused && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
        [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
    }

    return true;
}

void sfb::AudioPlayer::stop() noexcept {
    const auto didStopEngine = stopEngineIfRunning();

    clearDecoderQueue();
    cancelActiveDecoders();

    if (didStopEngine && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
        [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
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

            const auto prevFlags = setFlags(Flags::engineIsRunning | Flags::isPlaying);
            assert(bits::is_clear(prevFlags, Flags::isPlaying));

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

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
        [player_.delegate audioPlayer:player_ playbackStateChanged:playbackState];
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

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:)]) {
        [player_.delegate audioPlayer:player_ nowPlayingChanged:nowPlaying];
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

    if (const auto sampleRate = decoderState->sampleRate_; sampleRate > 0) {
        if (framePosition != SFBUnknownFramePosition) {
            playbackTime.currentTime = framePosition / sampleRate;
        }
        if (frameLength != SFBUnknownFrameLength) {
            playbackTime.totalTime = frameLength / sampleRate;
        }
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
        if (const auto sampleRate = decoderState->sampleRate_; sampleRate > 0) {
            if (currentPlaybackPosition.framePosition != SFBUnknownFramePosition) {
                currentPlaybackTime.currentTime = currentPlaybackPosition.framePosition / sampleRate;
            }
            if (currentPlaybackPosition.frameLength != SFBUnknownFrameLength) {
                currentPlaybackTime.totalTime = currentPlaybackPosition.frameLength / sampleRate;
            }
        }
        *playbackTime = currentPlaybackTime;
    }

    return true;
}

// MARK: - Seeking

bool sfb::AudioPlayer::seekInTime(NSTimeInterval secondsToSkip) noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    if (secondsToSkip == 0) {
        return true;
    }

    const auto sampleRate = decoderState->sampleRate_;
    const auto framePosition = decoderState->framePosition();
    const auto frameLength = decoderState->frameLength();

    auto targetFrame = framePosition + static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);
    targetFrame = std::clamp(targetFrame, 0LL, frameLength - 1);

    decoderState->requestSeekToFrame(targetFrame);
    decodingSemaphore_.signal();

    return true;
}

bool sfb::AudioPlayer::seekToTime(NSTimeInterval timeInSeconds) noexcept {
    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    const auto sampleRate = decoderState->sampleRate_;
    const auto frameLength = decoderState->frameLength();

    auto targetFrame = static_cast<AVAudioFramePosition>(timeInSeconds * sampleRate);
    targetFrame = std::clamp(targetFrame, 0LL, frameLength - 1);

    decoderState->requestSeekToFrame(targetFrame);
    decodingSemaphore_.signal();

    return true;
}

bool sfb::AudioPlayer::seekToPosition(double position) noexcept {
    position = std::clamp(position, 0.0, std::nextafter(1.0, 0.0));

    std::lock_guard lock{activeDecodersMutex_};

    auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr || !decoderState->decoder_.supportsSeeking) {
        return false;
    }

    const auto frameLength = decoderState->frameLength();
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

    const auto frameLength = decoderState->frameLength();
    frame = std::clamp(frame, 0LL, frameLength - 1);

    decoderState->requestSeekToFrame(frame);
    decodingSemaphore_.signal();

    return true;
}

bool sfb::AudioPlayer::supportsSeeking() const noexcept {
    std::lock_guard lock{activeDecodersMutex_};
    const auto *decoderState = firstActiveDecoderState();
    if (decoderState == nullptr) {
        return false;
    }
    return decoderState->decoder_.supportsSeeking;
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
    assert(engine_.isRunning == bits::is_set(loadFlags(), Flags::engineIsRunning) &&
           "AVAudioEngine may not be started or stopped outside of AudioPlayer");
}

// MARK: - Debugging

void sfb::AudioPlayer::logProcessingGraphDescription(os_log_t log, os_log_type_t type) const noexcept {
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

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
void sfb::AudioPlayer::processDecoders(std::stop_token stoken) noexcept
#else
void sfb::AudioPlayer::processDecoders() noexcept
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */
{
    pthread_setname_np("AudioPlayer.Decoding");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

    os_log_debug(log_, "<AudioPlayer: %p> decoding thread starting", this);

    // The buffer between the decoder state and the ring buffer
    AVAudioPCMBuffer *buffer = nil;
    // Whether there is a mismatch between the rendering format and the next decoder's processing format
    auto formatMismatch = false;

    // Returns true if the decoding thread should exit
    const auto stop_requested = [&] {
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
        return stoken.stop_requested();
#else
        return (flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::stopDecodingThread));
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */
    };

    while (!stop_requested()) {
        // The decoder state being processed
        DecoderState *decoderState = nullptr;
        auto ringBufferStale = false;

        {
            std::lock_guard lock{activeDecodersMutex_};

            // Process cancellations
            auto signal = false;
            for (const auto &decoderState : activeDecoders_) {
                if (bits::is_clear(decoderState->loadFlags(), DecoderState::Flags::cancelRequested)) {
                    continue;
                }

                if (decoderState->error_ == nil) {
                    os_log_debug(log_, "Canceling decoding for %{public}@", decoderState->decoder_);
                } else {
                    os_log_error(log_, "Aborting decoding for %{public}@ due to error", decoderState->decoder_);
                }

                decoderState->setFlags(DecoderState::Flags::isCanceled);
                ringBufferStale = true;

                // Submit the decoder canceled event
                if (decodingEvents_.writeAll(DecodingEventCommand::canceled, nextEventIdentificationNumber(),
                                             decoderState->sequenceNumber_)) {
                    signal = true;
                } else {
                    os_log_fault(log_, "Error writing decoder canceled event");
                }
            }

            // Signal the event thread if any decoders were canceled
            if (signal) {
                eventSemaphore_.signal();
            }

            // Get the earliest decoder state that has not completed rendering
            decoderState = firstActiveDecoderState();
        }

        // Process pending seeks
        if (decoderState != nullptr) {
            if (const auto flags = decoderState->loadFlags(); bits::is_set(flags, DecoderState::Flags::seekPending)) {
                if (NSError *seekError = nil; !decoderState->performSeek(&seekError)) {
                    decoderState->error_ = seekError;
                    decoderState->setFlags(DecoderState::Flags::cancelRequested);
                    continue;
                }
                ringBufferStale = true;

                if (bits::is_set(flags, DecoderState::Flags::decodingComplete)) {
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

                            const auto flags = nextDecoderState->loadFlags();
                            if (bits::is_set(flags, DecoderState::Flags::isCanceled)) {
                                continue;
                            }
                            if (bits::is_set(flags, DecoderState::Flags::decodingStarted)) {
                                os_log_debug(log_, "Suspending decoding for %{public}@", nextDecoderState->decoder_);

                                // TODO: Investigate a per-state buffer to mitigate frame loss
                                if (nextDecoderState->decoder_.supportsSeeking) {
                                    nextDecoderState->requestSeekToFrame(0);
                                    if (NSError *seekError = nil; !nextDecoderState->performSeek(&seekError)) {
                                        nextDecoderState->error_ = seekError;
                                        nextDecoderState->setFlags(DecoderState::Flags::cancelRequested);
                                        continue;
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
        }

        // Request a drain of the ring buffer during the next render cycle to prevent audible artifacts from seeking or
        // cancellation
        if (ringBufferStale) {
            setFlags(Flags::drainRequired);
        }

        // Get the earliest decoder state that has not completed decoding
        {
            std::lock_guard lock{activeDecodersMutex_};

            const auto iter = std::ranges::find_if(activeDecoders_, [](const auto &decoderState) {
                const auto flags = decoderState->loadFlags();
                return bits::has_none(flags, DecoderState::Flags::isCanceled | DecoderState::Flags::decodingComplete);
            });

            if (iter != activeDecoders_.cend()) {
                decoderState = (*iter).get();
            } else {
                decoderState = nullptr;
            }
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
                        os_log_error(log_, "Error creating decoder state for %{public}@: %{public}s", decoder,
                                     e.what());
                        submitDecodingErrorEvent([NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                     code:SFBAudioPlayerErrorCodeInternalError
                                                                 userInfo:nil]);
                        continue;
                    }
                }
            }

            if (decoderState != nullptr) {
                // Allocate decoder state internals
                if (!decoderState->allocate(ringBufferChunkSize)) {
                    os_log_error(log_,
                                 "Error allocating decoder state data: DecoderStateData::allocate failed with frame "
                                 "capacity %d",
                                 ringBufferChunkSize);
                    decoderState->error_ = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                               code:SFBAudioPlayerErrorCodeInternalError
                                                           userInfo:nil];
                    decoderState->setFlags(DecoderState::Flags::cancelRequested);
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
                                         "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d",
                                         stringDescribingAVAudioFormat(renderFormat), ringBufferChunkSize);
                            decoderState->error_ = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                       code:SFBAudioPlayerErrorCodeInternalError
                                                                   userInfo:nil];
                            decoderState->setFlags(DecoderState::Flags::cancelRequested);
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
                const auto okToReconfigure = [&] {
                    std::lock_guard lock{activeDecodersMutex_};
                    return activeDecoders_.size() == 1;
                }();

                if (okToReconfigure) {
                    clearFlags(Flags::drainRequired);
                    formatMismatch = false;

                    os_log_debug(log_, "Non-gapless join for %{public}@", decoderState->decoder_);

                    auto renderFormat = decoderState->converter_.outputFormat;
                    if (NSError *error = nil; !configureProcessingGraphAndRingBufferForFormat(renderFormat, &error)) {
                        decoderState->error_ = error;
                        decoderState->setFlags(DecoderState::Flags::cancelRequested);
                        continue;
                    }

                    // Allocate the buffer that is the intermediary between the decoder state and the ring buffer
                    if (auto format = buffer.format; format.channelCount != renderFormat.channelCount ||
                                                     format.sampleRate != renderFormat.sampleRate) {
                        buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderFormat
                                                               frameCapacity:ringBufferChunkSize];
                        if (buffer == nil) {
                            os_log_error(log_,
                                         "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d",
                                         stringDescribingAVAudioFormat(renderFormat), ringBufferChunkSize);
                            decoderState->error_ = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                                                       code:SFBAudioPlayerErrorCodeInternalError
                                                                   userInfo:nil];
                            decoderState->setFlags(DecoderState::Flags::cancelRequested);
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
                // Decode and write chunks to the ring buffer
                while (audioRingBuffer_.freeSpace() >= ringBufferChunkSize) {
                    // Decoding started
                    if (const auto flags = decoderState->loadFlags();
                        bits::is_clear(flags, DecoderState::Flags::decodingStarted)) {
                        const bool suspended = bits::is_set(flags, DecoderState::Flags::decodingSuspended);

                        if (!suspended) {
                            os_log_debug(log_, "Decoding starting for %{public}@", decoderState->decoder_);
                        } else {
                            os_log_debug(log_, "Decoding starting after suspension for %{public}@",
                                         decoderState->decoder_);
                        }

                        decoderState->setFlags(DecoderState::Flags::decodingStarted);

                        // Submit the decoding started event for the initial start only
                        if (!suspended) {
                            if (decodingEvents_.writeAll(DecodingEventCommand::started, nextEventIdentificationNumber(),
                                                         decoderState->sequenceNumber_)) {
                                eventSemaphore_.signal();
                            } else {
                                os_log_fault(log_, "Error writing decoding started event");
                            }
                        }
                    }

                    // Decode audio into the buffer, converting to the rendering format in the process
                    if (NSError *error = nil; !decoderState->decodeAudio(buffer, &error)) {
                        decoderState->error_ = error;
                        decoderState->setFlags(DecoderState::Flags::cancelRequested);
                        goto next_outer_iteration;
                    }

                    // Write the decoded audio to the ring buffer for rendering
                    const auto framesWritten = audioRingBuffer_.write(buffer.audioBufferList, buffer.frameLength);
                    if (framesWritten != buffer.frameLength) {
                        os_log_fault(
                                log_,
                                "Error writing audio to ring buffer: spsc::AudioRingBuffer::write failed for %d frames",
                                buffer.frameLength);
                    }

                    // Decoding complete
                    if (const auto flags = decoderState->loadFlags();
                        bits::is_set(flags, DecoderState::Flags::decodingComplete)) {
                        const bool resumed = bits::is_set(flags, DecoderState::Flags::decodingResumed);

                        // Submit the decoding complete event for the first completion only
                        if (!resumed) {
                            if (decodingEvents_.writeAll(DecodingEventCommand::complete,
                                                         nextEventIdentificationNumber(),
                                                         decoderState->sequenceNumber_)) {
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
            const auto targetMaxFreeSpace = audioRingBuffer_.capacity() / 4;
            const auto freeSpace = audioRingBuffer_.freeSpace();

            if (freeSpace > targetMaxFreeSpace) {
                // Minimal timeout if the ring buffer has more free space than desired
                deltaNanos = 2.5 * NSEC_PER_MSEC;
            } else {
                const auto duration = (targetMaxFreeSpace - freeSpace) / audioRingBuffer_.format().mSampleRate;
                deltaNanos = duration * NSEC_PER_SEC;
            }
        }

        // Wait for an event signal; ring buffer space availability is polled using the timeout
        decodingSemaphore_.wait(dispatch_time(DISPATCH_TIME_NOW, deltaNanos));

    next_outer_iteration:;
    }

    os_log_debug(log_, "<AudioPlayer: %p> decoding thread complete", this);
}

void sfb::AudioPlayer::submitDecodingErrorEvent(NSError *error) noexcept {
#if DEBUG
    assert(error != nil);
#endif /* DEBUG */

    NSError *err = nil;
    NSData *errorData = [NSKeyedArchiver archivedDataWithRootObject:error requiringSecureCoding:YES error:&err];
    if (errorData == nil) {
        os_log_error(log_, "Error archiving NSError for decoding error event: %{public}@", err);
        return;
    }

    auto [front, back] = decodingEvents_.writeVector();

    const auto frontSize = front.size();
    const auto spaceNeeded = sizeof(DecodingEventCommand) + sizeof(uint64_t) + sizeof(uint32_t) + errorData.length;

    if (frontSize + back.size() < spaceNeeded) {
        os_log_fault(log_, "Insufficient space to write decoding error event");
        return;
    }

    std::size_t cursor = 0;
    const auto writeArg = [&](const void *arg, std::size_t len) noexcept {
        const auto *src = static_cast<const unsigned char *>(arg);
        if (cursor + len <= frontSize) {
            std::memcpy(front.data() + cursor, src, len);
        } else if (cursor >= frontSize) {
            std::memcpy(back.data() + (cursor - frontSize), src, len);
        } else {
            const size_t toFront = frontSize - cursor;
            std::memcpy(front.data() + cursor, src, toFront);
            std::memcpy(back.data(), src + toFront, len - toFront);
        }
        cursor += len;
    };

    // Event header and payload
    const auto command = DecodingEventCommand::error;
    const auto identificationNumber = nextEventIdentificationNumber();
    const auto dataSize = static_cast<uint32_t>(errorData.length);

    writeArg(&command, sizeof command);
    writeArg(&identificationNumber, sizeof identificationNumber);
    writeArg(&dataSize, sizeof dataSize);
    writeArg(errorData.bytes, errorData.length);

    decodingEvents_.commitWrite(cursor);
    eventSemaphore_.signal();
}

// MARK: - Rendering

OSStatus sfb::AudioPlayer::render(BOOL &isSilence, const AudioTimeStamp &timestamp, AVAudioFrameCount frameCount,
                                  AudioBufferList *outputData) noexcept {
    const auto flags = loadFlags();

    // Discard any stale frames in the ring buffer from a seek or decoder cancelation
    if (bits::is_set(flags, Flags::drainRequired)) {
        audioRingBuffer_.drain();
        clearFlags(Flags::drainRequired);
        for (UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
            std::memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
        }
        isSilence = YES;
        return noErr;
    }

    // Output silence if not playing or muted
    if (!bits::is_set_without(flags, Flags::isPlaying, Flags::isMuted)) {
        for (UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
            std::memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
        }
        isSilence = YES;
        return noErr;
    }

    // Read audio from the ring buffer
    if (const auto framesRead = audioRingBuffer_.read(outputData, frameCount); framesRead > 0) {
#if DEBUG
        if (framesRead != frameCount) {
            os_log_debug(log_, "Insufficient audio in ring buffer: %zu frames available, %u requested", framesRead,
                         frameCount);
        }
#endif /* DEBUG */
        if (!renderingEvents_.writeAll(RenderingEventCommand::framesRendered, nextEventIdentificationNumber(),
                                       timestamp.mHostTime, timestamp.mRateScalar, static_cast<uint32_t>(framesRead))) {
            os_log_fault(log_, "Error writing frames rendered event");
        }
    } else {
        isSilence = YES;
    }

    return noErr;
}

// MARK: - Event Processing

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
void sfb::AudioPlayer::sequenceAndProcessEvents(std::stop_token stoken) noexcept
#else
void sfb::AudioPlayer::sequenceAndProcessEvents() noexcept
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */
{
    pthread_setname_np("AudioPlayer.Events");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

    os_log_debug(log_, "<AudioPlayer: %p> event processing thread starting", this);

    // Returns true if the event processing thread should exit
    const auto stop_requested = [&] {
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
        return stoken.stop_requested();
#else
        return (flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::stopEventThread));
#endif /* defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L */
    };

    while (!stop_requested()) {
        DecodingEventCommand decodingEventCommand;
        uint64_t decodingEventIdentificationNumber;
        auto gotDecodingEvent = decodingEvents_.readAll(decodingEventCommand, decodingEventIdentificationNumber);

        RenderingEventCommand renderingEventCommand;
        uint64_t renderingEventIdentificationNumber;
        auto gotRenderingEvent = renderingEvents_.readAll(renderingEventCommand, renderingEventIdentificationNumber);

        // Process all pending decoding and rendering events in sequential order
        while (gotDecodingEvent || gotRenderingEvent) {
            if (gotDecodingEvent &&
                (!gotRenderingEvent || decodingEventIdentificationNumber < renderingEventIdentificationNumber)) {
                processDecodingEvent(decodingEventCommand);
                gotDecodingEvent = decodingEvents_.readAll(decodingEventCommand, decodingEventIdentificationNumber);
            } else {
                processRenderingEvent(renderingEventCommand);
                gotRenderingEvent = renderingEvents_.readAll(renderingEventCommand, renderingEventIdentificationNumber);
            }
        }

        int64_t deltaNanos;
        {
            std::lock_guard lock{activeDecodersMutex_};
            if (firstActiveDecoderState() != nullptr) {
                deltaNanos = 7.5 * NSEC_PER_MSEC;
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

bool sfb::AudioPlayer::processDecodingEvent(DecodingEventCommand command) noexcept {
    switch (command) {
    case DecodingEventCommand::started:
        return processDecodingStartedEvent();

    case DecodingEventCommand::complete:
        return processDecodingCompleteEvent();

    case DecodingEventCommand::canceled:
        return processDecoderCanceledEvent();

    case DecodingEventCommand::error:
        return processDecodingErrorEvent();

    default:
        //        assert(false && "Unknown decoding event command");
        os_log_error(log_, "Unknown decoding event command: %u", command);
        return false;
    }
}

bool sfb::AudioPlayer::processDecodingStartedEvent() noexcept {
    uint64_t sequenceNumber;
    if (!decodingEvents_.read(sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoding started event");
        return false;
    }

    Decoder decoder = nil;
    Decoder currentDecoder = nil;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto iter = std::ranges::find(activeDecoders_, sequenceNumber, &DecoderState::sequenceNumber_);
            iter != activeDecoders_.cend()) {
            decoder = (*iter)->decoder_;
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoding started event",
                         sequenceNumber);
            return false;
        }

        if (const auto *decoderState = firstActiveDecoderState(); decoderState != nullptr) {
            currentDecoder = decoderState->decoder_;
        }
    }

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)]) {
        [player_.delegate audioPlayer:player_ decodingStarted:decoder];
    }

    if (bits::is_clear(loadFlags(), Flags::isPlaying) && decoder == currentDecoder) {
        setNowPlaying(decoder);
    }

    return true;
}

bool sfb::AudioPlayer::processDecodingCompleteEvent() noexcept {
    uint64_t sequenceNumber;
    if (!decodingEvents_.read(sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoding complete event");
        return false;
    }

    Decoder decoder = nil;
    {
        std::lock_guard lock{activeDecodersMutex_};

        if (const auto iter = std::ranges::find(activeDecoders_, sequenceNumber, &DecoderState::sequenceNumber_);
            iter != activeDecoders_.cend()) {
            decoder = (*iter)->decoder_;
        } else {
            os_log_error(log_, "Decoder state with sequence number %llu missing for decoding complete event",
                         sequenceNumber);
            return false;
        }
    }

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)]) {
        [player_.delegate audioPlayer:player_ decodingComplete:decoder];
    }

    return true;
}

bool sfb::AudioPlayer::processDecoderCanceledEvent() noexcept {
    uint64_t sequenceNumber;
    if (!decodingEvents_.read(sequenceNumber)) {
        os_log_error(log_, "Missing decoder sequence number for decoder canceled event");
        return false;
    }

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

    if (error == nil && [player_.delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)]) {
        [player_.delegate audioPlayer:player_ decoderCanceled:decoder framesRendered:framesRendered];
    } else if (error != nil && [player_.delegate respondsToSelector:@selector(audioPlayer:
                                                                            decodingAborted:error:framesRendered:)]) {
        [player_.delegate audioPlayer:player_ decodingAborted:decoder error:error framesRendered:framesRendered];
    }

    const auto hasNoDecoders = [&] {
        std::scoped_lock lock{queuedDecodersMutex_, activeDecodersMutex_};
        return queuedDecoders_.empty() && activeDecoders_.empty();
    }();

    if (hasNoDecoders) {
        setNowPlaying(nil);

        const auto didStopEngine = stopEngineIfRunning();
        if (didStopEngine && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
        }
    }

    return true;
}

bool sfb::AudioPlayer::processDecodingErrorEvent() noexcept {
    // The size in bytes of the archived NSError data
    uint32_t dataSize;
    if (!decodingEvents_.read(dataSize)) {
        os_log_error(log_, "Missing data size for decoding error event");
        return false;
    }

    // The archived NSError data
    NSMutableData *data = [NSMutableData dataWithLength:dataSize];
    if (decodingEvents_.read(data.mutableBytes, 1, dataSize, false) != dataSize) {
        os_log_error(log_, "Missing or incomplete archived NSError for decoding error event");
        return false;
    }

    NSError *err = nil;
    NSError *error = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSError class] fromData:data error:&err];
    if (error == nil) {
        os_log_error(log_, "Error unarchiving NSError for decoding error event: %{public}@", err);
        return false;
    }

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
        [player_.delegate audioPlayer:player_ encounteredError:error];
    }

    return true;
}

// MARK: Rendering Events

bool sfb::AudioPlayer::processRenderingEvent(RenderingEventCommand command) noexcept {
    switch (command) {
    case RenderingEventCommand::framesRendered:
        return processFramesRenderedEvent();

    default:
        //        assert(false && "Unknown rendering event command");
        os_log_error(log_, "Unknown rendering event command: %u", command);
        return false;
    }
}

bool sfb::AudioPlayer::processFramesRenderedEvent() noexcept {
    // The host time and rate scalar from the render cycle's timestamp
    uint64_t hostTime;
    double rateScalar;
    // The number of valid frames rendered
    uint32_t framesRendered;
    if (!renderingEvents_.readAll(hostTime, rateScalar, framesRendered)) {
        os_log_error(log_, "Missing timestamp or frames rendered for frames rendered event");
        return false;
    }

#if DEBUG
    assert(framesRendered > 0);
#endif /* DEBUG */

    // Perform bookkeeping to apportion the rendered frames appropriately
    //
    // framesRendered contains the number of valid frames that were rendered
    // but they could have come from multiple decoders

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

        AVAudioFramePosition framesRemainingToDistribute = framesRendered;

        auto iter = activeDecoders_.cbegin();
        while (iter != activeDecoders_.cend()) {
            const auto flags = (*iter)->loadFlags();

            // If a frames rendered event was posted it means valid frames were rendered
            // during that render cycle.
            //
            // However, between the time the frames rendered event was posted and when it is processed
            //   - A decoder may have been canceled
            //   - A seek can occur
            //
            // Bookkeeping is handled no differently for canceled decoders but rendering notifications are suppressed
            //
            // In the case of a seek the frames from that event are not valid and should be discarded.

            const auto decoderFramesConverted = (*iter)->framesConverted_.load(std::memory_order_acquire);
            const auto decoderFramesRendered = (*iter)->framesRendered_.load(std::memory_order_acquire);
            const auto decoderFramesRemaining = decoderFramesConverted - decoderFramesRendered;

            if (decoderFramesRemaining == 0) {
#if DEBUG
                os_log_debug(log_, "Not accounting for %lld frames in frames rendered event",
                             framesRemainingToDistribute);
#endif /* DEBUG */
                break;
            }

            // Rendering is starting
            if (bits::has_none(flags, DecoderState::Flags::isCanceled | DecoderState::Flags::renderingStarted)) {
                (*iter)->setFlags(DecoderState::Flags::renderingStarted);

                const auto frameOffset = framesRendered - framesRemainingToDistribute;
                const double deltaSeconds = frameOffset / (*iter)->sampleRate_;
                uint64_t eventTime =
                        hostTime + host_time::fromNanoseconds(static_cast<uint64_t>(deltaSeconds * rateScalar * 1e9));

                try {
                    queuedEvents.push_back({RenderingEventDetails::Type::willStart, (*iter)->decoder_, eventTime});
                } catch (const std::exception &e) {
                    os_log_error(log_, "Error queuing rendering will start event for %{public}@: %{public}s",
                                 (*iter)->decoder_, e.what());
                }
            }

            const auto framesFromThisDecoder = std::min(decoderFramesRemaining, framesRemainingToDistribute);

            (*iter)->framesRendered_.fetch_add(framesFromThisDecoder, std::memory_order_acq_rel);
            framesRemainingToDistribute -= framesFromThisDecoder;

            // Rendering is complete
            if (bits::is_set_without(flags, DecoderState::Flags::decodingComplete, DecoderState::Flags::isCanceled) &&
                framesFromThisDecoder == decoderFramesRemaining) {
                const auto frameOffset = framesRendered - framesRemainingToDistribute;
                const double deltaSeconds = frameOffset / (*iter)->sampleRate_;
                uint64_t eventTime =
                        hostTime + host_time::fromNanoseconds(static_cast<uint64_t>(deltaSeconds * rateScalar * 1e9));

                try {
                    queuedEvents.push_back({RenderingEventDetails::Type::willComplete, (*iter)->decoder_, eventTime});
                } catch (const std::exception &e) {
                    os_log_error(log_, "Error queuing rendering will complete event for %{public}@: %{public}s",
                                 (*iter)->decoder_, e.what());
                }

                os_log_debug(log_, "Deleting decoder state for %{public}@", (*iter)->decoder_);
                iter = activeDecoders_.erase(iter);
            } else {
                ++iter;
            }

            // All frames processed
            if (framesRemainingToDistribute == 0) {
                break;
            }
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
            assert(false && "Unknown RenderingEventDetails::Type");
        }
    }

    return true;
}

void sfb::AudioPlayer::handleRenderingWillStartEvent(Decoder decoder, uint64_t hostTime) noexcept {
    const auto now = host_time::current();
    if (now > hostTime) {
        os_log_error(log_, "Rendering started event processed %.2f msec late for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(now - hostTime)) / 1e6, decoder);
    } else {
#if DEBUG
        os_log_debug(log_, "Rendering will start in %.2f msec for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(hostTime - now)) / 1e6, decoder);
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
        const auto tolerance = static_cast<uint64_t>(1e9 / [that->sourceNode_ outputFormatForBus:0].sampleRate);
        if (delta > tolerance) {
            os_log_debug(log_, "Rendering started notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6,
                         now > hostTime ? "late" : "early");
        }
#endif /* DEBUG */

        that->setNowPlaying(decoder);

        if ([player.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)]) {
            [player.delegate audioPlayer:player renderingStarted:decoder];
        }
    });

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)]) {
        [player_.delegate audioPlayer:player_ renderingWillStart:decoder atHostTime:hostTime];
    }
}

void sfb::AudioPlayer::handleRenderingWillCompleteEvent(Decoder decoder, uint64_t hostTime) noexcept {
    const auto now = host_time::current();
    if (now > hostTime) {
        os_log_error(log_, "Rendering complete event processed %.2f msec late for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(now - hostTime)) / 1e6, decoder);
    } else {
#if DEBUG
        os_log_debug(log_, "Rendering will complete in %.2f msec for %{public}@",
                     static_cast<double>(host_time::toNanoseconds(hostTime - now)) / 1e6, decoder);
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
        const auto tolerance = static_cast<uint64_t>(1e9 / [that->sourceNode_ outputFormatForBus:0].sampleRate);
        if (delta > tolerance) {
            os_log_debug(log_, "Rendering complete notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6,
                         now > hostTime ? "late" : "early");
        }
#endif /* DEBUG */

        if ([player.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)]) {
            [player.delegate audioPlayer:player renderingComplete:decoder];
        }

        const auto hasNoDecoders = [&] {
            std::scoped_lock lock{that->queuedDecodersMutex_, that->activeDecodersMutex_};
            return that->queuedDecoders_.empty() && that->activeDecoders_.empty();
        }();

        // End of audio
        if (hasNoDecoders) {
#if DEBUG
            os_log_debug(log_, "End of audio reached");
#endif /* DEBUG */

            that->setNowPlaying(nil);

            if ([player.delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)]) {
                [player.delegate audioPlayerEndOfAudio:player];
            } else {
                const auto didStopEngine = stopEngineIfRunning();
                if (didStopEngine && [player_.delegate respondsToSelector:@selector(audioPlayer:
                                                                                  playbackStateChanged:)]) {
                    [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
                }
            }
        }
    });

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)]) {
        [player_.delegate audioPlayer:player_ renderingWillComplete:decoder atHostTime:hostTime];
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

    const auto iter = std::ranges::find_if(activeDecoders_, [](const auto &decoderState) {
        const auto flags = decoderState->loadFlags();
        return bits::is_clear(flags, DecoderState::Flags::isCanceled);
    });
    if (iter == activeDecoders_.cend()) {
        return nullptr;
    }
    return iter->get();
}

// MARK: - AVAudioEngine Notification Handling

void sfb::AudioPlayer::handleAudioEngineConfigurationChange(AVAudioEngine *engine, NSDictionary *userInfo) noexcept {
#pragma unused(userInfo)
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
        // Flags::engineIsRunning indicates if the engine was running before the interruption
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
                             "Mismatch between main mixer → output node connection channel count (%d) and hardware "
                             "channel count (%d)",
                             mixerNodeOutputFormat.channelCount, outputNodeOutputFormat.channelCount);
            }
            os_log_debug(log_, "Setting main mixer → output node connection format to %{public}@",
                         stringDescribingAVAudioFormat(outputNodeOutputFormat));
#endif /* DEBUG */

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
                if ([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                    [player_.delegate audioPlayer:player_ encounteredError:startError];
                }
                return;
            }

            // Restore previous playback state
            setFlags(prevState);
        }
    }

    if ([player_.delegate respondsToSelector:@selector(audioPlayer:audioEngineConfigurationChange:)]) {
        [player_.delegate audioPlayer:player_ audioEngineConfigurationChange:userInfo];
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

        if (preInterruptState_ != 0 && [player_.delegate respondsToSelector:@selector(audioPlayer:
                                                                                    playbackStateChanged:)]) {
            [player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
        }

        if ([player_.delegate respondsToSelector:@selector(audioPlayer:audioSessionInterruption:)]) {
            [player_.delegate audioPlayer:player_ audioSessionInterruption:userInfo];
        }

        break;
    }

    case AVAudioSessionInterruptionTypeEnded: {
        os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

        if ([player_.delegate respondsToSelector:@selector(audioPlayer:audioSessionInterruption:)]) {
            [player_.delegate audioPlayer:player_ audioSessionInterruption:userInfo];
        }

        if (const auto interruptionOption =
                    [[userInfo objectForKey:AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
            !(interruptionOption & AVAudioSessionInterruptionOptionShouldResume)) {
            return;
        }

        if (NSError *sessionError = nil; ![[AVAudioSession sharedInstance] setActive:YES error:&sessionError]) {
            os_log_error(log_, "Error activating AVAudioSession: %{public}@", sessionError);
            if ([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                [player_.delegate audioPlayer:player_ encounteredError:sessionError];
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
                    if ([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
                        [player_.delegate audioPlayer:player_ encounteredError:startError];
                    }
                    return;
                }
            }

            const auto prevFlags = setFlags(preInterruptState);
            assert(!bits::is_set_without(prevFlags, Flags::isPlaying, Flags::engineIsRunning));
        }

        if (preInterruptState_ && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)]) {
            [player_.delegate audioPlayer:player_
                     playbackStateChanged:static_cast<SFBAudioPlayerPlaybackState>(preInterruptState_)];
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

    // Allocate the ring buffer for the new format
    if (!audioRingBuffer_.allocate(*(format.streamDescription), ringBufferCapacity)) {
        os_log_error(log_,
                     "Unable to create audio ring buffer: spsc::AudioRingBuffer::allocate failed with format "
                     "%{public}@ and capacity %zu",
                     SFBASBDFormatDescription(format.streamDescription), ringBufferCapacity);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain
                                         code:SFBAudioPlayerErrorCodeInternalError
                                     userInfo:nil];
        }
        return false;
    }

    // Reconnect the source node to the next node in the processing chain
    // This is the mixer node in the default configuration, but additional nodes may
    // have been inserted between the source and mixer nodes. In this case allow the delegate
    // to make any necessary adjustments based on the format change if desired.
    if (AVAudioMixerNode *mixerNode = engine_.mainMixerNode;
        sourceNodeOutputConnectionPoint && sourceNodeOutputConnectionPoint.node != mixerNode) {
        if ([player_.delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
            AVAudioNode *node = [player_.delegate audioPlayer:player_
                                   reconfigureProcessingGraph:engine_
                                                   withFormat:format];
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
            // TODO: Re-evaluate whether failure to start AVAudioEngine during reconfiguration
            //       should be treated as a fatal error or handled as a recoverable condition,
            //       and document the chosen and tested behavior.
            if (error != nullptr) {
                *error = startError;
            }
            return false;
        }

        setFlags(prevState);
    }

    return true;
}
