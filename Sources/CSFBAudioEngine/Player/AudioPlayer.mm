//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <atomic>
#import <cassert>
#import <cmath>
#import <mutex>

#import <objc/runtime.h>

#import <CXXCoreAudio/CAChannelLayout.hpp>

#import <AVAudioFormat+SFBFormatTransformation.h>

#import "AudioPlayer.h"

#import "HostTimeUtilities.hpp"
#import "SFBAudioDecoder.h"
#import "SFBCStringForOSType.h"
#import "StringDescribingAVAudioFormat.h"

namespace {

/// The minimum number of frames to write to the ring buffer
constexpr AVAudioFrameCount kRingBufferChunkSize = 2048;

/// Objective-C associated object key indicating if a decoder has been canceled
constexpr char _decoderIsCanceledKey = '\0';

void AVAudioEngineConfigurationChangeNotificationCallback(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo)
{
	auto that = static_cast<SFB::AudioPlayer *>(observer);
	that->HandleAudioEngineConfigurationChange((__bridge AVAudioEngine *)object, (__bridge NSDictionary *)userInfo);
}

#if TARGET_OS_IPHONE
void AVAudioSessionInterruptionNotificationCallback(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo)
{
	auto that = static_cast<SFB::AudioPlayer *>(observer);
	that->HandleAudioSessionInterruption((__bridge NSDictionary *)userInfo);
}
#endif /* TARGET_OS_IPHONE */

#if !TARGET_OS_IPHONE
/// Returns the name of `audioUnit.deviceID`
///
/// This is the value of `kAudioObjectPropertyName` in the output scope on the main element
NSString * _Nullable AudioDeviceName(AUAudioUnit * _Nonnull audioUnit) noexcept
{
#if DEBUG
	assert(audioUnit != nil);
#endif /* DEBUG */

	AudioObjectPropertyAddress address = {
		.mSelector = kAudioObjectPropertyName,
		.mScope = kAudioObjectPropertyScopeOutput,
		.mElement = kAudioObjectPropertyElementMain
	};
	CFStringRef name = nullptr;
	UInt32 dataSize = sizeof(name);
	const auto result = AudioObjectGetPropertyData(audioUnit.deviceID, &address, 0, nullptr, &dataSize, &name);
	if(result != noErr) {
		os_log_error(SFB::AudioPlayer::log_, "AudioObjectGetPropertyData (kAudioObjectPropertyName, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}
	return (__bridge_transfer NSString *)name;
}
#endif /* !TARGET_OS_IPHONE */

} /* namespace */

namespace SFB {

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept
{
	static std::atomic_uint64_t nextIdentificationNumber = 1;
	static_assert(std::atomic_uint64_t::is_always_lock_free, "Lock-free std::atomic_uint64_t required");
	return nextIdentificationNumber.fetch_add(1, std::memory_order_relaxed);
}

const os_log_t AudioPlayer::log_ = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

// MARK: - Decoder State

/// State for tracking/syncing decoding progress
struct AudioPlayer::DecoderState final {
	/// Monotonically increasing instance counter
	const uint64_t			sequenceNumber_ 	{sequenceCounter_++};

	/// Decodes audio from the source representation to PCM
	const Decoder 			decoder_ 			{nil};

	/// The sample rate of the audio converter's output format
	const double 			sampleRate_ 		{0};

	/// Flags
	std::atomic_uint 		flags_ 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

	/// The number of frames decoded
	std::atomic_int64_t 	framesDecoded_ 		{0};
	/// The number of frames converted
	std::atomic_int64_t 	framesConverted_ 	{0};
	/// The number of frames rendered
	std::atomic_int64_t 	framesRendered_ 	{0};
	/// The total number of audio frames
	std::atomic_int64_t 	frameLength_ 		{0};
	/// The desired seek offset
	std::atomic_int64_t 	seekOffset_ 		{SFBUnknownFramePosition};

	static_assert(std::atomic_int64_t::is_always_lock_free, "Lock-free std::atomic_int64_t required");

	/// Converts audio from the decoder's processing format to another PCM variant at the same sample rate
	AVAudioConverter 		*converter_ 		{nil};
	/// Buffer used internally for buffering during conversion
	AVAudioPCMBuffer 		*decodeBuffer_ 		{nil};

	/// Next sequence number to use
	static uint64_t			sequenceCounter_;

	/// Possible bits in `flags_`
	enum class Flags : unsigned int {
		/// Decoding started
		decodingStarted 	= 1u << 0,
		/// Decoding complete
		decodingComplete 	= 1u << 1,
		/// Decoding was resumed after completion
		decodingResumed 	= 1u << 2,
		/// Decoding was suspended after starting
		decodingSuspended 	= 1u << 3,
		/// Rendering started
		renderingStarted 	= 1u << 4,
		/// Rendering complete
		renderingComplete 	= 1u << 5,
		/// A seek has been requested
		seekPending 		= 1u << 6,
		/// Decoder cancelation requested
		cancelRequested 	= 1u << 7,
		/// Decoder canceled
		isCanceled 			= 1u << 8,
	};

	DecoderState(Decoder _Nonnull decoder) noexcept
	: decoder_{decoder}, frameLength_{decoder.frameLength}, sampleRate_{decoder.processingFormat.sampleRate}
	{
#if DEBUG
		assert(decoder != nil);
#endif /* DEBUG */
	}

	bool Allocate(AVAudioFormat * _Nonnull format, AVAudioFrameCount frameCapacity = 1024) noexcept
	{
#if DEBUG
		assert(format != nil);
#endif /* DEBUG */

		converter_ = [[AVAudioConverter alloc] initFromFormat:decoder_.processingFormat toFormat:format];
		if(!converter_) {
			os_log_error(log_, "Error creating AVAudioConverter converting from %{public}@ to %{public}@", decoder_.processingFormat, format);
			return false;
		}

		// The logic in this class assumes no SRC is performed by mConverter
		assert(converter_.inputFormat.sampleRate == converter_.outputFormat.sampleRate);

		decodeBuffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter_.inputFormat frameCapacity:frameCapacity];
		if(!decodeBuffer_)
			return false;

		if(const auto framePosition = decoder_.framePosition; framePosition != 0) {
			framesDecoded_.store(framePosition, std::memory_order_release);
			framesConverted_.store(framePosition, std::memory_order_release);
			framesRendered_.store(framePosition, std::memory_order_release);
		}

		return true;
	}

	bool IsAllocated() const noexcept
	{
		return converter_ != nil;
	}

	AVAudioFramePosition FramePosition() const noexcept
	{
		return IsSeekPending() ? seekOffset_.load(std::memory_order_acquire) : framesRendered_.load(std::memory_order_acquire);
	}

	AVAudioFramePosition FrameLength() const noexcept
	{
		return frameLength_.load(std::memory_order_acquire);
	}

	bool DecodeAudio(AVAudioPCMBuffer * _Nonnull buffer, NSError **error = nullptr) noexcept
	{
#if DEBUG
		assert(buffer != nil);
		assert(buffer.frameCapacity == decodeBuffer_.frameCapacity);
#endif /* DEBUG */

		if(![decoder_ decodeIntoBuffer:decodeBuffer_ frameLength:decodeBuffer_.frameCapacity error:error])
			return false;

		if(decodeBuffer_.frameLength == 0) {
			flags_.fetch_or(static_cast<unsigned int>(Flags::decodingComplete), std::memory_order_acq_rel);

#if false
			// Some formats may not know the exact number of frames in advance
			// without processing the entire file, which is a potentially slow operation
			mFrameLength.store(mDecoder.framePosition, std::memory_order_release);
#endif /* false */

			buffer.frameLength = 0;
			return true;
		}

		this->framesDecoded_.fetch_add(decodeBuffer_.frameLength, std::memory_order_acq_rel);

		// Only PCM to PCM conversions are performed
		if(![converter_ convertToBuffer:buffer fromBuffer:decodeBuffer_ error:error])
			return false;
		framesConverted_.fetch_add(buffer.frameLength, std::memory_order_acq_rel);

		// If `buffer` is not full but -decodeIntoBuffer:frameLength:error: returned `YES`
		// decoding is complete
		if(buffer.frameLength != buffer.frameCapacity)
			flags_.fetch_or(static_cast<unsigned int>(Flags::decodingComplete), std::memory_order_acq_rel);

		return true;
	}

	/// Returns `true` if `Flags::decodingComplete` is set
	bool IsDecodingComplete() const noexcept
	{
		return flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::decodingComplete);
	}

	/// Returns `true` if `Flags::renderingStarted` is set
	bool HasRenderingStarted() const noexcept
	{
		return flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::renderingStarted);
	}

