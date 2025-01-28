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
#import <system_error>

#import <AVFAudio/AVFAudio.h>

#import "AudioPlayerNode.h"

#import "AVAudioChannelLayoutsAreEquivalent.h"
#import "HostTimeUtilities.hpp"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "StringDescribingAVAudioFormat.h"

namespace {

/// The minimum number of frames to write to the ring buffer
constexpr AVAudioFrameCount kRingBufferChunkSize = 2048;

/// libdispatch destructor function for `NSError` objects
void release_nserror_f(void * _Nullable context)
{
	(void)(__bridge_transfer NSError *)context;
}

} /* namespace */

namespace SFB {

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept
{
	static std::atomic_uint64_t nextIdentificationNumber = 1;
	static_assert(std::atomic_uint64_t::is_always_lock_free, "Lock-free std::atomic_uint64_t required");
	return nextIdentificationNumber.fetch_add(1);
}

const os_log_t AudioPlayerNode::sLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

#pragma mark - Decoder State

/// State for tracking/syncing decoding progress
struct AudioPlayerNode::DecoderState final {
	/// Monotonically increasing instance counter
	const uint64_t				mSequenceNumber 	= sSequenceNumber++;

	/// The sample rate of the audio converter's output format
	const double 				mSampleRate 		= 0;

	/// Decodes audio from the source representation to PCM
	const Decoder 				mDecoder 			= nil;

	/// Possible `DecoderState` flag values
	enum DecoderStateFlags : unsigned int {
		/// Decoding started
		eFlagDecodingStarted 	= 1u << 0,
		/// Decoding complete
		eFlagDecodingComplete 	= 1u << 1,
		/// Decoding was resumed after completion
		eFlagDecodingResumed 	= 1u << 2,
		/// Decoding was suspended after starting
		eFlagDecodingSuspended 	= 1u << 3,
		/// Rendering started
		eFlagRenderingStarted 	= 1u << 4,
		/// Rendering complete
		eFlagRenderingComplete 	= 1u << 5,
		/// A seek has been requested
		eFlagSeekPending 		= 1u << 6,
		/// Decoder cancelation requested
		eFlagCancelRequested	= 1u << 7,
		/// Decoder canceled
		eFlagIsCanceled 		= 1u << 8,
	};

	static constexpr AVAudioFrameCount 	kDefaultFrameCapacity 	= 1024;

	/// Flags
	std::atomic_uint 		mFlags 				= 0;
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

	/// The number of frames decoded
	std::atomic_int64_t 	mFramesDecoded 		= 0;
	/// The number of frames converted
	std::atomic_int64_t 	mFramesConverted 	= 0;
	/// The number of frames rendered
	std::atomic_int64_t 	mFramesRendered 	= 0;
	/// The total number of audio frames
	std::atomic_int64_t 	mFrameLength 		= 0;
	/// The desired seek offset
	std::atomic_int64_t 	mFrameToSeek 		= SFBUnknownFramePosition;

	static_assert(std::atomic_int64_t::is_always_lock_free, "Lock-free std::atomic_int64_t required");

	/// Converts audio from the decoder's processing format to another PCM variant at the same sample rate
	AVAudioConverter 		*mConverter 		= nil;
	/// Buffer used internally for buffering during conversion
	AVAudioPCMBuffer 		*mDecodeBuffer 		= nil;

	/// Next sequence number to use
	static uint64_t			sSequenceNumber;

	DecoderState(Decoder _Nonnull decoder, AVAudioFormat * _Nonnull format, AVAudioFrameCount frameCapacity = kDefaultFrameCapacity)
	: mFrameLength{decoder.frameLength}, mDecoder{decoder}, mSampleRate{format.sampleRate}
	{
#if DEBUG
		assert(decoder != nil);
		assert(format != nil);
#endif /* DEBUG */

		mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
		if(!mConverter) {
			os_log_error(sLog, "Error creating AVAudioConverter converting from %{public}@ to %{public}@", mDecoder.processingFormat, format);
			throw std::runtime_error("Error creating AVAudioConverter");
		}

		// The logic in this class assumes no SRC is performed by mConverter
		assert(mConverter.inputFormat.sampleRate == mConverter.outputFormat.sampleRate);

		mDecodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.inputFormat frameCapacity:frameCapacity];
		if(!mDecodeBuffer)
			throw std::system_error(std::error_code(ENOMEM, std::generic_category()));

		if(const auto framePosition = decoder.framePosition; framePosition != 0) {
			mFramesDecoded.store(framePosition);
			mFramesConverted.store(framePosition);
			mFramesRendered.store(framePosition);
		}
	}

	AVAudioFramePosition FramePosition() const noexcept
	{
		return IsSeekPending() ? mFrameToSeek.load() : mFramesRendered.load();
	}

	AVAudioFramePosition FrameLength() const noexcept
	{
		return mFrameLength.load();
	}

	bool DecodeAudio(AVAudioPCMBuffer * _Nonnull buffer, NSError **error = nullptr) noexcept
	{
#if DEBUG
		assert(buffer != nil);
		assert(buffer.frameCapacity == mDecodeBuffer.frameCapacity);
#endif /* DEBUG */

		if(![mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:mDecodeBuffer.frameCapacity error:error])
			return false;

		if(mDecodeBuffer.frameLength == 0) {
			mFlags.fetch_or(eFlagDecodingComplete, std::memory_order_acq_rel);

#if false
			// Some formats may not know the exact number of frames in advance
			// without processing the entire file, which is a potentially slow operation
			mFrameLength.store(mDecoder.framePosition);
#endif /* false */

			buffer.frameLength = 0;
			return true;
		}

		this->mFramesDecoded.fetch_add(mDecodeBuffer.frameLength);

		// Only PCM to PCM conversions are performed
		if(![mConverter convertToBuffer:buffer fromBuffer:mDecodeBuffer error:error])
			return false;
		mFramesConverted.fetch_add(buffer.frameLength);

		// If `buffer` is not full but -decodeIntoBuffer:frameLength:error: returned `YES`
		// decoding is complete
		if(buffer.frameLength != buffer.frameCapacity)
			mFlags.fetch_or(eFlagDecodingComplete, std::memory_order_acq_rel);

		return true;
	}

