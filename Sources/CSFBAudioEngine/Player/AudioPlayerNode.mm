//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <cassert>
#import <cmath>
#import <cstring>
#import <exception>
#import <functional>
#import <stdexcept>

#import <AVFAudio/AVFAudio.h>

#import <CXXCoreAudio/CAChannelLayout.hpp>

#import "AudioPlayerNode.h"

#import "HostTimeUtilities.hpp"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "StringDescribingAVAudioFormat.h"

namespace {

/// The minimum number of frames to write to the ring buffer
constexpr AVAudioFrameCount kRingBufferChunkSize = 2048;

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

const os_log_t AudioPlayerNode::log_ = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

// MARK: - Decoder State

/// State for tracking/syncing decoding progress
struct AudioPlayerNode::DecoderState final {
	/// Monotonically increasing instance counter
	const uint64_t			sequenceNumber_ 	{sequenceCounter_++};

	/// The sample rate of the audio converter's output format
	const double 			sampleRate_ 		{0};

	/// Decodes audio from the source representation to PCM
	const Decoder 			decoder_ 			{nil};

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

	DecoderState(Decoder _Nonnull decoder, AVAudioFormat * _Nonnull format, AVAudioFrameCount frameCapacity = 1024)
	: frameLength_{decoder.frameLength}, decoder_{decoder}, sampleRate_{format.sampleRate}
	{
#if DEBUG
		assert(decoder != nil);
		assert(format != nil);
#endif /* DEBUG */

		converter_ = [[AVAudioConverter alloc] initFromFormat:decoder_.processingFormat toFormat:format];
		if(!converter_) {
			os_log_error(log_, "Error creating AVAudioConverter converting from %{public}@ to %{public}@", decoder_.processingFormat, format);
			throw std::runtime_error("Error creating AVAudioConverter");
		}

		// The logic in this class assumes no SRC is performed by mConverter
		assert(converter_.inputFormat.sampleRate == converter_.outputFormat.sampleRate);

		decodeBuffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter_.inputFormat frameCapacity:frameCapacity];
		if(!decodeBuffer_)
			throw std::bad_alloc();

		if(const auto framePosition = decoder.framePosition; framePosition != 0) {
			framesDecoded_.store(framePosition, std::memory_order_release);
			framesConverted_.store(framePosition, std::memory_order_release);
			framesRendered_.store(framePosition, std::memory_order_release);
		}
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

uint64_t AudioPlayerNode::DecoderState::sequenceCounter_ = 1;

} /* namespace SFB */

// MARK: - AudioPlayerNode

SFB::AudioPlayerNode::AudioPlayerNode(AVAudioFormat *format, uint32_t ringBufferSize)
: renderingFormat_{format}
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	os_log_debug(log_, "Created <AudioPlayerNode: %p>, rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(renderingFormat_));

	// ========================================
	// Rendering Setup

	// Allocate the audio ring buffer moving audio from the decoder queue to the render block
	if(!audioRingBuffer_.Allocate(*(renderingFormat_.streamDescription), ringBufferSize)) {
		os_log_error(log_, "Unable to create audio ring buffer: CXXCoreAudio::AudioRingBuffer::Allocate failed");
		throw std::runtime_error("CXXCoreAudio::AudioRingBuffer::Allocate failed");
	}

	// Set up the render block
	renderBlock_ = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
		return Render(*isSilence, *timestamp, frameCount, outputData);
	};

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

	// Launch the decoding and event processing threads
	try {
		decodingThread_ = std::jthread(std::bind_front(&SFB::AudioPlayerNode::ProcessDecoders, this));
		eventThread_ = std::jthread(std::bind_front(&SFB::AudioPlayerNode::SequenceAndProcessEvents, this));
	} catch(const std::exception& e) {
		os_log_error(log_, "Unable to create thread: %{public}s", e.what());
		throw;
	}
}

SFB::AudioPlayerNode::~AudioPlayerNode() noexcept
{
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

	os_log_debug(log_, "<AudioPlayerNode: %p> destroyed", this);
}

// MARK: - Queue Management