	/// Returns the number of frames available to render.
	///
	/// This is the difference between the number of frames converted and the number of frames rendered
	AVAudioFramePosition FramesAvailableToRender() const noexcept
	{
		return framesConverted_.load(std::memory_order_acquire) - framesRendered_.load(std::memory_order_acquire);
	}

	/// Returns `true` if there are no frames available to render.
	bool AllAvailableFramesRendered() const noexcept
	{
		return FramesAvailableToRender() == 0;
	}

	/// Returns the number of frames rendered.
	AVAudioFramePosition FramesRendered() const noexcept
	{
		return framesRendered_.load(std::memory_order_acquire);
	}

	/// Adds `count` number of frames to the total count of frames rendered.
	void AddFramesRendered(AVAudioFramePosition count) noexcept
	{
		framesRendered_.fetch_add(count, std::memory_order_acq_rel);
	}

	/// Returns `true` if `Flags::seekPending` is set
	bool IsSeekPending() const noexcept
	{
		return flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::seekPending);
	}

	/// Sets the pending seek request to `frame`
	void RequestSeekToFrame(AVAudioFramePosition frame) noexcept
	{
		seekOffset_.store(frame, std::memory_order_release);
		flags_.fetch_or(static_cast<unsigned int>(Flags::seekPending), std::memory_order_acq_rel);
	}

	/// Performs the pending seek request, if present
	bool PerformSeekIfRequired() noexcept
	{
		if(!IsSeekPending())
			return true;

		auto seekOffset = seekOffset_.load(std::memory_order_acquire);
		os_log_debug(log_, "Seeking to frame %lld in %{public}@ ", seekOffset, decoder_);

		if([decoder_ seekToFrame:seekOffset error:nil])
			// Reset the converter to flush any buffers
			[converter_ reset];
		else
			os_log_debug(log_, "Error seeking to frame %lld", seekOffset);

		const auto newFrame = decoder_.framePosition;
		if(newFrame != seekOffset) {
			os_log_debug(log_, "Inaccurate seek to frame %lld, got %lld", seekOffset, newFrame);
			seekOffset = newFrame;
		}

		// Clear the seek request
		flags_.fetch_and(~static_cast<unsigned int>(Flags::seekPending), std::memory_order_acq_rel);

		// Update the frame counters accordingly
		// A seek is handled in essentially the same way as initial playback
		if(newFrame != SFBUnknownFramePosition) {
			framesDecoded_.store(newFrame, std::memory_order_release);
			framesConverted_.store(seekOffset, std::memory_order_release);
			framesRendered_.store(seekOffset, std::memory_order_release);
		}

		return newFrame != SFBUnknownFramePosition;
	}
};

uint64_t AudioPlayer::DecoderState::sequenceCounter_ = 1;

} /* namespace SFB */

// MARK: - AudioPlayer

SFB::AudioPlayer::AudioPlayer()
{
	// ========================================
	// Rendering Setup

	// Start out with 44.1 kHz stereo
	renderingFormat_ = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
	if(!renderingFormat_) {
		os_log_error(log_, "Unable to create AVAudioFormat for 44.1 kHz stereo");
		throw std::runtime_error("Unable to create AVAudioFormat");
	}

	// Allocate the audio ring buffer moving audio from the decoder queue to the render block
	if(!audioRingBuffer_.Allocate(*(renderingFormat_.streamDescription), 16384)) {
		os_log_error(log_, "Unable to create audio ring buffer: CXXCoreAudio::AudioRingBuffer::Allocate failed");
		throw std::runtime_error("CXXCoreAudio::AudioRingBuffer::Allocate failed");
	}

	// ========================================
	// Event Processing Setup

	// The decode event ring buffer is written to by the decoding thread and read from by the event queue
	if(!decodeEventRingBuffer_.Allocate(1024)) {
		os_log_error(log_, "Unable to create decode event ring buffer: SFB::RingBuffer::Allocate failed");
		throw std::runtime_error("SFB::RingBuffer::Allocate failed");
	}

	decodingSemaphore_ = dispatch_semaphore_create(0);
	if(!decodingSemaphore_) {
		os_log_error(log_, "Unable to create decode event semaphore: dispatch_semaphore_create failed");
		throw std::runtime_error("Unable to create decode event dispatch semaphore");
	}

	// The render event ring buffer is written to by the render block and read from by the event queue
	if(!renderEventRingBuffer_.Allocate(1024)) {
		os_log_error(log_, "Unable to create render event ring buffer: SFB::RingBuffer::Allocate failed");
		throw std::runtime_error("SFB::RingBuffer::Allocate failed");
	}

	eventSemaphore_ = dispatch_semaphore_create(0);
	if(!eventSemaphore_) {
		os_log_error(log_, "Unable to create render event semaphore: dispatch_semaphore_create failed");
		throw std::runtime_error("Unable to create render event dispatch semaphore");
	}

	// Create the dispatch queue used for event processing
	auto attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
	if(!attr) {
		os_log_error(log_, "dispatch_queue_attr_make_with_qos_class failed");
		throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
	}

	eventQueue_ = dispatch_queue_create_with_target("AudioPlayer.Events", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
	if(!eventQueue_) {
		os_log_error(log_, "Unable to create event dispatch queue: dispatch_queue_create failed");
		throw std::runtime_error("dispatch_queue_create_with_target failed");
	}

	// Launch the decoding and event processing threads
	try {
		decodingThread_ = std::jthread(std::bind_front(&SFB::AudioPlayer::ProcessDecoders, this));
		eventThread_ = std::jthread(std::bind_front(&SFB::AudioPlayer::SequenceAndProcessEvents, this));
	} catch(const std::exception& e) {
		os_log_error(log_, "Unable to create thread: %{public}s", e.what());
		throw;
	}

	// ========================================
	// Audio Processing Graph Setup

	engine_ = [[AVAudioEngine alloc] init];
	if(!engine_) {
		os_log_error(log_, "Unable to create AVAudioEngine instance");
		throw std::runtime_error("Unable to create AVAudioEngine");
	}

	sourceNode_ = [[AVAudioSourceNode alloc] initWithFormat:renderingFormat_
												renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
		return Render(*isSilence, *timestamp, frameCount, outputData);
	}];
	if(!sourceNode_)
		throw std::runtime_error("Unable to create AVAudioSourceNode instance");

	[engine_ attachNode:sourceNode_];
	[engine_ connect:sourceNode_ to:engine_.mainMixerNode format:renderingFormat_];
	[engine_ prepare];

	os_log_debug(log_, "Created <AudioPlayer: %p>, rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(renderingFormat_));

#if DEBUG
	LogProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

	// Register for configuration change notifications
	auto notificationCenter = CFNotificationCenterGetLocalCenter();
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioEngineConfigurationChangeNotificationCallback, (__bridge CFStringRef)AVAudioEngineConfigurationChangeNotification, (__bridge void *)engine_, CFNotificationSuspensionBehaviorDeliverImmediately);

#if TARGET_OS_IPHONE
	// Register for audio session interruption notifications
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioSessionInterruptionNotificationCallback, (__bridge CFStringRef)AVAudioSessionInterruptionNotification, (__bridge void *)[AVAudioSession sharedInstance], CFNotificationSuspensionBehaviorDeliverImmediately);
#endif /* TARGET_OS_IPHONE */
}

SFB::AudioPlayer::~AudioPlayer() noexcept
{
	auto notificationCenter = CFNotificationCenterGetLocalCenter();
	CFNotificationCenterRemoveEveryObserver(notificationCenter, this);

	Stop();

	// Register a stop callback for the decoding thread
	std::stop_callback decodingThreadStopCallback(decodingThread_.get_stop_token(), [this] {
		dispatch_semaphore_signal(decodingSemaphore_);
	});

	// Issue a stop request to the decoding thread and wait for it to exit
	decodingThread_.request_stop();
	try {
		decodingThread_.join();
	} catch(const std::exception& e) {
		os_log_error(log_, "Unable to join decoding thread: %{public}s", e.what());
	}

	// Register a stop callback for the event processing thread
	std::stop_callback eventThreadStopCallback(eventThread_.get_stop_token(), [this] {
		dispatch_semaphore_signal(eventSemaphore_);
	});

	// Issue a stop request to the event processing thread and wait for it to exit
	eventThread_.request_stop();
	try {
		eventThread_.join();
	} catch(const std::exception& e) {
		os_log_error(log_, "Unable to join event processing thread: %{public}s", e.what());
	}

	// Delete any remaining decoder state
	activeDecoders_.clear();

	os_log_debug(log_, "<AudioPlayer: %p> destroyed", this);
}