	/// Returns `true` if `eFlagDecodingComplete` is set
	bool IsDecodingComplete() const noexcept
	{
		return mFlags.load(std::memory_order_acquire) & eFlagDecodingComplete;
	}

	/// Returns `true` if `eFlagRenderingStarted` is set
	bool HasRenderingStarted() const noexcept
	{
		return mFlags.load(std::memory_order_acquire) & eFlagRenderingStarted;
	}

	/// Returns the number of frames available to render.
	///
	/// This is the difference between the number of frames converted and the number of frames rendered
	AVAudioFramePosition FramesAvailableToRender() const noexcept
	{
		return mFramesConverted.load() - mFramesRendered.load();
	}

	/// Returns `true` if there are no frames available to render.
	bool AllAvailableFramesRendered() const noexcept
	{
		return FramesAvailableToRender() == 0;
	}

	/// Returns the number of frames rendered.
	AVAudioFramePosition FramesRendered() const noexcept
	{
		return mFramesRendered.load();
	}

	/// Adds `count` number of frames to the total count of frames rendered.
	void AddFramesRendered(AVAudioFramePosition count) noexcept
	{
		mFramesRendered.fetch_add(count);
	}

	/// Returns `true` if `eFlagSeekPending` is set
	bool IsSeekPending() const noexcept
	{
		return mFlags.load(std::memory_order_acquire) & eFlagSeekPending;
	}

	/// Sets the pending seek request to `frame`
	void RequestSeekToFrame(AVAudioFramePosition frame) noexcept
	{
		mFrameToSeek.store(frame);
		mFlags.fetch_or(eFlagSeekPending, std::memory_order_acq_rel);
	}

	/// Performs the pending seek request, if present
	bool PerformSeekIfRequired() noexcept
	{
		if(!IsSeekPending())
			return true;

		auto seekOffset = mFrameToSeek.load();
		os_log_debug(sLog, "Seeking to frame %lld in %{public}@ ", seekOffset, mDecoder);

		if([mDecoder seekToFrame:seekOffset error:nil])
			// Reset the converter to flush any buffers
			[mConverter reset];
		else
			os_log_debug(sLog, "Error seeking to frame %lld", seekOffset);

		const auto newFrame = mDecoder.framePosition;
		if(newFrame != seekOffset) {
			os_log_debug(sLog, "Inaccurate seek to frame %lld, got %lld", seekOffset, newFrame);
			seekOffset = newFrame;
		}

		// Clear the seek request
		mFlags.fetch_and(~eFlagSeekPending, std::memory_order_acq_rel);

		// Update the frame counters accordingly
		// A seek is handled in essentially the same way as initial playback
		if(newFrame != SFBUnknownFramePosition) {
			mFramesDecoded.store(newFrame);
			mFramesConverted.store(seekOffset);
			mFramesRendered.store(seekOffset);
		}

		return newFrame != SFBUnknownFramePosition;
	}
};

uint64_t AudioPlayerNode::DecoderState::sSequenceNumber = 1;

} /* namespace SFB */

#pragma mark - AudioPlayerNode

SFB::AudioPlayerNode::AudioPlayerNode(AVAudioFormat *format, uint32_t ringBufferSize)
: mRenderingFormat{format}
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	os_log_debug(sLog, "Created <AudioPlayerNode: %p>, rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(mRenderingFormat));

	// ========================================
	// Rendering Setup

	// Allocate the audio ring buffer moving audio from the decoder queue to the render block
	if(!mAudioRingBuffer.Allocate(*(mRenderingFormat.streamDescription), ringBufferSize)) {
		os_log_error(sLog, "Unable to create audio ring buffer: SFB::AudioRingBuffer::Allocate failed");
		throw std::runtime_error("SFB::AudioRingBuffer::Allocate failed");
	}

	// Set up the render block
	mRenderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
		return Render(*isSilence, *timestamp, frameCount, outputData);
	};

	// ========================================
	// Event Processing Setup

	// The decode event ring buffer is written to by the decoding thread and read from by the event queue
	if(!mDecodeEventRingBuffer.Allocate(1024)) {
		os_log_error(sLog, "Unable to create decode event ring buffer: SFB::RingBuffer::Allocate failed");
		throw std::runtime_error("SFB::RingBuffer::Allocate failed");
	}

	// The render event ring buffer is written to by the render block and read from by the event queue
	if(!mRenderEventRingBuffer.Allocate(1024)) {
		os_log_error(sLog, "Unable to create render event ring buffer: SFB::RingBuffer::Allocate failed");
		throw std::runtime_error("SFB::RingBuffer::Allocate failed");
	}

	// Launch the decoding and event processing threads
	try {
		mDecodingThread = std::thread(&SFB::AudioPlayerNode::ProcessDecoders, this);
		mEventThread = std::thread(&SFB::AudioPlayerNode::ProcessEvents, this);
	}
	catch(const std::exception& e) {
		os_log_error(sLog, "Unable to create thread: %{public}s", e.what());
		throw;
	}
}