bool SFB::AudioPlayerNode::EnqueueDecoder(Decoder decoder, bool reset, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return false;

	if(!SupportsFormat(decoder.processingFormat)) {
		os_log_error(log_, "Unsupported decoder processing format: %{public}@", SFB::StringDescribingAVAudioFormat(decoder.processingFormat));

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioPlayerNodeErrorDomain
											 code:SFBAudioPlayerNodeErrorCodeFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
											  url:decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's format is not supported by this player.", @"")];

		return false;
	}

	if(reset) {
		// Mute until the decoder becomes active to prevent spurious events
		flags_.fetch_or(static_cast<unsigned int>(Flags::muteRequested), std::memory_order_acq_rel);
		Reset();
		flags_.fetch_or(static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);
	}

	try {
		std::lock_guard lock(queueLock_);
		queuedDecoders_.push_back(decoder);
	} catch(const std::exception& e) {
		os_log_error(log_, "Error pushing %{public}@ to queuedDecoders_: %{public}s", decoder, e.what());
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		if(reset)
			flags_.fetch_and(~static_cast<unsigned int>(Flags::muteRequested) & ~static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);
		return false;
	}

	os_log_info(log_, "Enqueued %{public}@", decoder);

	dispatch_semaphore_signal(decodingSemaphore_);

	return true;
}

SFB::AudioPlayerNode::Decoder SFB::AudioPlayerNode::DequeueDecoder() noexcept
{
	std::lock_guard lock(queueLock_);
	Decoder decoder = nil;
	if(!queuedDecoders_.empty()) {
		decoder = queuedDecoders_.front();
		queuedDecoders_.pop_front();
	}
	return decoder;
}

bool SFB::AudioPlayerNode::RemoveDecoderFromQueue(Decoder decoder) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	std::lock_guard lock(queueLock_);
	const auto iter = std::find(queuedDecoders_.cbegin(), queuedDecoders_.cend(), decoder);
	if(iter == queuedDecoders_.cend())
		return false;
	queuedDecoders_.erase(iter);
	return true;
}

SFB::AudioPlayerNode::Decoder SFB::AudioPlayerNode::CurrentDecoder() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return nil;
	return decoderState->decoder_;
}

void SFB::AudioPlayerNode::CancelActiveDecoders(bool cancelAllActive) noexcept
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

// MARK: - Playback Properties

SFBPlaybackPosition SFB::AudioPlayerNode::PlaybackPosition() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return SFBInvalidPlaybackPosition;
	return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
}

SFBPlaybackTime SFB::AudioPlayerNode::PlaybackTime() const noexcept
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

bool SFB::AudioPlayerNode::GetPlaybackPositionAndTime(SFBPlaybackPosition *playbackPosition, SFBPlaybackTime *playbackTime) const noexcept
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

bool SFB::AudioPlayerNode::SeekForward(NSTimeInterval secondsToSkip) noexcept
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

bool SFB::AudioPlayerNode::SeekBackward(NSTimeInterval secondsToSkip) noexcept
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

bool SFB::AudioPlayerNode::SeekToTime(NSTimeInterval timeInSeconds) noexcept
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

bool SFB::AudioPlayerNode::SeekToPosition(double position) noexcept
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

bool SFB::AudioPlayerNode::SeekToFrame(AVAudioFramePosition frame) noexcept
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

bool SFB::AudioPlayerNode::SupportsSeeking() const noexcept
{
	std::lock_guard lock(decoderLock_);
	const auto decoderState = FirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return false;
	return decoderState->decoder_.supportsSeeking;
}

// MARK: - Format Information

bool SFB::AudioPlayerNode::SupportsFormat(AVAudioFormat *format) const noexcept
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
	return format.channelCount == renderingFormat_.channelCount && format.sampleRate == renderingFormat_.sampleRate && CXXCoreAudio::AVAudioChannelLayoutsAreEquivalent(format.channelLayout, renderingFormat_.channelLayout);
}

// MARK: - Decoding