// MARK: - Playlist Management

bool SFB::AudioPlayer::EnqueueDecoder(Decoder decoder, bool forImmediatePlayback, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return false;

	// Ensure only one decoder can be enqueued at a time
	std::lock_guard lock{lock_};

	// Reconfigure the audio processing graph for the decoder's processing format if required
	const auto reconfigure = [&] {
		std::scoped_lock lock{queueLock_, decoderLock_};
		return queuedDecoders_.empty() && activeDecoders_.empty();
	}();

	if(reconfigure) {
		flags_.fetch_or(static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		if(!ConfigureProcessingGraphAndRingBufferForDecoder(decoder, error)) {
			flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
			return false;
		}
	}

	if(forImmediatePlayback) {
		// Mute until the decoder becomes active to prevent spurious events
		flags_.fetch_or(static_cast<unsigned int>(Flags::isMuted), std::memory_order_acq_rel);
		ClearDecoderQueue();
		CancelActiveDecoders(true);
		flags_.fetch_or(static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);
	}

	if(!PushDecoderToQueue(decoder)) {
		flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		if(forImmediatePlayback)
			flags_.fetch_and(~static_cast<unsigned int>(Flags::isMuted) & ~static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);
		return false;
	}

	os_log_info(log_, "Enqueued %{public}@", decoder);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::FormatWillBeGaplessIfEnqueued(AVAudioFormat *format) const noexcept
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */
	// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
	return format.channelCount == renderingFormat_.channelCount && format.sampleRate == renderingFormat_.sampleRate && CXXCoreAudio::AVAudioChannelLayoutsAreEquivalent(format.channelLayout, renderingFormat_.channelLayout);
}

bool SFB::AudioPlayer::PushDecoderToQueue(Decoder decoder) noexcept
{
	try {
		std::lock_guard lock(queueLock_);
		queuedDecoders_.push_back(decoder);
	} catch(const std::exception& e) {
		os_log_error(log_, "Error pushing %{public}@ to queuedDecoders_: %{public}s", decoder, e.what());
		return false;
	}

	return true;
}

SFB::AudioPlayer::Decoder SFB::AudioPlayer::PopDecoderFromQueue() noexcept
{
	Decoder decoder = nil;
	std::lock_guard lock(queueLock_);
	if(!queuedDecoders_.empty()) {
		decoder = queuedDecoders_.front();
		queuedDecoders_.pop_front();
	}
	return decoder;
}

SFB::AudioPlayer::Decoder SFB::AudioPlayer::CurrentDecoder() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return nil;
	return decoderState->decoder_;
}

void SFB::AudioPlayer::CancelActiveDecoders(bool cancelAllActive) noexcept
{
	std::lock_guard lock(decoderLock_);

	// Cancel all active decoders in sequence
	if(auto decoderState = FirstDecoderStateWithRenderingNotComplete(); decoderState) {
		decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::cancelRequested), std::memory_order_acq_rel);
		if(cancelAllActive) {
			decoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
			while(decoderState) {
				decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::cancelRequested), std::memory_order_acq_rel);
				decoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
			}
		}

		dispatch_semaphore_signal(decodingSemaphore_);
	}
}

void SFB::AudioPlayer::Reset() noexcept
{
	[engine_ reset];
	ClearDecoderQueue();
	CancelActiveDecoders(true);
}

// MARK: - Playback Control

bool SFB::AudioPlayer::Play(NSError **error) noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning))) {
		if(NSError *err = nil; ![engine_ startAndReturnError:&err]) {
			flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return false;
		}
		flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
	} else
		flags_.fetch_or(static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Play()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];

	return true;
}

void SFB::AudioPlayer::Pause() noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	flags_.fetch_and(~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePaused && "Incorrect playback state in Pause()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
}

void SFB::AudioPlayer::Resume() noexcept
{
	if(const auto flags = flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::engineIsRunning)) && !(flags & static_cast<unsigned int>(Flags::isPlaying)))
		return;

	flags_.fetch_or(static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Resume()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
}

void SFB::AudioPlayer::Stop() noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	[engine_ stop];
	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning) & ~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

	ClearDecoderQueue();
	CancelActiveDecoders(true);

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStateStopped && "Incorrect playback state in Stop()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
}

bool SFB::AudioPlayer::TogglePlayPause(NSError **error) noexcept
{
	const auto playbackState = PlaybackState();
	switch(playbackState) {
		case SFBAudioPlayerPlaybackStatePlaying:
			Pause();
			return true;
		case SFBAudioPlayerPlaybackStatePaused:
			Resume();
			return true;
		case SFBAudioPlayerPlaybackStateStopped:
			return Play(error);
	}
}

// MARK: - Player State

bool SFB::AudioPlayer::EngineIsRunning() const noexcept
{
	const auto isRunning = engine_.isRunning;
#if DEBUG
		assert(static_cast<bool>(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) == isRunning && "Cached value for engine_.isRunning invalid");
#endif /* DEBUG */
	return isRunning;
}

void SFB::AudioPlayer::SetNowPlaying(Decoder nowPlaying) noexcept
{
	Decoder previouslyPlaying = nil;
	{
		std::lock_guard lock(nowPlayingLock_);
		if(nowPlaying_ == nowPlaying)
			return;
		previouslyPlaying = nowPlaying_;
		nowPlaying_ = nowPlaying;
	}

	os_log_debug(log_, "Now playing changed to %{public}@", nowPlaying);

	if([player_.delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:previouslyPlaying:)])
		[player_.delegate audioPlayer:player_ nowPlayingChanged:nowPlaying previouslyPlaying:previouslyPlaying];
}

// MARK: - Playback Properties

SFBPlaybackPosition SFB::AudioPlayer::PlaybackPosition() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return SFBInvalidPlaybackPosition;
	return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
}

SFBPlaybackTime SFB::AudioPlayer::PlaybackTime() const noexcept
{
	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return SFBInvalidPlaybackTime;

	SFBPlaybackTime playbackTime = SFBInvalidPlaybackTime;

	const auto framePosition = decoderState->FramePosition();
	const auto frameLength = decoderState->FrameLength();

	if(const auto sampleRate = decoderState->sampleRate_; sampleRate > 0) {
		if(framePosition != SFBUnknownFramePosition)
			playbackTime.currentTime = framePosition / sampleRate;
		if(frameLength != SFBUnknownFrameLength)
			playbackTime.totalTime = frameLength / sampleRate;
	}

	return playbackTime;
}

bool SFB::AudioPlayer::GetPlaybackPositionAndTime(SFBPlaybackPosition *playbackPosition, SFBPlaybackTime *playbackTime) const noexcept
{
	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState) {
		if(playbackPosition)
			*playbackPosition = SFBInvalidPlaybackPosition;
		if(playbackTime)
			*playbackTime = SFBInvalidPlaybackTime;
		return false;
	}

	SFBPlaybackPosition currentPlaybackPosition = { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
	if(playbackPosition)
		*playbackPosition = currentPlaybackPosition;

	if(playbackTime) {
		SFBPlaybackTime currentPlaybackTime = SFBInvalidPlaybackTime;
		if(const auto sampleRate = decoderState->sampleRate_; sampleRate > 0) {
			if(currentPlaybackPosition.framePosition != SFBUnknownFramePosition)
				currentPlaybackTime.currentTime = currentPlaybackPosition.framePosition / sampleRate;
			if(currentPlaybackPosition.frameLength != SFBUnknownFrameLength)
				currentPlaybackTime.totalTime = currentPlaybackPosition.frameLength / sampleRate;
		}
		*playbackTime = currentPlaybackTime;
	}

	return true;
}

// MARK: - Seeking