SFB::AudioPlayerNode::~AudioPlayerNode()
{
	Stop();

	// Stop the decoding thread
	mFlags.fetch_or(eFlagStopDecodingThread, std::memory_order_acq_rel);
	mDecodingSemaphore.Signal();

	// Wait for the decoding thread to exit
	try {
		mDecodingThread.join();
	}
	catch(const std::exception& e) {
		os_log_debug(sLog, "Error joining decoding thread: %{public}s", e.what());
	}

	// Stop the event processing thread
	mFlags.fetch_or(eFlagStopEventThread, std::memory_order_acq_rel);
	mEventSemaphore.Signal();

	// Wait for the event processing thread to exit
	try {
		mEventThread.join();
	}
	catch(const std::exception& e) {
		os_log_debug(sLog, "Error joining event processing thread: %{public}s", e.what());
	}

	// Delete any remaining decoder state
	mActiveDecoders.clear();

	os_log_debug(sLog, "<AudioPlayerNode: %p> destroyed", this);
}

#pragma mark - Playback Properties

SFBPlaybackPosition SFB::AudioPlayerNode::PlaybackPosition() const noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return SFBInvalidPlaybackPosition;
	return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
}

SFBPlaybackTime SFB::AudioPlayerNode::PlaybackTime() const noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return SFBInvalidPlaybackTime;

	SFBPlaybackTime playbackTime = SFBInvalidPlaybackTime;

	const auto framePosition = decoderState->FramePosition();
	const auto frameLength = decoderState->FrameLength();

	if(const auto sampleRate = decoderState->mSampleRate; sampleRate > 0) {
		if(framePosition != SFBUnknownFramePosition)
			playbackTime.currentTime = framePosition / sampleRate;
		if(frameLength != SFBUnknownFrameLength)
			playbackTime.totalTime = frameLength / sampleRate;
	}

	return playbackTime;
}

bool SFB::AudioPlayerNode::GetPlaybackPositionAndTime(SFBPlaybackPosition *playbackPosition, SFBPlaybackTime *playbackTime) const noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
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
		if(const auto sampleRate = decoderState->mSampleRate; sampleRate > 0) {
			if(currentPlaybackPosition.framePosition != SFBUnknownFramePosition)
				currentPlaybackTime.currentTime = currentPlaybackPosition.framePosition / sampleRate;
			if(currentPlaybackPosition.frameLength != SFBUnknownFrameLength)
				currentPlaybackTime.totalTime = currentPlaybackPosition.frameLength / sampleRate;
		}
		*playbackTime = currentPlaybackTime;
	}

	return true;
}

#pragma mark - Seeking

bool SFB::AudioPlayerNode::SeekForward(NSTimeInterval secondsToSkip) noexcept
{
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->mDecoder.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->mSampleRate;
	const auto framePosition = decoderState->FramePosition();
	const auto frameLength = decoderState->FrameLength();

	auto targetFrame = framePosition + static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);
	if(targetFrame >= frameLength)
		targetFrame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(targetFrame);
	mDecodingSemaphore.Signal();

	return true;
}

bool SFB::AudioPlayerNode::SeekBackward(NSTimeInterval secondsToSkip) noexcept
{
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->mDecoder.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->mSampleRate;
	const auto framePosition = decoderState->FramePosition();

	auto targetFrame = framePosition - static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);
	if(targetFrame < 0)
		targetFrame = 0;

	decoderState->RequestSeekToFrame(targetFrame);
	mDecodingSemaphore.Signal();

	return true;
}

bool SFB::AudioPlayerNode::SeekToTime(NSTimeInterval timeInSeconds) noexcept
{
	if(timeInSeconds < 0)
		timeInSeconds = 0;

	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->mDecoder.supportsSeeking)
		return false;

	const auto sampleRate = decoderState->mSampleRate;
	const auto frameLength = decoderState->FrameLength();

	auto targetFrame = static_cast<AVAudioFramePosition>(timeInSeconds * sampleRate);
	if(targetFrame >= frameLength)
		targetFrame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(targetFrame);
	mDecodingSemaphore.Signal();

	return true;
}

bool SFB::AudioPlayerNode::SeekToPosition(double position) noexcept
{
	if(position < 0)
		position = 0;
	else if(position >= 1)
		position = std::nextafter(1.0, 0.0);

	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->mDecoder.supportsSeeking)
		return false;

	const auto frameLength = decoderState->FrameLength();
	const auto targetFrame = static_cast<AVAudioFramePosition>(frameLength * position);

	decoderState->RequestSeekToFrame(targetFrame);
	mDecodingSemaphore.Signal();

	return true;
}

bool SFB::AudioPlayerNode::SeekToFrame(AVAudioFramePosition frame) noexcept
{
	if(frame < 0)
		frame = 0;

	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState || !decoderState->mDecoder.supportsSeeking)
		return false;

	const auto frameLength = decoderState->FrameLength();
	if(frame >= frameLength)
		frame = std::max(frameLength - 1, 0ll);

	decoderState->RequestSeekToFrame(frame);
	mDecodingSemaphore.Signal();
	
	return true;
}

bool SFB::AudioPlayerNode::SupportsSeeking() const noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return false;
	return decoderState->mDecoder.supportsSeeking;
}

#pragma mark - Format Information

bool SFB::AudioPlayerNode::SupportsFormat(AVAudioFormat *format) const noexcept
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
	const auto channelLayoutsAreEquivalent = AVAudioChannelLayoutsAreEquivalent(format.channelLayout, mRenderingFormat.channelLayout);
	return format.channelCount == mRenderingFormat.channelCount && format.sampleRate == mRenderingFormat.sampleRate && channelLayoutsAreEquivalent;
}

#pragma mark - Queue Management