void SFB::AudioPlayerNode::ProcessDecoders(std::stop_token stoken) noexcept
{
	pthread_setname_np("AudioPlayerNode.Decoding");
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

	os_log_debug(log_, "Decoding thread starting");

	// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
	AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderingFormat_ frameCapacity:kRingBufferChunkSize];
	if(!buffer) {
		os_log_error(log_, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(renderingFormat_), kRingBufferChunkSize);
		NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeInternalError userInfo:nil];
		SubmitDecodingErrorEvent(error);
		return;
	}

	const auto ringBufferChunkDuration = kRingBufferChunkSize / audioRingBuffer_.Format().mSampleRate;

	for(;;) {
		// The decoder state being processed
		DecoderState *decoderState = nullptr;

		// Get the earliest decoder state that has not completed rendering
		{
			std::lock_guard lock(decoderLock_);
			decoderState = FirstDecoderStateWithRenderingNotComplete();

			// Process cancelations
			while(decoderState && (decoderState->flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(DecoderState::Flags::cancelRequested))) {
				os_log_debug(log_, "Canceling decoding for %{public}@", decoderState->decoder_);

				flags_.fetch_or(static_cast<unsigned int>(Flags::ringBufferNeedsReset), std::memory_order_acq_rel);
				decoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::isCanceled), std::memory_order_acq_rel);

				// Submit the decoder canceled event
				const DecodingEventHeader header{DecodingEventCommand::canceled};
				if(decodeEventRingBuffer_.WriteValues(header, decoderState->sequenceNumber_))
					dispatch_semaphore_signal(eventSemaphore_);
				else
					os_log_fault(log_, "Error writing decoder canceled event");

				decoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_);
			}
		}

		// Terminate the thread if requested after processing cancelations
		if(stoken.stop_requested())
		   break;

		// Process pending seeks
		if(decoderState && decoderState->IsSeekPending()) {
			// If a seek is pending request a ring buffer reset
			flags_.fetch_or(static_cast<unsigned int>(Flags::ringBufferNeedsReset), std::memory_order_acq_rel);

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

		// Get the earliest decoder state that has not completed decoding
		{
			std::lock_guard lock(decoderLock_);
			decoderState = FirstDecoderStateWithDecodingNotComplete();
		}

		// Dequeue the next decoder if there are no decoders that haven't completed decoding
		if(!decoderState) {
			if(auto decoder = DequeueDecoder(); decoder) {
				try {
					// Create the decoder state and add it to the list of active decoders
					// When the decoder's processing format and rendering format don't match
					// conversion will be performed in DecoderState::DecodeAudio()
					std::lock_guard lock(decoderLock_);
					activeDecoders_.push_back(std::make_unique<DecoderState>(decoder, renderingFormat_, kRingBufferChunkSize));
					decoderState = activeDecoders_.back().get();
				} catch(const std::exception& e) {
					os_log_error(log_, "Error creating decoder state for %{public}@: %{public}s", decoder, e.what());
					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeInternalError userInfo:nil];
					SubmitDecodingErrorEvent(error);
					continue;
				}

				// Clear the mute flags if needed
				if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::unmuteAfterDequeue))
					flags_.fetch_and(~static_cast<unsigned int>(Flags::isMuted) & ~static_cast<unsigned int>(Flags::muteRequested) & ~static_cast<unsigned int>(Flags::unmuteAfterDequeue), std::memory_order_acq_rel);

				os_log_debug(log_, "Dequeued %{public}@, processing format %{public}@", decoderState->decoder_, SFB::StringDescribingAVAudioFormat(decoderState->decoder_.processingFormat));
			}
		}

		// Reset the ring buffer if required, to prevent audible artifacts
		if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::ringBufferNeedsReset)) {
			flags_.fetch_and(~static_cast<unsigned int>(Flags::ringBufferNeedsReset), std::memory_order_acq_rel);

			// Ensure rendering is muted before performing operations on the ring buffer that aren't thread-safe
			if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::isMuted))) {
				if(node_.engine.isRunning) {
					flags_.fetch_or(static_cast<unsigned int>(Flags::muteRequested), std::memory_order_acq_rel);

					// The render block will clear Flags::muteRequested and set Flags::isMuted
					while(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::isMuted))) {
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
						// The engine may have stopped since the initial check with no subsequent opportunity for the render block to set Flags::isMuted
						if(!node_.engine.isRunning) {
							flags_.fetch_or(static_cast<unsigned int>(Flags::isMuted), std::memory_order_acq_rel);
							flags_.fetch_and(~static_cast<unsigned int>(Flags::muteRequested), std::memory_order_acq_rel);
							break;
						}
					}
				} else
					flags_.fetch_or(static_cast<unsigned int>(Flags::isMuted), std::memory_order_acq_rel);
			}

			// Reset() is not thread-safe but the render block is outputting silence
			audioRingBuffer_.Reset();

			// Clear the mute flag
			flags_.fetch_and(~static_cast<unsigned int>(Flags::isMuted), std::memory_order_acq_rel);
		}

		if(decoderState) {
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
							os_log_fault(log_, "Error writing decoding started event");
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
							os_log_fault(log_, "Error writing decoding complete event");
					}

					if(!resumed)
						os_log_debug(log_, "Decoding complete for %{public}@", decoderState->decoder_);
					else
						os_log_debug(log_, "Decoding complete after resuming for %{public}@", decoderState->decoder_);

					break;
				}
			}
		}

		// Wait for an event signal; timeout after the approximate time for space in the ring buffer to become available
		dispatch_semaphore_wait(decodingSemaphore_, dispatch_time(DISPATCH_TIME_NOW, ringBufferChunkDuration * NSEC_PER_SEC));
	}

	os_log_debug(log_, "Decoding thread complete");
}