bool SFB::AudioPlayer::SeekForward(NSTimeInterval secondsToSkip) noexcept
{
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->decoder_.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->sampleRate_;
	const auto framePosition = decoderState->FramePosition();
	const auto frameLength = decoderState->FrameLength();

	auto targetFrame = framePosition + static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);
	if(targetFrame >= frameLength)
		targetFrame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(targetFrame);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::SeekBackward(NSTimeInterval secondsToSkip) noexcept
{
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->decoder_.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->sampleRate_;
	const auto framePosition = decoderState->FramePosition();

	auto targetFrame = framePosition - static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);
	if(targetFrame < 0)
		targetFrame = 0;

	decoderState->RequestSeekToFrame(targetFrame);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::SeekToTime(NSTimeInterval timeInSeconds) noexcept
{
	if(timeInSeconds < 0)
		timeInSeconds = 0;

	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->decoder_.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->sampleRate_;
	const auto frameLength = decoderState->FrameLength();

	auto targetFrame = static_cast<AVAudioFramePosition>(timeInSeconds * sampleRate);
	if(targetFrame >= frameLength)
		targetFrame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(targetFrame);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::SeekToPosition(double position) noexcept
{
	if(position < 0)
		position = 0;
	else if(position >= 1)
		position = std::nextafter(1.0, 0.0);

	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->decoder_.supportsSeeking)
		return false;

	const auto frameLength = decoderState->FrameLength();
	const auto targetFrame = static_cast<AVAudioFramePosition>(frameLength * position);

	decoderState->RequestSeekToFrame(targetFrame);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::SeekToFrame(AVAudioFramePosition frame) noexcept
{
	if(frame < 0)
		frame = 0;

	std::lock_guard lock(decoderLock_);

	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->decoder_.supportsSeeking)
		return false;

	const auto frameLength = decoderState->FrameLength();
	if(frame >= frameLength)
		frame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(frame);
	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

bool SFB::AudioPlayer::SupportsSeeking() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return false;
	return decoderState->decoder_.supportsSeeking;
}

#if !TARGET_OS_IPHONE

// MARK: - Volume Control

float SFB::AudioPlayer::VolumeForChannel(AudioObjectPropertyElement channel) const noexcept
{
	AudioUnitParameterValue volume;
	const auto result = AudioUnitGetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(result != noErr) {
		os_log_error(log_, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		return std::nanf("1");
	}

	return volume;
}

bool SFB::AudioPlayer::SetVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept
{
	os_log_info(log_, "Setting volume for channel %u to %g", channel, volume);

	const auto result = AudioUnitSetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(result != noErr) {
		os_log_error(log_, "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return false;
	}

	return true;
}

// MARK: - Output Device

AUAudioObjectID SFB::AudioPlayer::OutputDeviceID() const noexcept
{
	return engine_.outputNode.AUAudioUnit.deviceID;
}

bool SFB::AudioPlayer::SetOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept
{
	os_log_info(log_, "Setting output device to 0x%x", outputDeviceID);

	if(NSError *err = nil; ![engine_.outputNode.AUAudioUnit setDeviceID:outputDeviceID error:&err]) {
		os_log_error(log_, "Error setting output device: %{public}@", err);
		if(error)
			*error = err;
		return false;
	}

	return true;
}

#endif /* !TARGET_OS_IPHONE */

// MARK: - Debugging

void SFB::AudioPlayer::LogProcessingGraphDescription(os_log_t log, os_log_type_t type) const noexcept
{
	NSMutableString *string = [NSMutableString stringWithFormat:@"<AudioPlayer: %p> audio processing graph:\n", this];

	const auto engine = engine_;
	const auto sourceNode = sourceNode_;

	AVAudioFormat *inputFormat = renderingFormat_;
	[string appendFormat:@"↓ rendering\n    %@\n", SFB::StringDescribingAVAudioFormat(inputFormat)];

	AVAudioFormat *outputFormat = [sourceNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@\n", sourceNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@\n", sourceNode];

	AVAudioConnectionPoint *connectionPoint = [[engine outputConnectionPointsForNode:sourceNode outputBus:0] firstObject];
	while(connectionPoint.node != engine.mainMixerNode) {
		inputFormat = [connectionPoint.node inputFormatForBus:connectionPoint.bus];
		outputFormat = [connectionPoint.node outputFormatForBus:connectionPoint.bus];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", connectionPoint.node, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", connectionPoint.node];

		connectionPoint = [[engine outputConnectionPointsForNode:connectionPoint.node outputBus:0] firstObject];
	}

	inputFormat = [engine.mainMixerNode inputFormatForBus:0];
	outputFormat = [engine.mainMixerNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@\n", engine.mainMixerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@\n", engine.mainMixerNode];

	inputFormat = [engine.outputNode inputFormatForBus:0];
	outputFormat = [engine.outputNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@]", engine.outputNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@", engine.outputNode];

#if !TARGET_OS_IPHONE
	[string appendFormat:@"\n↓ \"%@\"", AudioDeviceName(engine.outputNode.AUAudioUnit)];
#endif /* !TARGET_OS_IPHONE */

	os_log_with_type(log, type, "%{public}@", string);
}

// MARK: - Decoding