bool SFB::AudioPlayerNode::EnqueueDecoder(Decoder decoder, bool reset, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return false;

	if(!SupportsFormat(decoder.processingFormat)) {
		os_log_error(sLog, "Unsupported decoder processing format: %{public}@", SFB::StringDescribingAVAudioFormat(decoder.processingFormat));

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
		mFlags.fetch_or(eFlagMuteRequested, std::memory_order_acq_rel);
		Reset();
		mFlags.fetch_or(eFlagUmuteAfterDequeue, std::memory_order_acq_rel);
	}

	try {
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		mQueuedDecoders.push_back(decoder);
	}
	catch(const std::exception& e) {
		os_log_error(sLog, "Error pushing %{public}@ to mQueuedDecoders: %{public}s", decoder, e.what());
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		if(reset)
			mFlags.fetch_and(~eFlagMuteRequested & ~eFlagUmuteAfterDequeue, std::memory_order_acq_rel);
		return false;
	}

	os_log_info(sLog, "Enqueued %{public}@", decoder);

	mDecodingSemaphore.Signal();

	return true;
}

SFB::AudioPlayerNode::Decoder SFB::AudioPlayerNode::DequeueDecoder() noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
	Decoder decoder = nil;
	if(!mQueuedDecoders.empty()) {
		decoder = mQueuedDecoders.front();
		mQueuedDecoders.pop_front();
	}
	return decoder;
}

SFB::AudioPlayerNode::Decoder SFB::AudioPlayerNode::CurrentDecoder() const noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
	const auto decoderState = GetFirstDecoderStateWithRenderingNotComplete();
	if(!decoderState)
		return nil;
	return decoderState->mDecoder;
}

void SFB::AudioPlayerNode::CancelActiveDecoders(bool cancelAllActive) noexcept
{
	std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);

	// Cancel all active decoders in sequence
	if(auto decoderState = GetFirstDecoderStateWithRenderingNotComplete(); decoderState) {
		decoderState->mFlags.fetch_or(DecoderState::eFlagCancelRequested, std::memory_order_acq_rel);
		if(cancelAllActive) {
			decoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber);
			while(decoderState) {
				decoderState->mFlags.fetch_or(DecoderState::eFlagCancelRequested, std::memory_order_acq_rel);
				decoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber);
			}
		}

		mDecodingSemaphore.Signal();
	}
}

#pragma mark - Decoding