void SFB::AudioPlayerNode::SubmitDecodingErrorEvent(NSError *error) noexcept
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

	// Event header and payload
	const DecodingEventHeader header{DecodingEventCommand::error};
	const uint32_t dataSize = errorData.length;
	const void *data = errorData.bytes;

	std::size_t bytesWritten = 0;
	auto [front, back] = decodeEventRingBuffer_.GetWriteVector();

	const auto frontSize = front.size();
	const auto spaceNeeded = sizeof(DecodingEventHeader) + sizeof(uint32_t) + errorData.length;
	if(frontSize + back.size() < spaceNeeded) {
		os_log_fault(log_, "Insufficient space to write decoding error event");
		return;
	}

	std::size_t cursor = 0;
	auto write_single_arg = [&](const void *arg, std::size_t len) noexcept {
		const auto *src = static_cast<const uint8_t *>(arg);
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

	write_single_arg(&header, sizeof header);
	write_single_arg(&dataSize, sizeof dataSize);
	write_single_arg(data, dataSize);

	decodeEventRingBuffer_.CommitWrite(bytesWritten);
	dispatch_semaphore_signal(eventSemaphore_);
}

// MARK: - Rendering

OSStatus SFB::AudioPlayerNode::Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) noexcept
{
	// N.B. The ring buffer must not be read from or written to when Flags::isMuted is set
	// because the decoding thread could be performing non-thread safe operations

	// Output silence if not playing or muted
	if(const auto flags = flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::isPlaying)) || (flags & static_cast<unsigned int>(Flags::isMuted)) || (flags & static_cast<unsigned int>(Flags::muteRequested))) {
		// Mute if requested
		if(flags & static_cast<unsigned int>(Flags::muteRequested)) {
			flags_.fetch_or(static_cast<unsigned int>(Flags::isMuted), std::memory_order_acq_rel);
			flags_.fetch_and(~static_cast<unsigned int>(Flags::muteRequested), std::memory_order_acq_rel);
		}
		// Zero the output
		const auto byteCount = frameCount * audioRingBuffer_.Format().mBytesPerFrame;
		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
			std::memset(outputData->mBuffers[i].mData, 0, byteCount);
			outputData->mBuffers[i].mDataByteSize = byteCount;
		}
		isSilence = YES;
		return noErr;
	}

	// If there are audio frames available to read from the ring buffer read as many as possible
	if(const auto availableFrames = audioRingBuffer_.AvailableFrames(); availableFrames > 0) {
		const auto framesToRead = std::min(availableFrames, static_cast<CXXCoreAudio::AudioRingBuffer::size_type>(frameCount));
		const auto framesRead = audioRingBuffer_.Read(outputData, framesToRead);
		if(framesRead != framesToRead)
			os_log_fault(log_, "CXXCoreAudio::AudioRingBuffer::Read failed: Requested %zu frames, got %zu", framesToRead, framesRead);

		// If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
#if DEBUG
			os_log_debug(log_, "Insufficient audio in ring buffer: %zu frames available, %u requested", framesRead, frameCount);
#endif /* DEBUG */

			const auto framesOfSilence = frameCount - framesRead;
			const auto byteCountToSkip = framesRead * audioRingBuffer_.Format().mBytesPerFrame;
			const auto byteCountToZero = framesOfSilence * audioRingBuffer_.Format().mBytesPerFrame;
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(static_cast<uint8_t *>(outputData->mBuffers[i].mData) + byteCountToSkip, 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize += byteCountToZero;
			}
		}

		const RenderingEventHeader header{RenderingEventCommand::framesRendered};
		if(!renderEventRingBuffer_.WriteValues(header, timestamp, static_cast<uint32_t>(framesRead)))
			os_log_fault(log_, "Error writing frames rendered event");
	} else {
		// Output silence if the ring buffer is empty
		const auto byteCount = frameCount * audioRingBuffer_.Format().mBytesPerFrame;
		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
			std::memset(outputData->mBuffers[i].mData, 0, byteCount);
			outputData->mBuffers[i].mDataByteSize = byteCount;
		}
		isSilence = YES;
	}

	return noErr;
}