void SFB::AudioPlayer::ProcessDecoders(std::stop_token stoken) noexcept
{
	pthread_setname_np("AudioPlayer.Decoding");
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

	os_log_debug(log_, "Decoding thread starting");

	// The buffer between the decoder state and the ring buffer
	AVAudioPCMBuffer *buffer = nil;

	for(;;) {
		// The decoder state being processed
		DecoderState *decoderState = nullptr;
		auto ringBufferStale = false;

		// Get the earliest decoder state that has not completed rendering
		{
			std::lock_guard lock(decoderLock_);
			decoderState = FirstDecoderStateWithRenderingNotComplete();

			// Process cancelations
			while(decoderState && (decoderState->flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(DecoderState::Flags::cancelRequested))) {
				os_log_debug(log_, "Canceling decoding for %{public}@", decoderState->decoder_);

				ringBufferStale = true;
				decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::isCanceled), std::memory_order_acq_rel);

				// Submit the decoder canceled event
				const DecodingEventHeader header{DecodingEventCommand::canceled};
				if(decodeEventRingBuffer_.WriteValues(header, decoderState->sequenceNumber_))
					dispatch_semaphore_signal(eventSemaphore_);
				else
					os_log_error(log_, "Error writing decoder canceled event");

				decoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
			}
		}

		// Terminate the thread if requested after processing cancelations
		if(stoken.stop_requested())
			break;

		// Process pending seeks
		if(decoderState && decoderState->IsSeekPending()) {
			ringBufferStale = true;

			decoderState->PerformSeekIfRequired();

			if(decoderState->IsDecodingComplete()) {
				os_log_debug(log_, "Resuming decoding for %{public}@", decoderState->decoder_);

				decoderState->flags_.fetch_and(~static_cast<unsigned int>(DecoderState::Flags::decodingComplete), std::memory_order_acq_rel);
				decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::decodingResumed), std::memory_order_acq_rel);

				DecoderState *nextDecoderState = nullptr;
				{
					std::lock_guard lock(decoderLock_);
					nextDecoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
				}

				// Rewind ensuing decoder states if possible to avoid discarding frames
				while(nextDecoderState && (nextDecoderState->flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(DecoderState::Flags::decodingStarted))) {
					os_log_debug(log_, "Suspending decoding for %{public}@", nextDecoderState->decoder_);

					// TODO: Investigate a per-state buffer to mitigate frame loss
					if(nextDecoderState->decoder_.supportsSeeking) {
						nextDecoderState->RequestSeekToFrame(0);
						nextDecoderState->PerformSeekIfRequired();
					} else
						os_log_error(log_, "Discarding %lld frames from %{public}@", nextDecoderState->framesDecoded_.load(std::memory_order_acquire), nextDecoderState->decoder_);

					nextDecoderState->flags_.fetch_and(~static_cast<unsigned int>(DecoderState::Flags::decodingStarted), std::memory_order_acq_rel);
					nextDecoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::decodingSuspended), std::memory_order_acq_rel);

					{
						std::lock_guard lock(decoderLock_);
						nextDecoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(nextDecoderState->sequenceNumber_);
					}
				}
			}
		}

		// Request a drain of the ring buffer during the next render cycle to prevent audible artifacts from seeking or cancellation
		if(ringBufferStale)
			flags_.fetch_or(static_cast<unsigned int>(Flags::drainRequired), std::memory_order_acq_rel);

		// Get the earliest decoder state that has not completed decoding
		{
			std::lock_guard lock(decoderLock_);
			decoderState = FirstDecoderStateWithDecodingNotComplete();
		}

		// Dequeue the next decoder if there are no decoders that haven't completed decoding
		if(!decoderState) {
			if(auto decoder = PopDecoderFromQueue(); decoder) {
				try {
					// Create the decoder state and add it to the list of active decoders
					std::lock_guard lock(decoderLock_);
					activeDecoders_.push_back(std::make_unique<DecoderState>(decoder));
					decoderState = activeDecoders_.back().get();
				} catch(const std::exception& e) {
					os_log_error(log_, "Error creating decoder state for %{public}@: %{public}s", decoder, e.what());
					NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeInternalError userInfo:nil];
					SubmitDecodingErrorEvent(error);
					continue;
				}

				// Start decoding immediately if the join will be gapless (same sample rate, channel count, and channel layout)
				if(auto format = decoder.processingFormat; FormatWillBeGaplessIfEnqueued(format)) {
					if(!decoderState->Allocate(renderingFormat_, kRingBufferChunkSize)) {
						decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::isCanceled), std::memory_order_acq_rel);
						NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeInternalError userInfo:nil];
						SubmitDecodingErrorEvent(error);
						continue;
					}

					// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
					if(!buffer) {
						buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderingFormat_ frameCapacity:kRingBufferChunkSize];
						if(!buffer) {
							os_log_error(log_, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(renderingFormat_), kRingBufferChunkSize);
							NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeInternalError userInfo:nil];
							SubmitDecodingErrorEvent(error);
							continue;
						}
					}
				} else {
					// If the next decoder cannot be gaplessly joined set the mismatch flag and wait;
					// decoding can't start until the processing graph is reconfigured which occurs after
					// all active decoders complete
					flags_.fetch_or(static_cast<unsigned int>(Flags::formatMismatch), std::memory_order_acq_rel);
					os_log_debug(log_, "Non-gapless join for %{public}@", decoder);
				}

				// Clear the mute flags if needed
				if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::unmuteAfterDequeue))
					flags_.fetch_and(~static_cast<unsigned int>(Flags::isMuted) & ~static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);

				os_log_debug(log_, "Dequeued %{public}@, processing format %{public}@", decoderState->decoder_, SFB::StringDescribingAVAudioFormat(decoderState->decoder_.processingFormat));
			}
		}

		// If there a format mismatch the processing graph requires reconfiguration before decoding can begin
		if(decoderState && (flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::formatMismatch))) {
			// Wait until all other decoders complete processing before reconfiguring the graph
			const auto okToReconfigure = [&] {
				std::lock_guard lock(decoderLock_);
				return activeDecoders_.size() == 1;
			}();

			if(okToReconfigure) {
				flags_.fetch_and(~static_cast<unsigned int>(Flags::formatMismatch), std::memory_order_release);

				{
					std::lock_guard lock{lock_};
					NSError *error = nil;
					if(!ConfigureProcessingGraphAndRingBufferForDecoder(decoderState->decoder_, &error)) {
						decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::isCanceled), std::memory_order_acq_rel);
						SubmitDecodingErrorEvent(error);
						continue;
					}
				}

				if(!decoderState->Allocate(renderingFormat_, kRingBufferChunkSize)) {
					decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::isCanceled), std::memory_order_acq_rel);
					NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeInternalError userInfo:nil];
					SubmitDecodingErrorEvent(error);
					continue;
				}

				// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
				if(AVAudioFormat *format = buffer.format; !buffer || format.channelCount != renderingFormat_.channelCount || format.sampleRate != renderingFormat_.sampleRate) {
					buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderingFormat_ frameCapacity:kRingBufferChunkSize];
					if(!buffer) {
						os_log_error(log_, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(renderingFormat_), kRingBufferChunkSize);
						NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeInternalError userInfo:nil];
						SubmitDecodingErrorEvent(error);
						continue;
					}
				}
			}
			else
				decoderState = nullptr;
		}

		if(decoderState && !(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::drainRequired))) {
			// Decode and write chunks to the ring buffer
			while(audioRingBuffer_.FreeSpace() >= kRingBufferChunkSize) {
				// Decoding started
				if(const auto flags = decoderState->flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(DecoderState::Flags::decodingStarted))) {
					const bool suspended = flags & static_cast<unsigned int>(DecoderState::Flags::decodingSuspended);

					if(!suspended)
						os_log_debug(log_, "Decoding starting for %{public}@", decoderState->decoder_);
					else
						os_log_debug(log_, "Decoding starting after suspension for %{public}@", decoderState->decoder_);

					decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::decodingStarted), std::memory_order_acq_rel);

					// Submit the decoding started event for the initial start only
					if(!suspended) {
						const DecodingEventHeader header{DecodingEventCommand::started};
						if(decodeEventRingBuffer_.WriteValues(header, decoderState->sequenceNumber_))
							dispatch_semaphore_signal(eventSemaphore_);
						else
							os_log_error(log_, "Error writing decoding started event");
					}
				}

				// Decode audio into the buffer, converting to the rendering format in the process
				if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
					os_log_error(log_, "Error decoding audio: %{public}@", error);
					if(error)
						SubmitDecodingErrorEvent(error);
				}

				// Write the decoded audio to the ring buffer for rendering
				const auto framesWritten = audioRingBuffer_.Write(buffer.audioBufferList, buffer.frameLength);
				if(framesWritten != buffer.frameLength)
					os_log_error(log_, "CXXCoreAudio::AudioRingBuffer::Write() failed");

				// Decoding complete
				if(const auto flags = decoderState->flags_.load(std::memory_order_acquire); flags & static_cast<unsigned int>(DecoderState::Flags::decodingComplete)) {
					const bool resumed = flags & static_cast<unsigned int>(DecoderState::Flags::decodingResumed);

					// Submit the decoding complete event for the first completion only
					if(!resumed) {
						const DecodingEventHeader header{DecodingEventCommand::complete};
						if(decodeEventRingBuffer_.WriteValues(header, decoderState->sequenceNumber_))
							dispatch_semaphore_signal(eventSemaphore_);
						else
							os_log_error(log_, "Error writing decoding complete event");
					}

					if(!resumed)
						os_log_debug(log_, "Decoding complete for %{public}@", decoderState->decoder_);
					else
						os_log_debug(log_, "Decoding complete after resuming for %{public}@", decoderState->decoder_);

					break;
				}
			}
		}

		int64_t deltaNanos;
		// Idling
		if(!decoderState)
			deltaNanos = NSEC_PER_SEC / 2;
		// Determine timeout based on ring buffer free space
		else {
			// Attempt to keep the ring buffer 75% full
			const auto targetMaxFreeSpace = audioRingBuffer_.Capacity() / 4;
			const auto freeSpace = audioRingBuffer_.FreeSpace();

			// Minimal timeout if the ring buffer has more free space than desired
			if(freeSpace > targetMaxFreeSpace)
				deltaNanos =  2.5 * NSEC_PER_MSEC;
			else {
				const auto duration = (targetMaxFreeSpace - freeSpace) / audioRingBuffer_.Format().mSampleRate;
				deltaNanos = duration * NSEC_PER_SEC;
			}
		}

		// Wait for an event signal; ring buffer space availability is polled using the timeout
		dispatch_semaphore_wait(decodingSemaphore_, dispatch_time(DISPATCH_TIME_NOW, deltaNanos));
	}

	os_log_debug(log_, "Decoding thread complete");
}

void SFB::AudioPlayer::SubmitDecodingErrorEvent(NSError *error) noexcept
{
#if DEBUG
	assert(error != nil);
#endif /* DEBUG */

	NSError *err = nil;
	NSData *errorData = [NSKeyedArchiver archivedDataWithRootObject:error requiringSecureCoding:YES error:&err];
	if(!errorData) {
		os_log_error(log_, "Error archiving NSError for decoding error event: %{public}@", err);
		return;
	}

	auto [front, back] = decodeEventRingBuffer_.GetWriteVector();

	const auto frontSize = front.size();
	const auto spaceNeeded = sizeof(DecodingEventHeader) + sizeof(uint32_t) + errorData.length;
	if(frontSize + back.size() < spaceNeeded) {
		os_log_fault(log_, "Insufficient space to write decoding error event");
		return;
	}

	std::size_t cursor = 0;
	auto write_single_arg = [&](const void *arg, std::size_t len) noexcept {
		const auto *src = static_cast<const unsigned char *>(arg);
		if(cursor + len <= frontSize)
			std::memcpy(front.data() + cursor, src, len);
		else if(cursor >= frontSize)
			std::memcpy(back.data() + (cursor - frontSize), src, len);
		else {
			const size_t toFront = frontSize - cursor;
			std::memcpy(front.data() + cursor, src, toFront);
			std::memcpy(back.data(), src + toFront, len - toFront);
		}
		cursor += len;
	};

	// Event header and payload
	const DecodingEventHeader header{DecodingEventCommand::error};
	const auto dataSize = static_cast<uint32_t>(errorData.length);

	write_single_arg(&header, sizeof header);
	write_single_arg(&dataSize, sizeof dataSize);
	write_single_arg(errorData.bytes, errorData.length);

	decodeEventRingBuffer_.CommitWrite(cursor);
	dispatch_semaphore_signal(eventSemaphore_);
}

// MARK: - Rendering