void SFB::AudioPlayerNode::ProcessDecoders() noexcept
{
#if DEBUG
	assert(std::this_thread::get_id() == mDecodingThread.get_id());
#endif /* DEBUG */

	pthread_setname_np("AudioPlayerNode.Decoding");
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

	os_log_debug(sLog, "Decoding thread starting");

	// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
	AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mRenderingFormat frameCapacity:kRingBufferChunkSize];
	if(!buffer) {
		os_log_error(sLog, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(mRenderingFormat), kRingBufferChunkSize);
		NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeInternalError userInfo:nil];
		SubmitDecodingErrorEvent(error);
		return;
	}

	for(;;) {
		// The decoder state being processed
		DecoderState *decoderState = nullptr;

		// Get the earliest decoder state that has not completed rendering
		{
			std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
			decoderState = GetFirstDecoderStateWithRenderingNotComplete();

			// Process cancelations
			while(decoderState && (decoderState->mFlags.load(std::memory_order_acquire) & DecoderState::eFlagCancelRequested)) {
				os_log_debug(sLog, "Canceling decoding for %{public}@", decoderState->mDecoder);

				mFlags.fetch_or(eFlagRingBufferNeedsReset, std::memory_order_acq_rel);
				decoderState->mFlags.fetch_or(DecoderState::eFlagIsCanceled, std::memory_order_acq_rel);

				// Submit the decoder canceled event
				const DecodingEventHeader header{DecodingEventCommand::eCanceled};
				if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
					mEventSemaphore.Signal();
				else
					os_log_fault(sLog, "Error writing decoder canceled event");

				decoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber);
			}
		}

		// Terminate the thread if requested after processing cancelations
		if(mFlags.load(std::memory_order_acquire) & eFlagStopDecodingThread)
		   break;

		// Process pending seeks
		if(decoderState && decoderState->IsSeekPending()) {
			// If a seek is pending request a ring buffer reset
			mFlags.fetch_or(eFlagRingBufferNeedsReset, std::memory_order_acq_rel);

			decoderState->PerformSeekIfRequired();

			if(decoderState->IsDecodingComplete()) {
				os_log_debug(sLog, "Resuming decoding for %{public}@", decoderState->mDecoder);

				decoderState->mFlags.fetch_and(~DecoderState::eFlagDecodingComplete, std::memory_order_acq_rel);
				decoderState->mFlags.fetch_or(DecoderState::eFlagDecodingResumed, std::memory_order_acq_rel);

				DecoderState *nextDecoderState = nullptr;
				{
					std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
					nextDecoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber);
				}

				// Rewind ensuing decoder states if possible to avoid discarding frames
				while(nextDecoderState && (nextDecoderState->mFlags.load(std::memory_order_acquire) & DecoderState::eFlagDecodingStarted)) {
					os_log_debug(sLog, "Suspending decoding for %{public}@", nextDecoderState->mDecoder);

					// TODO: Investigate a per-state buffer to mitigate frame loss
					if(nextDecoderState->mDecoder.supportsSeeking) {
						nextDecoderState->RequestSeekToFrame(0);
						nextDecoderState->PerformSeekIfRequired();
					}
					else
						os_log_error(sLog, "Discarding %lld frames from %{public}@", nextDecoderState->mFramesDecoded.load(), nextDecoderState->mDecoder);

					nextDecoderState->mFlags.fetch_and(~DecoderState::eFlagDecodingStarted, std::memory_order_acq_rel);
					nextDecoderState->mFlags.fetch_or(DecoderState::eFlagDecodingSuspended, std::memory_order_acq_rel);

					{
						std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
						nextDecoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(nextDecoderState->mSequenceNumber);
					}
				}
			}
		}

		// Get the earliest decoder state that has not completed decoding
		{
			std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
			decoderState = GetFirstDecoderStateWithDecodingNotComplete();
		}

		// Dequeue the next decoder if there are no decoders that haven't completed decoding
		if(!decoderState) {
			if(auto decoder = DequeueDecoder(); decoder) {
				try {
					// Create the decoder state and add it to the list of active decoders
					// When the decoder's processing format and rendering format don't match
					// conversion will be performed in DecoderState::DecodeAudio()
					std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
					mActiveDecoders.push_back(std::make_unique<DecoderState>(decoder, mRenderingFormat, kRingBufferChunkSize));
					decoderState = mActiveDecoders.back().get();
				}
				catch(const std::exception& e) {
					os_log_error(sLog, "Error creating decoder state for %{public}@: %{public}s", decoder, e.what());
					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeInternalError userInfo:nil];
					SubmitDecodingErrorEvent(error);
					continue;
				}

				// Clear the mute flags if needed
				if(mFlags.load(std::memory_order_acquire) & eFlagUmuteAfterDequeue)
					mFlags.fetch_and(~eFlagIsMuted & ~eFlagMuteRequested & ~eFlagUmuteAfterDequeue, std::memory_order_acq_rel);

				os_log_debug(sLog, "Dequeued %{public}@, processing format %{public}@", decoderState->mDecoder, SFB::StringDescribingAVAudioFormat(decoderState->mDecoder.processingFormat));
			}
		}

		if(decoderState) {
			// Reset the ring buffer if required, to prevent audible artifacts
			if(mFlags.load(std::memory_order_acquire) & eFlagRingBufferNeedsReset) {
				mFlags.fetch_and(~eFlagRingBufferNeedsReset, std::memory_order_acq_rel);

				// Ensure rendering is muted before performing operations on the ring buffer that aren't thread-safe
				if(!(mFlags.load(std::memory_order_acquire) & eFlagIsMuted)) {
					if(mNode.engine.isRunning) {
						mFlags.fetch_or(eFlagMuteRequested, std::memory_order_acq_rel);

						// The render block will clear eFlagMuteRequested and set eFlagIsMuted
						while(!(mFlags.load(std::memory_order_acquire) & eFlagIsMuted)) {
							auto timeout = mDecodingSemaphore.Wait(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC));
							// If the timeout occurred the engine may have stopped since the initial check
							// with no subsequent opportunity for the render block to set eFlagIsMuted
							if(!timeout && !mNode.engine.isRunning) {
								mFlags.fetch_or(eFlagIsMuted, std::memory_order_acq_rel);
								mFlags.fetch_and(~eFlagMuteRequested, std::memory_order_acq_rel);
								break;
							}
						}
					}
					else
						mFlags.fetch_or(eFlagIsMuted, std::memory_order_acq_rel);
				}

				// Reset() is not thread-safe but the render block is outputting silence
				mAudioRingBuffer.Reset();

				// Clear the mute flag
				mFlags.fetch_and(~eFlagIsMuted, std::memory_order_acq_rel);
			}

			// Decode and write chunks to the ring buffer
			while(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize) {
				if(const auto flags = decoderState->mFlags.load(std::memory_order_acquire); !(flags & DecoderState::eFlagDecodingStarted)) {
					const bool suspended = flags & DecoderState::eFlagDecodingSuspended;

					if(!suspended)
						os_log_debug(sLog, "Decoding starting for %{public}@", decoderState->mDecoder);
					else
						os_log_debug(sLog, "Decoding starting after suspension for %{public}@", decoderState->mDecoder);

					decoderState->mFlags.fetch_or(DecoderState::eFlagDecodingStarted, std::memory_order_acq_rel);

					// Submit the decoding started event for the initial start only
					if(!suspended) {
						const DecodingEventHeader header{DecodingEventCommand::eStarted};
						if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
							mEventSemaphore.Signal();
						else
							os_log_fault(sLog, "Error writing decoding started event");
					}
				}

				// Decode audio into the buffer, converting to the rendering format in the process
				if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
					os_log_error(sLog, "Error decoding audio: %{public}@", error);
					if(error)
						SubmitDecodingErrorEvent(error);
				}

				// Write the decoded audio to the ring buffer for rendering
				const auto framesWritten = mAudioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
				if(framesWritten != buffer.frameLength)
					os_log_error(sLog, "SFB::AudioRingBuffer::Write() failed");

				if(const auto flags = decoderState->mFlags.load(std::memory_order_acquire); flags & DecoderState::eFlagDecodingComplete) {
					const bool resumed = flags & DecoderState::eFlagDecodingResumed;

					// Submit the decoding complete event for the first completion only
					if(!resumed) {
						const DecodingEventHeader header{DecodingEventCommand::eComplete};
						if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
							mEventSemaphore.Signal();
						else
							os_log_fault(sLog, "Error writing decoding complete event");
					}

					if(!resumed)
						os_log_debug(sLog, "Decoding complete for %{public}@", decoderState->mDecoder);
					else
						os_log_debug(sLog, "Decoding complete after resuming for %{public}@", decoderState->mDecoder);

					break;
				}
			}
		}

		// Wait for additional space in the ring buffer, another event signal, or for another decoder to be enqueued
		mDecodingSemaphore.Wait();
	}

	os_log_debug(sLog, "Decoding thread complete");
}