// MARK: - Event Processing

void SFB::AudioPlayerNode::SequenceAndProcessEvents(std::stop_token stoken) noexcept
{
	pthread_setname_np("AudioPlayerNode.Events");
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

		// Decoding events will be signaled; render events are polled using the timeout
		dispatch_semaphore_wait(eventSemaphore_, dispatch_time(DISPATCH_TIME_NOW, 7.5 * NSEC_PER_MSEC));
	}

	os_log_debug(log_, "Event processing thread complete");
}

void SFB::AudioPlayerNode::ProcessDecodingEvent(const DecodingEventHeader& header) noexcept
{
	switch(header.mCommand) {
		case DecodingEventCommand::started:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(log_, "Decoder state with sequence number %llu missing for decoding started event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->decoder_;
				}

				if(decodingStartedBlock_)
					decodingStartedBlock_(decoder);
			} else
				os_log_fault(log_, "Missing decoder sequence number for decoding started event");
			break;

		case DecodingEventCommand::complete:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(log_, "Decoder state with sequence number %llu missing for decoding complete event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->decoder_;
				}

				if(decodingCompleteBlock_)
					decodingCompleteBlock_(decoder);
			} else
				os_log_fault(log_, "Missing decoder sequence number for decoding complete event");
			break;

		case DecodingEventCommand::canceled:
			if(uint64_t decoderSequenceNumber; decodeEventRingBuffer_.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;
				AVAudioFramePosition framesRendered;

				{
					std::lock_guard lock(decoderLock_);
					const auto decoderState = DecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(log_, "Decoder state with sequence number %llu missing for decoder canceled event", decoderSequenceNumber);
						break;
					}

					decoder = decoderState->decoder_;
					framesRendered = decoderState->FramesRendered();

					if(!DeleteDecoderStateWithSequenceNumber(decoderSequenceNumber))
						os_log_fault(log_, "Unable to delete decoder state with sequence number %llu in decoder canceled event", decoderSequenceNumber);
				}

				if(decoderCanceledBlock_)
					decoderCanceledBlock_(decoder, framesRendered);
			} else
				os_log_fault(log_, "Missing decoder sequence number for decoder canceled event");
			break;

		case DecodingEventCommand::error:
		{
			uint32_t dataSize;
			if(!decodeEventRingBuffer_.ReadValue(dataSize)) {
				os_log_fault(log_, "Missing data size for decoding error event");
				break;
			}

			NSMutableData *data = [NSMutableData dataWithLength:dataSize];
			if(decodeEventRingBuffer_.Read(data.mutableBytes, 1, dataSize, false) != dataSize) {
				os_log_fault(log_, "Missing or incomplete archived NSError for decoding error event");
				break;
			}

			NSError *err = nil;
			NSError *error = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSError class] fromData:data error:&err];
			if(!error) {
				os_log_error(log_, "Error unarchiving NSError for decoding error event: %{public}@", err);
				break;
			}

			if(asynchronousErrorBlock_)
				asynchronousErrorBlock_(error);
			break;
		}

		default:
			os_log_fault(log_, "Unknown decode event command: %u", header.mCommand);
			break;
	}
}