OSStatus SFB::AudioPlayer::Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) noexcept
{
	const auto flags = flags_.load(std::memory_order_acquire);

	// Discard any stale frames in the ring buffer from a seek or decoder cancelation
	if(flags & static_cast<unsigned int>(Flags::drainRequired)) {
		audioRingBuffer_.Drain();
		flags_.fetch_and(~static_cast<unsigned int>(Flags::drainRequired), std::memory_order_acq_rel);

		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i)
			std::memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
		isSilence = YES;
		return noErr;
	}

	// Output silence if not playing or muted
	if(!(flags & static_cast<unsigned int>(Flags::isPlaying)) || (flags & static_cast<unsigned int>(Flags::isMuted))) {
		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i)
			std::memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
		isSilence = YES;
		return noErr;
	}

	// Read audio from the ring buffer
	if(const auto framesRead = audioRingBuffer_.Read(outputData, frameCount); framesRead > 0) {
#if DEBUG
		if(framesRead != frameCount)
			os_log_debug(log_, "Insufficient audio in ring buffer: %zu frames available, %u requested", framesRead, frameCount);
#endif /* DEBUG */
		const RenderingEventHeader header{RenderingEventCommand::framesRendered};
		if(!renderEventRingBuffer_.WriteValues(header, timestamp, static_cast<uint32_t>(framesRead)))
			os_log_fault(log_, "Error writing frames rendered event");
	} else
		isSilence = YES;

	return noErr;
}

// MARK: - Event Processing

void SFB::AudioPlayer::SequenceAndProcessEvents(std::stop_token stoken) noexcept
{
	pthread_setname_np("AudioPlayer.Events");
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

	os_log_debug(log_, "Event processing thread starting");

	while(!stoken.stop_requested()) {
		auto decodeEventHeader = decodeEventRingBuffer_.ReadValue<DecodingEventHeader>();
		auto renderEventHeader = renderEventRingBuffer_.ReadValue<RenderingEventHeader>();

		// Process all pending decode and render events in chronological order
		for(;;) {
			// Nothing left to do
			if(!decodeEventHeader && !renderEventHeader)
				break;
			else if(decodeEventHeader && !renderEventHeader) {
				// Process the decode event
				ProcessDecodingEvent(*decodeEventHeader);
				decodeEventHeader = decodeEventRingBuffer_.ReadValue<DecodingEventHeader>();
			} else if(!decodeEventHeader && renderEventHeader) {
				// Process the render event
				ProcessRenderingEvent(*renderEventHeader);
				renderEventHeader = renderEventRingBuffer_.ReadValue<RenderingEventHeader>();
			} else if(decodeEventHeader->mIdentificationNumber < renderEventHeader->mIdentificationNumber) {
				// Process the event with an earlier identification number
				ProcessDecodingEvent(*decodeEventHeader);
				decodeEventHeader = decodeEventRingBuffer_.ReadValue<DecodingEventHeader>();
			} else {
				ProcessRenderingEvent(*renderEventHeader);
				renderEventHeader = renderEventRingBuffer_.ReadValue<RenderingEventHeader>();
			}
		}

		int64_t deltaNanos;
		{
			std::lock_guard lock(decoderLock_);
			if(FirstDecoderStateWithRenderingNotComplete())
				deltaNanos = 7.5 * NSEC_PER_MSEC;
			// Use a longer timeout when idle
			else
				deltaNanos = NSEC_PER_SEC / 2;
		}

		// Decoding events will be signaled; render events are polled using the timeout
		dispatch_semaphore_wait(eventSemaphore_, dispatch_time(DISPATCH_TIME_NOW, deltaNanos));
	}

	os_log_debug(log_, "Event processing thread complete");
}

void SFB::AudioPlayer::ProcessDecodingEvent(const DecodingEventHeader& header) noexcept
{
	switch(header.mCommand) {
		case DecodingEventCommand::started:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_error(log_, "Decoder state with sequence number %llu missing for decoding started event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->decoder_;
				}

				HandleDecodingStarted(decoder);
			} else
				os_log_error(log_, "Missing decoder sequence number for decoding started event");
			break;

		case DecodingEventCommand::complete:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_error(log_, "Decoder state with sequence number %llu missing for decoding complete event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->decoder_;
				}

				HandleDecodingComplete(decoder);
			} else
				os_log_error(log_, "Missing decoder sequence number for decoding complete event");
			break;

		case DecodingEventCommand::canceled:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;
				AVAudioFramePosition framesRendered;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_error(log_, "Decoder state with sequence number %llu missing for decoder canceled event", decoderSequenceNumber);
						break;
					}

					decoder = decoderState->decoder_;
					framesRendered = decoderState->FramesRendered();

					if(!DeleteDecoderStateWithSequenceNumber(decoderSequenceNumber))
						os_log_error(log_, "Unable to delete decoder state with sequence number %llu in decoder canceled event", decoderSequenceNumber);
				}

				HandleDecoderCanceled(decoder, framesRendered);
			} else
				os_log_error(log_, "Missing decoder sequence number for decoder canceled event");
			break;

		case DecodingEventCommand::error:
		{
			uint32_t dataSize;
			if(!decodeEventRingBuffer_.ReadValue(dataSize)) {
				os_log_error(log_, "Missing data size for decoding error event");
				break;
			}

			NSMutableData *data = [NSMutableData dataWithLength:dataSize];
			if(decodeEventRingBuffer_.Read(data.mutableBytes, 1, dataSize, false) != dataSize) {
				os_log_error(log_, "Missing or incomplete archived NSError for decoding error event");
				break;
			}

			NSError *err = nil;
			NSError *error = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSError class] fromData:data error:&err];
			if(!error) {
				os_log_error(log_, "Error unarchiving NSError for decoding error event: %{public}@", err);
				break;
			}

			HandleAsynchronousError(error);
			break;
		}

		default:
			os_log_error(log_, "Unknown decode event command: %u", header.mCommand);
			break;
	}
}

void SFB::AudioPlayer::ProcessRenderingEvent(const RenderingEventHeader& header) noexcept
{
	switch(header.mCommand) {
		case RenderingEventCommand::framesRendered:
			// The timestamp of the render cycle
			AudioTimeStamp timestamp;
			// The number of valid frames rendered
			uint32_t framesRendered;

			if(renderEventRingBuffer_.ReadValues(timestamp, framesRendered)) {
#if DEBUG
				assert(framesRendered > 0);
#endif /* DEBUG */

				// Perform bookkeeping to apportion the rendered frames appropriately
				//
				// framesRendered contains the number of valid frames that were rendered
				// However, these could have come from any number of decoders depending on buffer sizes
				// So it is necessary to split them up here

				DecoderState *decoderState = nullptr;
				AVAudioFramePosition framesRemainingToDistribute = framesRendered;
				while(framesRemainingToDistribute > 0) {
					uint64_t hostTime = 0;
					Decoder startedDecoder = nil;
					Decoder completeDecoder = nil;

					{
						std::lock_guard lock(decoderLock_);
						if(decoderState)
							decoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
						else
							decoderState = FirstDecoderStateWithRenderingNotComplete();
						if(!decoderState)
							break;

						const auto decoderFramesRemaining = decoderState->FramesAvailableToRender();
						const auto framesFromThisDecoder = std::min(decoderFramesRemaining, framesRemainingToDistribute);

						// Rendering is starting
						if(!decoderState->HasRenderingStarted() && framesFromThisDecoder > 0) {
							decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::renderingStarted), std::memory_order_acq_rel);

							const auto frameOffset = framesRendered - framesRemainingToDistribute;
							const double deltaSeconds = frameOffset / audioRingBuffer_.Format().mSampleRate;
							hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

							const auto now = SFB::GetCurrentHostTime();
							if(now > hostTime)
								os_log_error(log_, "Rendering started event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->decoder_);
#if DEBUG
							else
								os_log_debug(log_, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->decoder_);
#endif /* DEBUG */

							startedDecoder = decoderState->decoder_;
						}

						decoderState->AddFramesRendered(framesFromThisDecoder);
						framesRemainingToDistribute -= framesFromThisDecoder;

						// Rendering is complete
						if(decoderState->IsDecodingComplete() && decoderState->AllAvailableFramesRendered()) {
							decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::renderingComplete), std::memory_order_acq_rel);

							completeDecoder = decoderState->decoder_;

							const auto frameOffset = framesRendered - framesRemainingToDistribute;
							const double deltaSeconds = frameOffset / audioRingBuffer_.Format().mSampleRate;
							hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

							const auto now = SFB::GetCurrentHostTime();
							if(now > hostTime)
								os_log_error(log_, "Rendering complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->decoder_);
#if DEBUG
							else
								os_log_debug(log_, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->decoder_);
#endif /* DEBUG */

							if(!DeleteDecoderStateWithSequenceNumber(decoderState->sequenceNumber_))
								os_log_error(log_, "Unable to delete decoder state with sequence number %llu in rendering complete event", decoderState->sequenceNumber_);
						}
					}

					// Call blocks after unlock
					if(startedDecoder)
						HandleRenderingWillStart(startedDecoder, hostTime);
					if(completeDecoder)
						HandleRenderingWillComplete(completeDecoder, hostTime);
				}
			} else
				os_log_error(log_, "Missing timestamp or frames rendered for frames rendered event");
			break;

		default:
			os_log_error(log_, "Unknown render event command: %u", header.mCommand);
			break;
	}
}