void SFB::AudioPlayerNode::SubmitDecodingErrorEvent(NSError *error) noexcept
{
#if DEBUG
	assert(std::this_thread::get_id() == mDecodingThread.get_id());
#endif /* DEBUG */
	NSCParameterAssert(error != nil);

	NSError *err = nil;
	NSData *errorData = [NSKeyedArchiver archivedDataWithRootObject:error requiringSecureCoding:YES error:&err];
	if(!errorData) {
		os_log_error(sLog, "Error archiving NSError for decoding error event: %{public}@", err);
		return;
	}

	const auto spaceNeeded = sizeof(DecodingEventHeader) + sizeof(uint32_t) + errorData.length;
	if(spaceNeeded > mDecodeEventRingBuffer.BytesAvailableToWrite()) {
		os_log_fault(sLog, "Insufficient space to write decoding error event");
		return;
	}

	// Event header and payload
	const DecodingEventHeader header{DecodingEventCommand::eError};
	const uint32_t dataSize = static_cast<uint32_t>(errorData.length);
	const void *data = errorData.bytes;

	uint32_t bytesWritten = 0;
	auto wvec = mDecodeEventRingBuffer.WriteVector();

	const auto do_write = [&bytesWritten, wvec](const void *arg, uint32_t sz) noexcept {
		auto bytesRemaining = sz;
		// Write to wvec.first if space is available
		if(wvec.first.mBufferCapacity > bytesWritten) {
			const auto n = std::min(bytesRemaining, wvec.first.mBufferCapacity - bytesWritten);
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(wvec.first.mBuffer) + bytesWritten),
						arg,
						n);
			bytesRemaining -= n;
			bytesWritten += n;
		}
		// Write to wvec.second
		if(bytesRemaining > 0){
			const auto n = bytesRemaining;
			std::memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(wvec.second.mBuffer) + (bytesWritten - wvec.first.mBufferCapacity)),
						arg,
						n);
			bytesWritten += n;
		}
	};

	do_write(&header, sizeof(header));
	do_write(&dataSize, sizeof(dataSize));
	do_write(data, dataSize);

	mDecodeEventRingBuffer.AdvanceWritePosition(bytesWritten);
	mEventSemaphore.Signal();
}

#pragma mark - Rendering

OSStatus SFB::AudioPlayerNode::Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) noexcept
{
	// Mute if requested
	if(mFlags.load(std::memory_order_acquire) & eFlagMuteRequested) {
		mFlags.fetch_or(eFlagIsMuted, std::memory_order_acq_rel);
		mFlags.fetch_and(~eFlagMuteRequested, std::memory_order_acq_rel);
		mDecodingSemaphore.Signal();
	}

	// N.B. The ring buffer must not be read from or written to when eFlagIsMuted is set
	// because the decoding queue could be performing non-thread safe operations

	// Output silence if not playing or muted
	if(const auto flags = mFlags.load(std::memory_order_acquire); !(flags & eFlagIsPlaying) || (flags & eFlagIsMuted)) {
		const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
			std::memset(outputData->mBuffers[i].mData, 0, byteCountToZero);
			outputData->mBuffers[i].mDataByteSize = byteCountToZero;
		}
		isSilence = YES;
		return noErr;
	}

	// If there are audio frames available to read from the ring buffer read as many as possible
	if(const auto framesAvailableToRead = mAudioRingBuffer.FramesAvailableToRead(); framesAvailableToRead > 0) {
		const auto framesToRead = std::min(framesAvailableToRead, frameCount);
		const uint32_t framesRead = mAudioRingBuffer.Read(outputData, framesToRead);
		if(framesRead != framesToRead)
			os_log_fault(sLog, "SFB::AudioRingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

		// If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
#if DEBUG
			os_log_debug(sLog, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);
#endif /* DEBUG */

			const auto framesOfSilence = frameCount - framesRead;
			const auto byteCountToSkip = mAudioRingBuffer.Format().FrameCountToByteSize(framesRead);
			const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(framesOfSilence);
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(outputData->mBuffers[i].mData) + byteCountToSkip), 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize += byteCountToZero;
			}
		}

		// If there is adequate space in the ring buffer for another chunk signal the decoding thread
		if(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize)
			mDecodingSemaphore.Signal();

		const RenderingEventHeader header{RenderingEventCommand::eFramesRendered};
		if(mRenderEventRingBuffer.WriteValues(header, timestamp, framesRead))
			mEventSemaphore.Signal();
		else
			os_log_fault(sLog, "Error writing frames rendered event");
	}
	// Output silence if the ring buffer is empty
	else {
		const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
		for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
			std::memset(outputData->mBuffers[i].mData, 0, byteCountToZero);
			outputData->mBuffers[i].mDataByteSize = byteCountToZero;
		}
		isSilence = YES;
	}

	return noErr;
}

#pragma mark - Event Processing

void SFB::AudioPlayerNode::ProcessEvents() noexcept
{
#if DEBUG
	assert(std::this_thread::get_id() == mEventThread.get_id());
#endif /* DEBUG */

	pthread_setname_np("AudioPlayerNode.Events");
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

	os_log_debug(sLog, "Event processing thread starting");

	while(!(mFlags.load(std::memory_order_acquire) & eFlagStopEventThread)) {
		auto decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
		auto renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();

		// Process all pending decode and render events in chronological order
		for(;;) {
			// Nothing left to do
			if(!decodeEventHeader && !renderEventHeader)
				break;
			// Process the decode event
			else if(decodeEventHeader && !renderEventHeader) {
				ProcessEvent(*decodeEventHeader);
				decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
			}
			// Process the render event
			else if(!decodeEventHeader && renderEventHeader) {
				ProcessEvent(*renderEventHeader);
				renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();
			}
			// Process the event with an earlier identification number
			else if(decodeEventHeader->mIdentificationNumber < renderEventHeader->mIdentificationNumber) {
				ProcessEvent(*decodeEventHeader);
				decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
			}
			else {
				ProcessEvent(*renderEventHeader);
				renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();
			}
		}

		// Wait for the next event
		mEventSemaphore.Wait();
	}

	os_log_debug(sLog, "Event processing thread complete");
}