void SFB::AudioPlayerNode::ProcessRenderingEvent(const RenderingEventHeader& header) noexcept
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
					Decoder nextDecoder = nil;
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

							// Check for a decoder transition
							if(const auto nextDecoderState = FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->sequenceNumber_); nextDecoderState) {
								const auto nextDecoderFramesRemaining = nextDecoderState->FramesAvailableToRender();
								const auto framesFromNextDecoder = std::min(nextDecoderFramesRemaining, framesRemainingToDistribute);

#if DEBUG
								assert(!nextDecoderState->HasRenderingStarted());
#endif /* DEBUG */

								nextDecoderState->flags_.fetch_or(static_cast<unsigned int>(DecoderState::Flags::renderingStarted), std::memory_order_acq_rel);

								nextDecoderState->AddFramesRendered(framesFromNextDecoder);
								framesRemainingToDistribute -= framesFromNextDecoder;

								const auto frameOffset = framesRendered - framesRemainingToDistribute;
								const double deltaSeconds = frameOffset / audioRingBuffer_.Format().mSampleRate;
								hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

								const auto now = SFB::GetCurrentHostTime();
								if(now > hostTime)
									os_log_error(log_, "Rendering decoder changed event processed %.2f msec late for transition from %{public}@ to %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->decoder_, nextDecoderState->decoder_);
#if DEBUG
								else
									os_log_debug(log_, "Rendering decoder will change in %.2f msec from %{public}@ to %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->decoder_, nextDecoderState->decoder_);
#endif /* DEBUG */

								nextDecoder = nextDecoderState->decoder_;

								if(!DeleteDecoderStateWithSequenceNumber(decoderState->sequenceNumber_))
									os_log_fault(log_, "Unable to delete decoder state with sequence number %llu in rendering decoder changed event", decoderState->sequenceNumber_);

								decoderState = nextDecoderState;
							} else {
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
									os_log_fault(log_, "Unable to delete decoder state with sequence number %llu in rendering complete event", decoderState->sequenceNumber_);
							}
						}
					}

					// Call blocks after unlock
					if(startedDecoder && renderingWillStartBlock_)
						renderingWillStartBlock_(startedDecoder, hostTime);

					if(nextDecoder && renderingDecoderWillChangeBlock_)
						renderingDecoderWillChangeBlock_(completeDecoder, nextDecoder, hostTime);
					else if(completeDecoder && renderingWillCompleteBlock_)
						renderingWillCompleteBlock_(completeDecoder, hostTime);
				}
			} else
				os_log_fault(log_, "Missing timestamp or frames rendered for frames rendered event");
			break;

		default:
			os_log_fault(log_, "Unknown render event command: %u", header.mCommand);
			break;
	}
}

// MARK: - Active Decoder Management

SFB::AudioPlayerNode::DecoderState * const SFB::AudioPlayerNode::FirstDecoderStateWithDecodingNotComplete() const noexcept
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

SFB::AudioPlayerNode::DecoderState * const SFB::AudioPlayerNode::FirstDecoderStateWithRenderingNotComplete() const noexcept
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

SFB::AudioPlayerNode::DecoderState * const SFB::AudioPlayerNode::FirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(const uint64_t sequenceNumber) const noexcept
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

SFB::AudioPlayerNode::DecoderState * const SFB::AudioPlayerNode::DecoderStateWithSequenceNumber(const uint64_t sequenceNumber) const noexcept
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

bool SFB::AudioPlayerNode::DeleteDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) noexcept
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