// MARK: - Active Decoder Management

SFB::AudioPlayer::DecoderState * const SFB::AudioPlayer::FirstDecoderStateWithDecodingNotComplete() const noexcept
{
#if DEBUG
	decoderLock_.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(activeDecoders_.cbegin(), activeDecoders_.cend(), [](const auto& decoderState) {
		const auto flags = decoderState->flags_.load(std::memory_order_acquire);
		const bool canceled = flags & static_cast<unsigned int>(DecoderState::Flags::isCanceled);
		const bool decodingComplete = flags & static_cast<unsigned int>(DecoderState::Flags::decodingComplete);
		return !canceled && !decodingComplete;
	});
	if(iter == activeDecoders_.cend())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayer::DecoderState * const SFB::AudioPlayer::FirstDecoderStateWithRenderingNotComplete() const noexcept
{
#if DEBUG
	decoderLock_.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(activeDecoders_.cbegin(), activeDecoders_.cend(), [](const auto& decoderState) {
		const auto flags = decoderState->flags_.load(std::memory_order_acquire);
		const bool canceled = flags & static_cast<unsigned int>(DecoderState::Flags::isCanceled);
		const bool renderingComplete = flags & static_cast<unsigned int>(DecoderState::Flags::renderingComplete);
		return !canceled && !renderingComplete;
	});
	if(iter == activeDecoders_.cend())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayer::DecoderState * const SFB::AudioPlayer::FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(const uint64_t sequenceNumber) const noexcept
{
#if DEBUG
	decoderLock_.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(activeDecoders_.cbegin(), activeDecoders_.cend(), [sequenceNumber](const auto& decoderState) {
		if(decoderState->sequenceNumber_ <= sequenceNumber)
			return false;
		const auto flags = decoderState->flags_.load(std::memory_order_acquire);
		const bool canceled = flags & static_cast<unsigned int>(DecoderState::Flags::isCanceled);
		const bool renderingComplete = flags & static_cast<unsigned int>(DecoderState::Flags::renderingComplete);
		return !canceled && !renderingComplete;
	});
	if(iter == activeDecoders_.cend())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayer::DecoderState * const SFB::AudioPlayer::DecoderStateWithSequenceNumber(const uint64_t sequenceNumber) const noexcept
{
#if DEBUG
	decoderLock_.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(activeDecoders_.cbegin(), activeDecoders_.cend(), [sequenceNumber](const auto& decoderState) {
		return decoderState->sequenceNumber_ == sequenceNumber;
	});
	if(iter == activeDecoders_.cend())
		return nullptr;
	return iter->get();
}

bool SFB::AudioPlayer::DeleteDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) noexcept
{
#if DEBUG
	decoderLock_.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(activeDecoders_.cbegin(), activeDecoders_.cend(), [sequenceNumber](const auto& decoderState) {
		return decoderState->sequenceNumber_ == sequenceNumber;
	});
	if(iter == activeDecoders_.cend())
		return false;

	os_log_debug(log_, "Deleting decoder state for %{public}@", (*iter)->decoder_);
	activeDecoders_.erase(iter);

	return true;
}

// MARK: - AVAudioEngine Notification Handling

void SFB::AudioPlayer::HandleAudioEngineConfigurationChange(AVAudioEngine *engine, NSDictionary *userInfo) noexcept
{
	if(engine != engine_) {
		os_log_error(log_, "AVAudioEngineConfigurationChangeNotification received for incorrect AVAudioEngine instance");
		return;
	}

	// AVAudioEngine posts this notification from a dedicated queue
	os_log_debug(log_, "Received AVAudioEngineConfigurationChangeNotification");

	// AVAudioEngine stops itself when interrupted and there is no way to determine if the engine was
	// running before this notification was issued unless the state is cached
	const auto flags = flags_.load(std::memory_order_acquire);
	const auto engineWasRunning = flags & static_cast<unsigned int>(Flags::engineIsRunning);
	const auto wasPlaying = flags & static_cast<unsigned int>(Flags::isPlaying);

	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning) & ~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

	// Update the audio processing graph
	const auto success = [&] {
		std::lock_guard lock{lock_};
		return ConfigureProcessingGraph(renderingFormat_, false);
	}();

	if(!success) {
		os_log_error(log_, "Unable to configure audio processing graph for %{public}@", SFB::StringDescribingAVAudioFormat(renderingFormat_));
		// The graph is not in a working state
		if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
			NSError *error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeFormatNotSupported userInfo:nil];
			[player_.delegate audioPlayer:player_ encounteredError:error];
		}
		return;
	}

	// Restart AVAudioEngine if previously running
	if(engineWasRunning) {
		if(NSError *error = nil; ![engine_ startAndReturnError:&error]) {
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", error);
//			if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
//				[player_.delegate audioPlayer:player_ encounteredError:error];
			return;
		}

		if(wasPlaying)
			flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
		else
			flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
	}

	if(const auto flags = flags_.load(std::memory_order_acquire); (engineWasRunning != (flags & static_cast<unsigned int>(Flags::engineIsRunning)) || wasPlaying != (flags & static_cast<unsigned int>(Flags::isPlaying))) && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:PlaybackState()];

	if([player_.delegate respondsToSelector:@selector(audioPlayerAVAudioEngineConfigurationChange:)])
		[player_.delegate audioPlayerAVAudioEngineConfigurationChange:player_];
}

#if TARGET_OS_IPHONE
void SFB::AudioPlayer::HandleAudioSessionInterruption(NSDictionary *userInfo) noexcept
{
	const auto interruptionType = [[userInfo objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
	switch(interruptionType) {
		case AVAudioSessionInterruptionTypeBegan:
			os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeBegan)");
			Pause();
			break;

		case AVAudioSessionInterruptionTypeEnded:
			os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

			// AVAudioEngine stops itself when AVAudioSessionInterruptionNotification is received
			// However, Flags::engineIsRunning indicates if the engine was running before the interruption
			if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) {
				flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
				if(NSError *error = nil; ![engine_ startAndReturnError:&error]) {
					os_log_error(log_, "Error starting AVAudioEngine: %{public}@", error);
					return;
				}
				flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
			}
			break;

		default:
			os_log_error(log_, "Unknown value %lu for AVAudioSessionInterruptionTypeKey", static_cast<unsigned long>(interruptionType));
			break;
	}
}
#endif /* TARGET_OS_IPHONE */

// MARK: - Processing Graph Management

bool SFB::AudioPlayer::ConfigureProcessingGraphAndRingBufferForDecoder(Decoder decoder, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
	lock_.assert_owner();
#endif /* DEBUG */

	auto format = decoder.processingFormat;
	if(FormatWillBeGaplessIfEnqueued(format))
		return true;

	// Attempt to preserve the playback state
	const auto flags = flags_.load(std::memory_order_acquire);
	const auto engineWasRunning = flags & static_cast<unsigned int>(Flags::engineIsRunning);
	const auto wasPlaying = flags & static_cast<unsigned int>(Flags::isPlaying);

	// If the rendering format and the decoder's format cannot be gaplessly joined
	// reconfigure AVAudioEngine with a new AVAudioSourceNode with the correct format
	if(!ConfigureProcessingGraph(format, true)) {
		if(error)
			*error = [NSError errorWithDomain:SFBAudioPlayerErrorDomain code:SFBAudioPlayerErrorCodeFormatNotSupported userInfo:nil];
		SetNowPlaying(nil);
		return false;
	}

	if(!audioRingBuffer_.Allocate(*(renderingFormat_.streamDescription), 16384)) {
		os_log_error(log_, "Unable to create audio ring buffer: CXXCoreAudio::AudioRingBuffer::Allocate failed");
		return false;
	}

	// Restart AVAudioEngine and playback as appropriate
	if(engineWasRunning && !(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning))) {
		if(NSError *err = nil; ![engine_ startAndReturnError:&err]) {
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return false;
		}

		if(wasPlaying)
			flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning) | static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);
		else
			flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
	}