void SFB::AudioPlayerNode::ProcessEvent(const DecodingEventHeader& header) noexcept
{
#if DEBUG
	assert(std::this_thread::get_id() == mEventThread.get_id());
#endif /* DEBUG */

	switch(header.mCommand) {
		case DecodingEventCommand::eStarted:
			if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(sLog, "Decoder state with sequence number %llu missing for decoding started event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->mDecoder;
				}

				if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)])
					[mNode.delegate audioPlayerNode:mNode decodingStarted:decoder];
			}
			else
				os_log_fault(sLog, "Missing decoder sequence number for decoding started event");
			break;

		case DecodingEventCommand::eComplete:
			if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;

				{
					std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(sLog, "Decoder state with sequence number %llu missing for decoding complete event", decoderSequenceNumber);
						break;
					}
					decoder = decoderState->mDecoder;
				}

				if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)])
					[mNode.delegate audioPlayerNode:mNode decodingComplete:decoder];
			}
			else
				os_log_fault(sLog, "Missing decoder sequence number for decoding complete event");
			break;

		case DecodingEventCommand::eCanceled:
			if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
				Decoder decoder;
				AVAudioFramePosition framesRendered;

				{
					std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(sLog, "Decoder state with sequence number %llu missing for decoder canceled event", decoderSequenceNumber);
						break;
					}

					decoder = decoderState->mDecoder;
					framesRendered = decoderState->FramesRendered();

					if(!DeleteDecoderStateWithSequenceNumber(decoderSequenceNumber))
						os_log_fault(sLog, "Unable to delete decoder state with sequence number %llu in decoder canceled event", decoderSequenceNumber);
				}

				if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decoderCanceled:framesRendered:)])
					[mNode.delegate audioPlayerNode:mNode decoderCanceled:decoder framesRendered:framesRendered];
			}
			else
				os_log_fault(sLog, "Missing decoder sequence number for decoder canceled event");
			break;

		case DecodingEventCommand::eError:
		{
			uint32_t dataSize;
			if(!mDecodeEventRingBuffer.ReadValue(dataSize)) {
				os_log_fault(sLog, "Missing data size for decoding error event");
				break;
			}

			NSMutableData *data = [NSMutableData dataWithLength:dataSize];
			if(!mDecodeEventRingBuffer.Read(data.mutableBytes, dataSize)) {
				os_log_fault(sLog, "Missing archived NSError for decoding error event");
				break;
			}

			NSError *err = nil;
			NSError *error = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSError class] fromData:data error:&err];
			if(!error) {
				os_log_error(sLog, "Error unarchiving NSError for decoding error event: %{public}@", err);
				break;
			}

			if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)])
				[mNode.delegate audioPlayerNode:mNode encounteredError:error];
			break;
		}

		default:
			os_log_fault(sLog, "Unknown decode event command: %u", header.mCommand);
			break;
	}
}

void SFB::AudioPlayerNode::ProcessEvent(const RenderingEventHeader& header) noexcept
{
#if DEBUG
	assert(std::this_thread::get_id() == mEventThread.get_id());
#endif /* DEBUG */

	switch(header.mCommand) {
		case RenderingEventCommand::eFramesRendered:
			// The timestamp of the render cycle
			AudioTimeStamp timestamp;
			// The number of valid frames rendered
			uint32_t framesRendered;

			if(mRenderEventRingBuffer.ReadValues(timestamp, framesRendered)) {
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
					uint64_t hostTime;
					Decoder startedDecoder = nil;
					Decoder nextDecoder = nil;
					Decoder completeDecoder = nil;

					{
						std::lock_guard<SFB::UnfairLock> lock(mDecoderLock);
						if(decoderState)
							decoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber);
						else
							decoderState = GetFirstDecoderStateWithRenderingNotComplete();
						if(!decoderState)
							break;

						const auto decoderFramesRemaining = decoderState->FramesAvailableToRender();
						const auto framesFromThisDecoder = std::min(decoderFramesRemaining, framesRemainingToDistribute);

						// Rendering is starting
						if(!decoderState->HasRenderingStarted() && framesFromThisDecoder > 0) {
							decoderState->mFlags.fetch_or(DecoderState::eFlagRenderingStarted, std::memory_order_acq_rel);

							const auto frameOffset = framesRendered - framesRemainingToDistribute;
							const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
							hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

							const auto now = SFB::GetCurrentHostTime();
							if(now > hostTime)
								os_log_error(sLog, "Rendering started event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->mDecoder);
#if DEBUG
							else
								os_log_debug(sLog, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->mDecoder);
#endif /* DEBUG */

							startedDecoder = decoderState->mDecoder;
						}

						decoderState->AddFramesRendered(framesFromThisDecoder);
						framesRemainingToDistribute -= framesFromThisDecoder;

						// Rendering is complete
						if(decoderState->IsDecodingComplete() && decoderState->AllAvailableFramesRendered()) {
							decoderState->mFlags.fetch_or(DecoderState::eFlagRenderingComplete, std::memory_order_acq_rel);

							completeDecoder = decoderState->mDecoder;

							// Check for a decoder transition
							if(const auto nextDecoderState = GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(decoderState->mSequenceNumber); nextDecoderState) {
								const auto nextDecoderFramesRemaining = nextDecoderState->FramesAvailableToRender();
								const auto framesFromNextDecoder = std::min(nextDecoderFramesRemaining, framesRemainingToDistribute);

#if DEBUG
								assert(!nextDecoderState->HasRenderingStarted());
#endif /* DEBUG */

								nextDecoderState->mFlags.fetch_or(DecoderState::eFlagRenderingStarted, std::memory_order_acq_rel);

								nextDecoderState->AddFramesRendered(framesFromNextDecoder);
								framesRemainingToDistribute -= framesFromNextDecoder;

								const auto frameOffset = framesRendered - framesRemainingToDistribute;
								const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
								hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

								const auto now = SFB::GetCurrentHostTime();
								if(now > hostTime)
									os_log_error(sLog, "Rendering decoder changed event processed %.2f msec late for transition from %{public}@ to %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->mDecoder, nextDecoderState->mDecoder);
#if DEBUG
								else
									os_log_debug(sLog, "Rendering decoder will change in %.2f msec from %{public}@ to %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->mDecoder, nextDecoderState->mDecoder);
#endif /* DEBUG */

								nextDecoder = nextDecoderState->mDecoder;

								if(!DeleteDecoderStateWithSequenceNumber(decoderState->mSequenceNumber))
									os_log_fault(sLog, "Unable to delete decoder state with sequence number %llu in rendering decoder changed event", decoderState->mSequenceNumber);

								decoderState = nextDecoderState;
							}
							else {
								const auto frameOffset = framesRendered - framesRemainingToDistribute;
								const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
								hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

								const auto now = SFB::GetCurrentHostTime();
								if(now > hostTime)
									os_log_error(sLog, "Rendering complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->mDecoder);
#if DEBUG
								else
									os_log_debug(sLog, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->mDecoder);
#endif /* DEBUG */

								if(!DeleteDecoderStateWithSequenceNumber(decoderState->mSequenceNumber))
									os_log_fault(sLog, "Unable to delete decoder state with sequence number %llu in rendering complete event", decoderState->mSequenceNumber);
							}
						}
					}

					// Send delegate messages after unlock
					if(startedDecoder && [mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)])
						[mNode.delegate audioPlayerNode:mNode renderingWillStart:startedDecoder atHostTime:hostTime];

					if(nextDecoder && [mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingDecoder:willChangeToDecoder:atHostTime:)])
						[mNode.delegate audioPlayerNode:mNode renderingDecoder:completeDecoder willChangeToDecoder:nextDecoder atHostTime:hostTime];
					else if(completeDecoder && [mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillComplete:atHostTime:)])
						[mNode.delegate audioPlayerNode:mNode renderingWillComplete:completeDecoder atHostTime:hostTime];
				}
			}
			else
				os_log_fault(sLog, "Missing timestamp or frames rendered for frames rendered event");
			break;

		default:
			os_log_fault(sLog, "Unknown render event command: %u", header.mCommand);
			break;
	}
}