#if DEBUG
	{
		const auto flags = flags_.load(std::memory_order_acquire);
		assert((flags & static_cast<unsigned int>(Flags::engineIsRunning)) == engineWasRunning && "Incorrect audio engine state in ConfigureForAndEnqueueDecoder()");
		assert((flags & static_cast<unsigned int>(Flags::isPlaying)) == wasPlaying && "Incorrect playback state in ConfigureForAndEnqueueDecoder()");
	}
#endif /* DEBUG */

	return true;
}

bool SFB::AudioPlayer::ConfigureProcessingGraph(AVAudioFormat *format, bool replaceSourceNode) noexcept
{
#if DEBUG
	assert(format != nil);
	assert(replaceSourceNode || [format isEqual:renderingFormat_]);
	lock_.assert_owner();
#endif /* DEBUG */

	if(!format.isStandard) {
		AVAudioFormat *standardEquivalentFormat = [format standardEquivalent];
		if(!standardEquivalentFormat) {
			os_log_error(log_, "Unable to convert format %{public}@ to standard equivalent", SFB::StringDescribingAVAudioFormat(format));
			return false;
		}
		format = standardEquivalentFormat;
	}

	AVAudioSourceNode *sourceNode = nil;
	if(replaceSourceNode) {
		sourceNode = [[AVAudioSourceNode alloc] initWithFormat:format
												   renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
			return Render(*isSilence, *timestamp, frameCount, outputData);
		}];
		if(!sourceNode)
			return false;
	}

	// Even if the engine isn't running, call stop to force release of any render resources
	// Empirically this is necessary when transitioning between formats with different
	// channel counts, although it seems that it shouldn't be
	[engine_ stop];
	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning) & ~static_cast<unsigned int>(Flags::isPlaying), std::memory_order_acq_rel);

	AVAudioOutputNode *outputNode = engine_.outputNode;
	AVAudioMixerNode *mixerNode = engine_.mainMixerNode;

	// This class requires that the main mixer node be connected to the output node
	assert([engine_ inputConnectionPointForNode:outputNode inputBus:0].node == mixerNode && "Illegal AVAudioEngine configuration");

	AVAudioFormat *outputNodeOutputFormat = [outputNode outputFormatForBus:0];
	AVAudioFormat *mixerNodeOutputFormat = [mixerNode outputFormatForBus:0];

	const auto outputFormatsMismatch = outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount || outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate;
	if(outputFormatsMismatch) {
		os_log_debug(log_,
					 "Mismatch between output formats for main mixer and output nodes:\n    mainMixerNode: %{public}@\n       outputNode: %{public}@",
					 SFB::StringDescribingAVAudioFormat(mixerNodeOutputFormat),
					 SFB::StringDescribingAVAudioFormat(outputNodeOutputFormat));

		[engine_ disconnectNodeInput:outputNode bus:0];

		// Reconnect the mixer and output nodes using the output node's output format
		[engine_ connect:mixerNode to:outputNode format:outputNodeOutputFormat];
	}

	if(sourceNode) {
		[engine_ attachNode:sourceNode];

		AVAudioConnectionPoint *sourceNodeOutputConnectionPoint = [[engine_ outputConnectionPointsForNode:sourceNode_ outputBus:0] firstObject];
		[engine_ detachNode:sourceNode_];

		sourceNode_ = sourceNode;
		renderingFormat_ = format;

		// Reconnect the player node to the next node in the processing chain
		// This is the mixer node in the default configuration, but additional nodes may
		// have been inserted between the player and mixer nodes. In this case allow the delegate
		// to make any necessary adjustments based on the format change if desired.
		if(sourceNodeOutputConnectionPoint && sourceNodeOutputConnectionPoint.node != mixerNode) {
			if([player_.delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
				AVAudioNode *node = [player_.delegate audioPlayer:player_ reconfigureProcessingGraph:engine_ withFormat:format];
				// Ensure the delegate returned a valid node
				assert(node != nil && "nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
				[engine_ connect:sourceNode_ to:node format:format];
			} else
				[engine_ connect:sourceNode_ to:sourceNodeOutputConnectionPoint.node format:format];
		} else
			[engine_ connect:sourceNode_ to:mixerNode format:format];
	}

	// AVAudioMixerNode handles sample rate conversion, but it may require input buffer sizes
	// (maximum frames per slice) greater than the default for AVAudioSourceNode (1156).
	//
	// For high sample rates, the sample rate conversion can require more rendered frames than are available by default.
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the mixer don't match, adjust
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	if(format.sampleRate > outputNodeOutputFormat.sampleRate) {
		os_log_debug(log_, "AVAudioMixerNode input sample rate (%g Hz) and output sample rate (%g Hz) don't match", format.sampleRate, outputNodeOutputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		const double ratio = format.sampleRate / outputNodeOutputFormat.sampleRate;
		const auto maximumFramesToRender = static_cast<AUAudioFrameCount>(std::ceil(512 * ratio));

		if(auto audioUnit = sourceNode_.AUAudioUnit; audioUnit.maximumFramesToRender < maximumFramesToRender) {
			const auto renderResourcesAllocated = audioUnit.renderResourcesAllocated;
			if(renderResourcesAllocated)
				[audioUnit deallocateRenderResources];

			os_log_debug(log_, "Adjusting AVAudioSourceNode's maximumFramesToRender to %u", maximumFramesToRender);
			audioUnit.maximumFramesToRender = maximumFramesToRender;

			NSError *error;
			if(renderResourcesAllocated && ![audioUnit allocateRenderResourcesAndReturnError:&error])
				os_log_error(log_, "Error allocating AUAudioUnit render resources for AVAudioSourceNode: %{public}@", error);
		}
	}

#if DEBUG
	LogProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

	[engine_ prepare];

	return true;
}

// MARK: - Event Notifications

void SFB::AudioPlayer::HandleDecodingStarted(Decoder decoder) noexcept
{
	if([player_.delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)])
		[player_.delegate audioPlayer:player_ decodingStarted:decoder];

	if(const auto flags = flags_.load(std::memory_order_acquire); (flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !((flags & static_cast<unsigned int>(Flags::engineIsRunning)) && (flags & static_cast<unsigned int>(Flags::isPlaying))) && CurrentDecoder() == decoder) {
		flags_.fetch_or(static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
		SetNowPlaying(decoder);
	}
	flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
}

void SFB::AudioPlayer::HandleDecodingComplete(Decoder decoder) noexcept
{
	if([player_.delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)])
		[player_.delegate audioPlayer:player_ decodingComplete:decoder];
}

void SFB::AudioPlayer::HandleRenderingWillStart(Decoder decoder, uint64_t hostTime) noexcept
{
	// Schedule the rendering started notification at the expected host time
	dispatch_after(hostTime, eventQueue_, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering will start notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / renderingFormat_.sampleRate);
		if(delta > tolerance)
			os_log_debug(log_, "Rendering started notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::pendingDecoderBecameActive)))
			SetNowPlaying(decoder);
		flags_.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[player_.delegate audioPlayer:player_ renderingStarted:decoder];
	});

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillStart:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleRenderingWillComplete(Decoder _Nonnull decoder, uint64_t hostTime) noexcept
{
	// Schedule the rendering completed notification at the expected host time
	dispatch_after(hostTime, eventQueue_, ^{
		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering will complete notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / renderingFormat_.sampleRate);
		if(delta > tolerance)
			os_log_debug(log_, "Rendering complete notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[player_.delegate audioPlayer:player_ renderingComplete:decoder];

		// End of audio
		if(const auto flags = flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !(flags & static_cast<unsigned int>(Flags::formatMismatch)) ) {
#if DEBUG
			os_log_debug(log_, "End of audio reached");
#endif /* DEBUG */

			SetNowPlaying(nil);

			if([player_.delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)])
				[player_.delegate audioPlayerEndOfAudio:player_];
			else
				Stop();
		}
	});

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillComplete:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleDecoderCanceled(Decoder decoder, AVAudioFramePosition framesRendered) noexcept
{
	// Mark the decoder as canceled for any scheduled render notifications
	objc_setAssociatedObject(decoder, &_decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

	if([player_.delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)])
		[player_.delegate audioPlayer:player_ decoderCanceled:decoder framesRendered:framesRendered];

	flags_.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
	if(const auto flags = flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !(flags & static_cast<unsigned int>(Flags::engineIsRunning)))
		SetNowPlaying(nil);
}

void SFB::AudioPlayer::HandleAsynchronousError(NSError *error) noexcept
{
	if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
		[player_.delegate audioPlayer:player_ encounteredError:error];
}