#pragma mark - Active Decoder Management

SFB::AudioPlayerNode::DecoderState * SFB::AudioPlayerNode::GetFirstDecoderStateWithDecodingNotComplete() const noexcept
{
#if DEBUG
	mDecoderLock.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(mActiveDecoders.cbegin(), mActiveDecoders.cend(), [](const auto& decoderState){
		const auto flags = decoderState->mFlags.load(std::memory_order_acquire);
		const bool canceled = flags & DecoderState::eFlagIsCanceled;
		const bool decodingComplete = flags & DecoderState::eFlagDecodingComplete;
		return !canceled && !decodingComplete;
	});
	if(iter == mActiveDecoders.end())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayerNode::DecoderState * SFB::AudioPlayerNode::GetFirstDecoderStateWithRenderingNotComplete() const noexcept
{
#if DEBUG
	mDecoderLock.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(mActiveDecoders.cbegin(), mActiveDecoders.cend(), [](const auto& decoderState){
		const auto flags = decoderState->mFlags.load(std::memory_order_acquire);
		const bool canceled = flags & DecoderState::eFlagIsCanceled;
		const bool renderingComplete = flags & DecoderState::eFlagRenderingComplete;
		return !canceled && !renderingComplete;
	});
	if(iter == mActiveDecoders.end())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayerNode::DecoderState * SFB::AudioPlayerNode::GetFirstDecoderStateFollowingSequenceNumberWithRenderingNotComplete(const uint64_t sequenceNumber) const noexcept
{
#if DEBUG
	mDecoderLock.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(mActiveDecoders.cbegin(), mActiveDecoders.cend(), [sequenceNumber](const auto& decoderState){
		if(decoderState->mSequenceNumber <= sequenceNumber)
			return false;
		const auto flags = decoderState->mFlags.load(std::memory_order_acquire);
		const bool canceled = flags & DecoderState::eFlagIsCanceled;
		const bool renderingComplete = flags & DecoderState::eFlagRenderingComplete;
		return !canceled && !renderingComplete;
	});
	if(iter == mActiveDecoders.end())
		return nullptr;
	return iter->get();
}

SFB::AudioPlayerNode::DecoderState * SFB::AudioPlayerNode::GetDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) const noexcept
{
#if DEBUG
	mDecoderLock.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(mActiveDecoders.cbegin(), mActiveDecoders.cend(), [sequenceNumber](const auto& decoderState){
		return decoderState->mSequenceNumber == sequenceNumber;
	});
	if(iter == mActiveDecoders.end())
		return nullptr;
	return iter->get();
}

bool SFB::AudioPlayerNode::DeleteDecoderStateWithSequenceNumber(const uint64_t sequenceNumber) noexcept
{
#if DEBUG
	mDecoderLock.assert_owner();
#endif /* DEBUG */

	const auto iter = std::find_if(mActiveDecoders.cbegin(), mActiveDecoders.cend(), [sequenceNumber](const auto& decoderState){
		return decoderState->mSequenceNumber == sequenceNumber;
	});
	if(iter == mActiveDecoders.end())
		return false;

	os_log_debug(sLog, "Deleting decoder state for %{public}@", (*iter)->mDecoder);
	mActiveDecoders.erase(iter);

	return true;
}
