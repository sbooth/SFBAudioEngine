//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <array>
#import <atomic>
#import <cassert>
#import <cmath>
#import <exception>
#import <memory>
#import <mutex>
#import <queue>

#import <os/log.h>

#import <AudioToolbox/AudioFormat.h>

#import <SFBAudioRingBuffer.hpp>
#import <SFBDispatchSemaphore.hpp>
#import <SFBRingBuffer.hpp>
#import <SFBUnfairLock.hpp>

#import "SFBAudioPlayerNode.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "SFBStringDescribingAVAudioFormat.h"
#import "SFBTimeUtilities.hpp"

const NSTimeInterval SFBUnknownTime = -1;
NSErrorDomain const SFBAudioPlayerNodeErrorDomain = @"org.sbooth.AudioEngine.AudioPlayerNode";

namespace {

#pragma mark - Shared State

const os_log_t _audioPlayerNodeLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

#pragma mark - AVAudioChannelLayout Equivalence

/// Returns `true` if `lhs` and `rhs` are equivalent
///
/// Channel layouts are considered equivalent if:
/// 1) Both channel layouts are `nil`
/// 2) One channel layout is `nil` and the other has a mono or stereo layout tag
/// 3) `kAudioFormatProperty_AreChannelLayoutsEquivalent` is true
bool AVAudioChannelLayoutsAreEquivalent(AVAudioChannelLayout * _Nullable lhs, AVAudioChannelLayout * _Nullable rhs) noexcept
{
	if(!lhs && !rhs)
		return true;
	else if(lhs && !rhs) {
		auto layoutTag = lhs.layoutTag;
		if(layoutTag == kAudioChannelLayoutTag_Mono || layoutTag == kAudioChannelLayoutTag_Stereo)
			return true;
	}
	else if(!lhs && rhs) {
		auto layoutTag = rhs.layoutTag;
		if(layoutTag == kAudioChannelLayoutTag_Mono || layoutTag == kAudioChannelLayoutTag_Stereo)
			return true;
	}

	if(!lhs || !rhs)
		return false;

	const AudioChannelLayout *layouts [] = {
		lhs.layout,
		rhs.layout
	};

	UInt32 layoutsEqual = 0;
	UInt32 propertySize = sizeof(layoutsEqual);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), static_cast<const void *>(layouts), &propertySize, &layoutsEqual);

	if(noErr != result)
		return false;

	return layoutsEqual;
}

#pragma mark - Decoder State

/// State for tracking/syncing decoding progress
struct DecoderState {
	using atomic_ptr = std::atomic<DecoderState *>;
	static_assert(atomic_ptr::is_always_lock_free, "Lock-free std::atomic<DecoderState *> required");

	static constexpr AVAudioFrameCount 	kDefaultFrameCapacity 	= 1024;
	static constexpr int64_t			kInvalidFramePosition 	= -1;

	enum DecoderStateFlags : unsigned int {
		eFlagCancelDecoding 	= 1u << 0,
		eFlagDecodingStarted 	= 1u << 1,
		eFlagDecodingComplete 	= 1u << 2,
		eFlagRenderingStarted 	= 1u << 3,
		eFlagRenderingComplete 	= 1u << 4,
	};

	/// Monotonically increasing instance counter
	const uint64_t			mSequenceNumber 	= sSequenceNumber++;

	/// Decoder state flags
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
	std::atomic_int64_t 	mFrameToSeek 		= kInvalidFramePosition;

	static_assert(std::atomic_int64_t::is_always_lock_free, "Lock-free std::atomic_int64_t required");

	/// Decodes audio from the source representation to PCM
	id <SFBPCMDecoding> 	mDecoder 			= nil;
	/// Converts audio from the decoder's processing format to another PCM variant at the same sample rate
	AVAudioConverter 		*mConverter 		= nil;
	/// Buffer used internally for buffering during conversion
	AVAudioPCMBuffer 		*mDecodeBuffer 		= nil;

	/// Next sequence number to use
	static uint64_t			sSequenceNumber;

	DecoderState(id <SFBPCMDecoding> _Nonnull decoder, AVAudioFormat * _Nonnull format, AVAudioFrameCount frameCapacity = kDefaultFrameCapacity)
	: mFrameLength{decoder.frameLength}, mDecoder{decoder}
	{
#if DEBUG
		assert(decoder != nil);
		assert(format != nil);
#endif /* DEBUG */

		mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
		if(!mConverter) {
			os_log_error(_audioPlayerNodeLog, "Error creating AVAudioConverter converting from %{public}@ to %{public}@", mDecoder.processingFormat, format);
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
		const auto seek = mFrameToSeek.load();
		return seek == kInvalidFramePosition ? mFramesRendered.load() : seek;
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
			mFlags.fetch_or(eFlagDecodingComplete);
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
			mFlags.fetch_or(eFlagDecodingComplete);

		return true;
	}

	/// Returns `true` if there is a pending seek request
	bool HasPendingSeek() const noexcept
	{
		return mFrameToSeek.load() != kInvalidFramePosition;
	}

	/// Sets the pending seek request to `frame`
	void RequestSeekToFrame(AVAudioFramePosition frame) noexcept
	{
		mFrameToSeek.store(frame);
	}

	/// Performs the pending seek request, if present
	bool PerformPendingSeek() noexcept
	{
		auto seekOffset = mFrameToSeek.load();
		if(seekOffset == kInvalidFramePosition)
			return true;

		os_log_debug(_audioPlayerNodeLog, "Seeking to frame %lld in %{public}@ ", seekOffset, mDecoder);

		if([mDecoder seekToFrame:seekOffset error:nil])
			// Reset the converter to flush any buffers
			[mConverter reset];
		else
			os_log_debug(_audioPlayerNodeLog, "Error seeking to frame %lld", seekOffset);

		const auto newFrame = mDecoder.framePosition;
		if(newFrame != seekOffset) {
			os_log_debug(_audioPlayerNodeLog, "Inaccurate seek to frame %lld, got %lld", seekOffset, newFrame);
			seekOffset = newFrame;
		}

		// Update the seek request
		mFrameToSeek.store(kInvalidFramePosition);

		// Update the frame counters accordingly
		// A seek is handled in essentially the same way as initial playback
		if(newFrame != kInvalidFramePosition) {
			mFramesDecoded.store(newFrame);
			mFramesConverted.store(seekOffset);
			mFramesRendered.store(seekOffset);
		}

		return newFrame != kInvalidFramePosition;
	}
};

uint64_t DecoderState::sSequenceNumber = 1;
using DecoderQueue = std::deque<id <SFBPCMDecoding>>;

#pragma mark - Decoder State Array

constexpr size_t kDecoderStateArraySize = 8;
using DecoderStateArray = std::array<DecoderState::atomic_ptr, kDecoderStateArraySize>;

/// Returns the element in `decoders` with the smallest sequence number that has not completed rendering
DecoderState * _Nullable GetActiveDecoderStateWithSmallestSequenceNumber(const DecoderStateArray& decoders) noexcept
{
	DecoderState *result = nullptr;
	for(const auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(const auto flags = decoderState->mFlags.load(); flags & DecoderState::eFlagRenderingComplete)
			continue;

		if(!result)
			result = decoderState;
		else if(decoderState->mSequenceNumber < result->mSequenceNumber)
			result = decoderState;
	}

	return result;
}

/// Returns the element in `decoders` with the smallest sequence number greater than `sequenceNumber` that has not completed rendering
DecoderState * _Nullable GetActiveDecoderStateFollowingSequenceNumber(const DecoderStateArray& decoders, const uint64_t& sequenceNumber) noexcept
{
	DecoderState *result = nullptr;
	for(const auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(const auto flags = decoderState->mFlags.load(); flags & DecoderState::eFlagRenderingComplete)
			continue;

		if(!result && decoderState->mSequenceNumber > sequenceNumber)
			result = decoderState;
		else if(result && decoderState->mSequenceNumber > sequenceNumber && decoderState->mSequenceNumber < result->mSequenceNumber)
			result = decoderState;
	}

	return result;
}

/// Returns the element in `decoders` with sequence number equal to `sequenceNumber`
DecoderState * _Nullable GetDecoderStateWithSequenceNumber(const DecoderStateArray& decoders, const uint64_t& sequenceNumber) noexcept
{
	for(const auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(decoderState->mSequenceNumber == sequenceNumber)
			return decoderState;
	}

	return nullptr;
}

/// Deletes the element in `decoders` with sequence number equal to `sequenceNumber`
void DeleteDecoderStateWithSequenceNumber(DecoderStateArray& decoders, const uint64_t& sequenceNumber) noexcept
{
	for(auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState || decoderState->mSequenceNumber != sequenceNumber)
			continue;

		os_log_debug(_audioPlayerNodeLog, "Deleting decoder state for %{public}@", decoderState->mDecoder);
		delete atomic_ptr.exchange(nullptr);
	}
}

#pragma mark - Events

/// Returns the next event identification number
/// - note: Event identification numbers are unique across all event types
uint64_t NextEventIdentificationNumber() noexcept
{
	static std::atomic_uint64_t nextIdentificationNumber = 1;
	static_assert(std::atomic_uint64_t::is_always_lock_free, "Lock-free std::atomic_uint64_t required");
	return nextIdentificationNumber.fetch_add(1);
}

/// An event header consisting of an event command and event identification number
template <typename T, typename = std::enable_if_t<std::is_same_v<std::underlying_type_t<T>, uint32_t>>>
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

#pragma mark Decoding Events

/// Decoding queue events
enum class DecodingEventCommand : uint32_t {
	eStarted 	= 1,
	eComplete 	= 2,
	eCanceled 	= 3,
	eError 		= 4,
};

/// A decoding event header
using DecodingEventHeader = EventHeader<DecodingEventCommand>;

#pragma mark Rendering Events

/// Render block events
enum class RenderingEventCommand : uint32_t {
	eStarted 		= 1,
	eComplete 		= 2,
	eEndOfAudio		= 3,
};

/// A rendering event command and identification number
using RenderingEventHeader = EventHeader<RenderingEventCommand>;

#pragma mark - AudioPlayerNode

void release_nserror_f(void *context)
{
	(void)(__bridge_transfer NSError *)context;
}

/// SFBAudioPlayerNode implementation
struct AudioPlayerNode {
	using unique_ptr = std::unique_ptr<AudioPlayerNode>;

	/// The minimum number of frames to write to the ring buffer
	static constexpr AVAudioFrameCount kRingBufferChunkSize = 2048;

	enum AudioPlayerNodeFlags : unsigned int {
		eFlagIsPlaying 				= 1u << 0,
		eFlagIsMuted 				= 1u << 1,
		eFlagMuteRequested 			= 1u << 2,
		eFlagRingBufferNeedsReset 	= 1u << 3,
	};

	/// Unsafe reference to owning `SFBAudioPlayerNode` instance
	__unsafe_unretained SFBAudioPlayerNode *mNode 			= nil;

	/// The render block supplying audio
	AVAudioSourceNodeRenderBlock 	mRenderBlock 			= nullptr;

private:
	/// The format of the audio supplied by `mRenderBlock`
	AVAudioFormat 					*mRenderingFormat		= nil;

	/// Ring buffer used to transfer audio between the decoding dispatch queue and the render block
	SFB::AudioRingBuffer			mAudioRingBuffer 		= {};

	/// Active decoders and associated state
	DecoderStateArray 				*mActiveDecoders 		= nullptr;

	/// Decoders enqueued for playback that are not yet active
	DecoderQueue 					mQueuedDecoders 		= {};
	/// Lock used to protect access to `mQueuedDecoders`
	mutable SFB::UnfairLock			mQueueLock;

	/// Dispatch queue used for decoding
	dispatch_queue_t				mDecodingQueue 			= nullptr;
	/// Dispatch semaphore used for communication with the decoding queue
	SFB::DispatchSemaphore			mDecodingSemaphore 		{0};
	/// Dispatch group used to track decoding tasks
	dispatch_group_t 				mDecodingGroup			= nullptr;

	/// Ring buffer used to communicate events from the decoding queue
	SFB::RingBuffer					mDecodeEventRingBuffer;
	/// Ring buffer used to communicate events from the render block
	SFB::RingBuffer					mRenderEventRingBuffer;

	/// Dispatch queue used for event processing
	dispatch_queue_t				mEventProcessingQueue	= nullptr;
	/// Dispatch source initiating event processing by the render block
	dispatch_source_t				mEventProcessingSource 	= nullptr;
	/// Dispatch group used to track event processing initiated by the decoding queue
	dispatch_group_t 				mEventProcessingGroup	= nullptr;

	/// AudioPlayerNode flags
	std::atomic_uint 				mFlags 					= 0;
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

	/// Counter used for unique keys to `dispatch_queue_set_specific`
	std::atomic_uint64_t 			mDispatchKeyCounter 	= 1;

public:
	AudioPlayerNode(AVAudioFormat * _Nonnull format, uint32_t ringBufferSize)
	: mRenderingFormat{format}
	{
#if DEBUG
		assert(format != nil);
#endif /* DEBUG */

		os_log_debug(_audioPlayerNodeLog, "Created <AudioPlayerNode: %p>, rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(mRenderingFormat));

		// Allocate and initialize the decoder state array
		mActiveDecoders = new DecoderStateArray;
		for(auto& atomic_ptr : *mActiveDecoders)
			atomic_ptr.store(nullptr);

		// ========================================
		// Decoding Setup

		// Create the dispatch queue used for decoding
		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
		if(!attr) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_attr_make_with_qos_class failed");
			throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
		}

		mDecodingQueue = dispatch_queue_create_with_target("AudioPlayerNode.Decoding", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!mDecodingQueue) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decoding dispatch queue: dispatch_queue_create_with_target failed");
			throw std::runtime_error("dispatch_queue_create_with_target failed");
		}

		mDecodingGroup = dispatch_group_create();
		if(!mDecodingGroup) {
			os_log_error(_audioPlayerNodeLog, "Unable to decoding dispatch group: dispatch_group_create failed");
			throw std::runtime_error("dispatch_group_create failed");
		}

		// ========================================
		// Rendering Setup

		// Allocate the audio ring buffer moving audio from the decoder queue to the render block
		if(!mAudioRingBuffer.Allocate(*(mRenderingFormat.streamDescription), ringBufferSize)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create audio ring buffer: SFB::Audio::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::Audio::RingBuffer::Allocate failed");
		}

		// Set up the render block
		mRenderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
			return Render(*isSilence, *timestamp, frameCount, outputData);
		};

		// ========================================
		// Event Processing Setup

		// The decode event ring buffer is written to by the decoding queue and read from by the event queue
		if(!mDecodeEventRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decode event ring buffer: SFB::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate failed");
		}

		// The render event ring buffer is written to by the render block and read from by the event queue
		if(!mRenderEventRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create render event ring buffer: SFB::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate failed");
		}

		// Create the dispatch queue used for event processing, reusing the same attributes
		mEventProcessingQueue = dispatch_queue_create_with_target("AudioPlayerNode.Events", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!mEventProcessingQueue) {
			os_log_error(_audioPlayerNodeLog, "Unable to create event processing dispatch queue: dispatch_queue_create_with_target failed");
			throw std::runtime_error("dispatch_queue_create_with_target failed");
		}

		// Create the dispatch source used to trigger event processing from the render block
		mEventProcessingSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, mEventProcessingQueue);
		if(!mEventProcessingSource) {
			os_log_error(_audioPlayerNodeLog, "Unable to create event processing dispatch source: dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		mEventProcessingGroup = dispatch_group_create();
		if(!mEventProcessingGroup) {
			os_log_error(_audioPlayerNodeLog, "Unable to create event processing dispatch group: dispatch_group_create failed");
			throw std::runtime_error("dispatch_group_create failed");
		}

		dispatch_source_set_event_handler(mEventProcessingSource, ^{
			ProcessPendingEvents();
		});

		// Start processing events from the render block
		dispatch_activate(mEventProcessingSource);
	}

	~AudioPlayerNode()
	{
		Stop();

		// Cancel any further event processing initiated by the render block
		dispatch_source_cancel(mEventProcessingSource);

		// Wait for the current decoder to complete cancelation
		dispatch_group_wait(mDecodingGroup, DISPATCH_TIME_FOREVER);

		// Wait for event processing initiated by the decoding queue to complete
		dispatch_group_wait(mEventProcessingGroup, DISPATCH_TIME_FOREVER);

		// Delete any remaining decoder state
		for(auto& atomic_ptr : *mActiveDecoders)
			delete atomic_ptr.exchange(nullptr);
		delete mActiveDecoders;

		os_log_debug(_audioPlayerNodeLog, "<AudioPlayerNode: %p> destroyed", this);
	}

#pragma mark - Playback Control

	void Play() noexcept
	{
		mFlags.fetch_or(eFlagIsPlaying);
	}

	void Pause() noexcept
	{
		mFlags.fetch_and(~eFlagIsPlaying);
	}

	void Stop() noexcept
	{
		mFlags.fetch_and(~eFlagIsPlaying);
		Reset();
	}

	void TogglePlayPause() noexcept
	{
		mFlags.fetch_xor(eFlagIsPlaying);
	}

#pragma mark - Playback State

	bool IsPlaying() const noexcept
	{
		return (mFlags.load() & eFlagIsPlaying) != 0;
	}

	bool IsReady() const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? true : false;
	}

#pragma mark - Playback Properties

	SFBAudioPlayerNodePlaybackPosition PlaybackPosition() const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .framePosition = SFBUnknownFramePosition, .frameLength = SFBUnknownFrameLength };

		return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
	}

	SFBAudioPlayerNodePlaybackTime PlaybackTime() const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		SFBAudioPlayerNodePlaybackTime playbackTime = { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		const auto framePosition = decoderState->FramePosition();
		const auto frameLength = decoderState->FrameLength();

		if(const auto sampleRate = decoderState->mConverter.outputFormat.sampleRate; sampleRate > 0) {
			if(framePosition != SFBUnknownFramePosition)
				playbackTime.currentTime = framePosition / sampleRate;
			if(frameLength != SFBUnknownFrameLength)
				playbackTime.totalTime = frameLength / sampleRate;
		}

		return playbackTime;
	}

	bool GetPlaybackPositionAndTime(SFBAudioPlayerNodePlaybackPosition * _Nullable playbackPosition, SFBAudioPlayerNodePlaybackTime * _Nullable playbackTime) const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState) {
			if(playbackPosition)
				*playbackPosition = { .framePosition = SFBUnknownFramePosition, .frameLength = SFBUnknownFrameLength };
			if(playbackTime)
				*playbackTime = { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };
			return false;
		}

		SFBAudioPlayerNodePlaybackPosition currentPlaybackPosition = { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
		if(playbackPosition)
			*playbackPosition = currentPlaybackPosition;

		if(playbackTime) {
			SFBAudioPlayerNodePlaybackTime currentPlaybackTime = { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };
			if(const auto sampleRate = decoderState->mConverter.outputFormat.sampleRate; sampleRate > 0) {
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

	bool SeekForward(NSTimeInterval secondsToSkip) noexcept
	{
		if(secondsToSkip < 0)
			secondsToSkip = 0;

		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		const auto sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		const auto framePosition = decoderState->FramePosition();
		auto targetFrame = framePosition + static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);

		if(targetFrame >= decoderState->FrameLength())
			targetFrame = std::max(decoderState->FrameLength() - 1, 0ll);

		return SeekToFrame(targetFrame);
	}

	bool SeekBackward(NSTimeInterval secondsToSkip) noexcept
	{
		if(secondsToSkip < 0)
			secondsToSkip = 0;

		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		const auto sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		const auto framePosition = decoderState->FramePosition();
		auto targetFrame = framePosition - static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);

		if(targetFrame < 0)
			targetFrame = 0;

		return SeekToFrame(targetFrame);
	}

	bool SeekToTime(NSTimeInterval timeInSeconds) noexcept
	{
		if(timeInSeconds < 0)
			timeInSeconds = 0;

		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		const auto sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		auto targetFrame = static_cast<AVAudioFramePosition>(timeInSeconds * sampleRate);

		if(targetFrame >= decoderState->FrameLength())
			targetFrame = std::max(decoderState->FrameLength() - 1, 0ll);

		return SeekToFrame(targetFrame);
	}

	bool SeekToPosition(double position) noexcept
	{
		if(position < 0)
			position = 0;
		else if(position >= 1)
			position = std::nextafter(1.0, 0.0);

		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		auto frameLength = decoderState->FrameLength();
		return SeekToFrame(static_cast<AVAudioFramePosition>(frameLength * position));
	}

	bool SeekToFrame(AVAudioFramePosition frame) noexcept
	{
		if(frame < 0)
			frame = 0;

		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState || !decoderState->mDecoder.supportsSeeking)
			return false;

		if(frame >= decoderState->FrameLength())
			frame = std::max(decoderState->FrameLength() - 1, 0ll);

		decoderState->RequestSeekToFrame(frame);
		mDecodingSemaphore.Signal();

		return true;
	}

	bool SupportsSeeking() const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? decoderState->mDecoder.supportsSeeking : false;
	}

#pragma mark - Format Information

	AVAudioFormat * _Nonnull RenderingFormat() const noexcept
	{
		return mRenderingFormat;
	}

	bool SupportsFormat(AVAudioFormat * _Nonnull format) const noexcept
	{
#if DEBUG
		assert(format != nil);
#endif /* DEBUG */

		// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
		const auto channelLayoutsAreEquivalent = AVAudioChannelLayoutsAreEquivalent(format.channelLayout, mRenderingFormat.channelLayout);
		return format.channelCount == mRenderingFormat.channelCount && format.sampleRate == mRenderingFormat.sampleRate && channelLayoutsAreEquivalent;
	}

#pragma mark - Queue Management

	bool EnqueueDecoder(id <SFBPCMDecoding> _Nonnull decoder, bool reset, NSError **error) noexcept
	{
#if DEBUG
		assert(decoder != nil);
#endif /* DEBUG */

		if(!decoder.isOpen && ![decoder openReturningError:error])
			return false;

		if(!SupportsFormat(decoder.processingFormat)) {
			os_log_error(_audioPlayerNodeLog, "Unsupported decoder processing format: %{public}@", SFB::StringDescribingAVAudioFormat(decoder.processingFormat));

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
			mFlags.fetch_or(eFlagMuteRequested);
			Reset();
		}

		os_log_info(_audioPlayerNodeLog, "Enqueuing %{public}@", decoder);

		{
			std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
			mQueuedDecoders.push_back(decoder);
		}

		DequeueAndProcessDecoder(reset);

		return true;
	}

	id <SFBPCMDecoding> _Nullable DequeueDecoder() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		id <SFBPCMDecoding> decoder = nil;
		if(!mQueuedDecoders.empty()) {
			decoder = mQueuedDecoders.front();
			mQueuedDecoders.pop_front();
		}
		return decoder;
	}

	id<SFBPCMDecoding> _Nullable CurrentDecoder() const noexcept
	{
		const auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? decoderState->mDecoder : nil;
	}

	void CancelCurrentDecoder() noexcept
	{
		if(auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(); decoderState) {
			if(decoderState->mFlags.load() & DecoderState::eFlagDecodingComplete) {
#if DEBUG
				os_log_debug(_audioPlayerNodeLog, "Canceling a decoder that has already completed decoding");
#endif /* DEBUG */
				// Submit the decoding canceled event
				const DecodingEventHeader header{DecodingEventCommand::eCanceled};
				if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
					dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
						ProcessPendingEvents();
					});
				else
					os_log_fault(_audioPlayerNodeLog, "Error writing decoding canceled event");
			}
			else {
				decoderState->mFlags.fetch_or(DecoderState::eFlagCancelDecoding);
				mDecodingSemaphore.Signal();
			}
		}
	}

	void ClearQueue() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		mQueuedDecoders.resize(0);
	}

	bool QueueIsEmpty() const noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		return mQueuedDecoders.empty();
	}

	void Reset() noexcept
	{
		ClearQueue();
		CancelCurrentDecoder();
	}

private:

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number that has not completed rendering
	DecoderState * _Nullable GetActiveDecoderStateWithSmallestSequenceNumber() const noexcept
	{
		return ::GetActiveDecoderStateWithSmallestSequenceNumber(*mActiveDecoders);
	}

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number greater than `sequenceNumber` that has not completed rendering
	DecoderState * _Nullable GetActiveDecoderStateFollowingSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetActiveDecoderStateFollowingSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

	/// Returns the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	DecoderState * _Nullable GetDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetDecoderStateWithSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

	/// Deletes the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber`
	void DeleteDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) noexcept
	{
		::DeleteDecoderStateWithSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

#pragma mark - Decoding

	void DequeueAndProcessDecoder(bool unmuteNeeded) noexcept
	{
		dispatch_group_async(mDecodingGroup, mDecodingQueue, ^{
			// Dequeue and process the next decoder
			if(auto decoder = DequeueDecoder(); decoder) {
				// Create the decoder state
				DecoderState *decoderState = nullptr;

				try {
					// When the decoder's processing format and rendering format don't match
					// conversion will be performed in DecoderState::DecodeAudio()
					decoderState = new DecoderState(decoder, mRenderingFormat, kRingBufferChunkSize);
				}

				catch(const std::exception& e) {
					os_log_error(_audioPlayerNodeLog, "Error creating decoder state: %{public}s", e.what());

					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain
														 code:SFBAudioPlayerNodeErrorCodeInternalError
													 userInfo:@{ NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"An internal error occurred in AudioPlayerNode.", @""),
																 NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Error creating DecoderState", @""),
															  }];

					// Submit the error event
					const DecodingEventHeader header{DecodingEventCommand::eError};

					const auto key = mDispatchKeyCounter.fetch_add(1);
					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValues(header, key))
						dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
							ProcessPendingEvents();
						});
					else
						os_log_fault(_audioPlayerNodeLog, "Error writing decoding error event");

					return;
				}

				// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
				AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mRenderingFormat frameCapacity:kRingBufferChunkSize];
				if(!buffer) {
					os_log_error(_audioPlayerNodeLog, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(mRenderingFormat), kRingBufferChunkSize);

					delete decoderState;

					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain
														 code:SFBAudioPlayerNodeErrorCodeInternalError
													 userInfo:@{ NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"An internal error occurred in AudioPlayerNode.", @""),
																 NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Error creating AVAudioPCMBuffer", @""),
															  }];

					// Submit the error event
					const DecodingEventHeader header{DecodingEventCommand::eError};

					const auto key = mDispatchKeyCounter.fetch_add(1);
					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValues(header, key))
						dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
							ProcessPendingEvents();
						});
					else
						os_log_fault(_audioPlayerNodeLog, "Error writing decoding error event");

					return;
				}

				// Add the decoder state to the list of active decoders
				auto stored = false;
				do {
					for(auto& atomic_ptr : *mActiveDecoders) {
						auto current = atomic_ptr.load();
						if(current)
							continue;

						// In essence `mActiveDecoders` is an SPSC queue with `mDecodingQueue` as producer
						// and the event processor as consumer, with the stored values used in between production
						// and consumption by any number of other threads/queues including the render block.
						//
						// Slots in `mActiveDecoders` are assigned values in two places: here and the
						// event processor. The event processor assigns nullptr to slots before deleting
						// the stored value while this code assigns non-null values to slots holding nullptr.
						//
						// Since `mActiveDecoders[i]` was atomically loaded and has been verified not null,
						// it is safe to use store() instead of compare_exchange_strong() because this is the
						// only code that could have changed the slot to a non-null value and it is called solely
						// from the decoding queue.
						// There is the possibility that a non-null value was collected from the slot and the slot
						// was assigned nullptr in between load() and the check for null. If this happens the
						// assignment could have taken place but didn't.
						//
						// When `mActiveDecoders` is full this code either needs to wait for a slot to open up or fail.
						//
						// `mActiveDecoders` may be full when the capacity of mAudioRingBuffer exceeds the
						// total number of audio frames for all the decoders in `mActiveDecoders` and audio is not
						// being consumed by the render block.
						// The default frame capacity for `mAudioRingBuffer` is 16384. With 8 slots available in
						// `mActiveDecoders`, the average number of frames a decoder needs to contain for
						// all slots to be full is 2048. For audio at 8000 Hz that equates to 0.26 sec and at
						// 44,100 Hz 2048 frames equates to 0.05 sec.
						// This code elects to wait for a slot to open up instead of failing.
						// This isn't a concern in practice since the main use case for this class is music, not
						// sequential buffers of 0.05 sec. In normal use it's expected that slots 0 and 1 will
						// be the only ones used.
						atomic_ptr.store(decoderState);
						stored = true;
						break;
					}

					if(!stored) {
						os_log_debug(_audioPlayerNodeLog, "No open slots in mActiveDecoders");
						struct timespec rqtp = {
							.tv_sec = 0,
							.tv_nsec = NSEC_PER_SEC / 20
						};
						nanosleep(&rqtp, nullptr);
					}
				} while(!stored);

				// Clear the mute flags if needed
				if(unmuteNeeded)
					mFlags.fetch_and(~eFlagIsMuted & ~eFlagMuteRequested);

				os_log_debug(_audioPlayerNodeLog, "Dequeued %{public}@, processing format %{public}@", decoderState->mDecoder, SFB::StringDescribingAVAudioFormat(decoderState->mDecoder.processingFormat));

				// Process the decoder until canceled or complete
				for(;;) {
					// If a seek is pending request a ring buffer reset
					if(decoderState->HasPendingSeek())
						mFlags.fetch_or(eFlagRingBufferNeedsReset);

					// Reset the ring buffer if required, to prevent audible artifacts
					if(mFlags.load() & eFlagRingBufferNeedsReset) {
						mFlags.fetch_and(~eFlagRingBufferNeedsReset);

						// Ensure rendering is muted before performing operations on the ring buffer that aren't thread-safe
						if(!(mFlags.load() & eFlagIsMuted)) {
							if(mNode.engine.isRunning) {
								mFlags.fetch_or(eFlagMuteRequested);

								// The render block will clear eFlagMuteRequested and set eFlagIsMuted
								while(!(mFlags.load() & eFlagIsMuted)) {
									auto timeout = mDecodingSemaphore.Wait(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC));
									// If the timeout occurred the engine may have stopped since the initial check
									// with no subsequent opportunity for the render block to set eFlagIsMuted
									if(!timeout && !mNode.engine.isRunning) {
										mFlags.fetch_or(eFlagIsMuted);
										mFlags.fetch_and(~eFlagMuteRequested);
										break;
									}
								}
							}
							else
								mFlags.fetch_or(eFlagIsMuted);
						}

						// Perform seek if one is pending
						if(decoderState->HasPendingSeek())
							decoderState->PerformPendingSeek();

						// Reset() is not thread-safe but the render block is outputting silence
						mAudioRingBuffer.Reset();

						// Clear the mute flag
						mFlags.fetch_and(~eFlagIsMuted);
					}

					if(decoderState->mFlags.load() & DecoderState::eFlagCancelDecoding) {
						os_log_debug(_audioPlayerNodeLog, "Canceling decoding for %{public}@", decoderState->mDecoder);

						mFlags.fetch_or(eFlagRingBufferNeedsReset);

						// Submit the decoding canceled event
						const DecodingEventHeader header{DecodingEventCommand::eCanceled};
						if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
							dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
								ProcessPendingEvents();
							});
						else
							os_log_fault(_audioPlayerNodeLog, "Error writing decoding canceled event");

						return;
					}

					// Decode and write chunks to the ring buffer
					while(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize) {
						if(!(decoderState->mFlags.load() & DecoderState::eFlagDecodingStarted)) {
							os_log_debug(_audioPlayerNodeLog, "Decoding started for %{public}@", decoderState->mDecoder);

							decoderState->mFlags.fetch_or(DecoderState::eFlagDecodingStarted);

							// Submit the decoding started event
							const DecodingEventHeader header{DecodingEventCommand::eStarted};
							if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
								dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
									ProcessPendingEvents();
								});
							else
								os_log_fault(_audioPlayerNodeLog, "Error writing decoding started event");
						}

						// Decode audio into the buffer, converting to the rendering format in the process
						if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
							os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);

							if(error) {
								// Submit the error event
								const DecodingEventHeader header{DecodingEventCommand::eError};

								const auto key = mDispatchKeyCounter.fetch_add(1);
								dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

								if(mDecodeEventRingBuffer.WriteValues(header, key))
									dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
										ProcessPendingEvents();
									});
								else
									os_log_fault(_audioPlayerNodeLog, "Error writing decoding error event");
							}
						}

						// Write the decoded audio to the ring buffer for rendering
						const auto framesWritten = mAudioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
						if(framesWritten != buffer.frameLength)
							os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Write() failed");

						if(decoderState->mFlags.load() & DecoderState::eFlagDecodingComplete) {
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

							// Submit the decoding complete event
							const DecodingEventHeader header{DecodingEventCommand::eComplete};
							if(mDecodeEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber))
								dispatch_group_async(mEventProcessingGroup, mEventProcessingQueue, ^{
									ProcessPendingEvents();
								});
							else
								os_log_fault(_audioPlayerNodeLog, "Error writing decoding complete event");

							os_log_debug(_audioPlayerNodeLog, "Decoding complete for %{public}@", decoderState->mDecoder);

							return;
						}
					}

					// Wait for additional space in the ring buffer or for another event signal
					mDecodingSemaphore.Wait();
				}
			}
		});
	}

	// MARK: - Rendering

	OSStatus Render(BOOL& isSilence, const AudioTimeStamp& timestamp, AVAudioFrameCount frameCount, AudioBufferList * _Nonnull outputData) noexcept
	{
		// ========================================
		// Pre-rendering actions

		// ========================================
		// 0. Mute if requested
		if(mFlags.load() & eFlagMuteRequested) {
			mFlags.fetch_or(eFlagIsMuted);
			mFlags.fetch_and(~eFlagMuteRequested);
			mDecodingSemaphore.Signal();
		}

		// ========================================
		// Rendering

		// N.B. The ring buffer must not be read from or written to when eFlagIsMuted is set
		// because the decoding queue could be performing non-thread safe operations

		// ========================================
		// 1. Output silence if not playing or muted
		if(const auto flags = mFlags.load(); !(flags & eFlagIsPlaying) || flags & eFlagIsMuted) {
			const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(outputData->mBuffers[i].mData, 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize = byteCountToZero;
			}
			isSilence = YES;
			return noErr;
		}

		// ========================================
		// 2. Determine how many audio frames are available to read from the ring buffer
		const auto framesAvailableToRead = static_cast<AVAudioFrameCount>(mAudioRingBuffer.FramesAvailableToRead());

		// ========================================
		// 3. Output silence if the ring buffer is empty
		if(framesAvailableToRead == 0) {
			const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(outputData->mBuffers[i].mData, 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize = byteCountToZero;
			}
			isSilence = YES;
			return noErr;
		}

		// ========================================
		// 4. Read as many frames as available from the ring buffer
		const auto framesToRead = std::min(framesAvailableToRead, frameCount);
		const auto framesRead = static_cast<AVAudioFrameCount>(mAudioRingBuffer.Read(outputData, framesToRead));
		if(framesRead != framesToRead)
			os_log_fault(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

		// ========================================
		// 5. If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
#if DEBUG
			os_log_debug(_audioPlayerNodeLog, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);
#endif /* DEBUG */

			const auto framesOfSilence = frameCount - framesRead;
			const auto byteCountToSkip = mAudioRingBuffer.Format().FrameCountToByteSize(framesRead);
			const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(framesOfSilence);
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(outputData->mBuffers[i].mData) + byteCountToSkip), 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize += byteCountToZero;
			}
		}

		// ========================================
		// 6. If there is adequate space in the ring buffer for another chunk signal the decoding queue
		if(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize)
			mDecodingSemaphore.Signal();

		// ========================================
		// Post-rendering actions

		// ========================================
		// 7. There is nothing more to do if no frames were rendered
		if(framesRead == 0)
			return noErr;

		// ========================================
		// 8. Perform bookkeeping to apportion the rendered frames appropriately
		//
		// framesRead contains the number of valid frames that were rendered
		// However, these could have come from any number of decoders depending on buffer sizes
		// So it is necessary to split them up here

		auto framesRemainingToDistribute = framesRead;

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		while(decoderState) {
			const auto decoderFramesRemaining = static_cast<AVAudioFrameCount>(decoderState->mFramesConverted.load() - decoderState->mFramesRendered.load());
			const auto framesFromThisDecoder = std::min(decoderFramesRemaining, framesRemainingToDistribute);

			if(!(decoderState->mFlags.load() & DecoderState::eFlagRenderingStarted)) {
				decoderState->mFlags.fetch_or(DecoderState::eFlagRenderingStarted);

				// Submit the rendering started event
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
				const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

				const RenderingEventHeader header{RenderingEventCommand::eStarted};
				if(mRenderEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber, hostTime))
					dispatch_source_merge_data(mEventProcessingSource, 1);
				else
					os_log_fault(_audioPlayerNodeLog, "Error writing rendering started event");
			}

			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
			framesRemainingToDistribute -= framesFromThisDecoder;

			if((decoderState->mFlags.load() & DecoderState::eFlagDecodingComplete) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
				decoderState->mFlags.fetch_or(DecoderState::eFlagRenderingComplete);

				// Submit the rendering complete event
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
				const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

				const RenderingEventHeader header{RenderingEventCommand::eComplete};
				if(mRenderEventRingBuffer.WriteValues(header, decoderState->mSequenceNumber, hostTime))
					dispatch_source_merge_data(mEventProcessingSource, 1);
				else
					os_log_fault(_audioPlayerNodeLog, "Error writing rendering complete event");
			}

			if(framesRemainingToDistribute == 0)
				break;

			decoderState = GetActiveDecoderStateFollowingSequenceNumber(decoderState->mSequenceNumber);
		}

		// ========================================
		// 9. If there are no active decoders schedule the end of audio notification

		decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState) {
			const uint32_t frameOffset = framesRead;
			const double deltaSeconds = frameOffset / mAudioRingBuffer.Format().mSampleRate;
			const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(deltaSeconds * timestamp.mRateScalar);

			const RenderingEventHeader header{RenderingEventCommand::eEndOfAudio};
			if(mRenderEventRingBuffer.WriteValues(header, hostTime))
				dispatch_source_merge_data(mEventProcessingSource, 1);
			else
				os_log_fault(_audioPlayerNodeLog, "Error writing end of audio event");
		}

		return noErr;
	}

	// MARK: - Event Processing

	void ProcessPendingEvents() noexcept
	{
#if DEBUG
		dispatch_assert_queue(mEventProcessingQueue);
#endif /* DEBUG */

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
	}

	void ProcessEvent(const DecodingEventHeader& header) noexcept
	{
		switch(header.mCommand) {
			case DecodingEventCommand::eStarted:
				if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding started event", decoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingStarted:decoderState->mDecoder];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing decoder sequence number for decoding started event");
				break;

			case DecodingEventCommand::eComplete:
				if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding complete event", decoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingComplete:decoderState->mDecoder];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing decoder sequence number for decoding complete event");
				break;

			case DecodingEventCommand::eCanceled:
				if(uint64_t decoderSequenceNumber; mDecodeEventRingBuffer.ReadValue(decoderSequenceNumber)) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding canceled event", decoderSequenceNumber);
						break;
					}

					const auto decoder = decoderState->mDecoder;
					const auto framesRendered = decoderState->mFramesRendered.load();
					DeleteDecoderStateWithSequenceNumber(decoderSequenceNumber);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:framesRendered:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingCanceled:decoder framesRendered:framesRendered];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing decoder sequence number for decoding canceled event");
				break;

			case DecodingEventCommand::eError:
				if(uint64_t key; mDecodeEventRingBuffer.ReadValue(key)) {
					NSError *error = (__bridge NSError *)dispatch_queue_get_specific(mNode.delegateQueue, reinterpret_cast<void *>(key));
					if(!error) {
						os_log_fault(_audioPlayerNodeLog, "Dispatch queue context data for key %llu missing for decoding error event", key);
						break;
					}

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), nullptr, nullptr);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode encounteredError:error];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing key for decoding error event");
				break;

			default:
				os_log_fault(_audioPlayerNodeLog, "Unknown decode event command: %u", header.mCommand);
				break;
		}
	}

	void ProcessEvent(const RenderingEventHeader& header) noexcept
	{
		switch(header.mCommand) {
			case RenderingEventCommand::eStarted:
				if(uint64_t decoderSequenceNumber, hostTime; mRenderEventRingBuffer.ReadValues(decoderSequenceNumber, hostTime)) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for rendering started event", decoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > hostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will start event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->mDecoder);
#if DEBUG
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->mDecoder);
#endif /* DEBUG */

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode renderingWillStart:decoderState->mDecoder atHostTime:hostTime];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing decoder sequence number or host time for rendering started event");
				break;

			case RenderingEventCommand::eComplete:
				if(uint64_t decoderSequenceNumber, hostTime; mRenderEventRingBuffer.ReadValues(decoderSequenceNumber, hostTime)) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(decoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for rendering complete event", decoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > hostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6, decoderState->mDecoder);
#if DEBUG
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6, decoderState->mDecoder);
#endif /* DEBUG */

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillComplete:atHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode renderingWillComplete:decoderState->mDecoder atHostTime:hostTime];
						});
					}

					DeleteDecoderStateWithSequenceNumber(decoderSequenceNumber);
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing decoder sequence number or host time for rendering complete event");
				break;

			case RenderingEventCommand::eEndOfAudio:
				if(uint64_t hostTime; mRenderEventRingBuffer.ReadValue(hostTime)) {
					const auto now = SFB::GetCurrentHostTime();
					if(now > hostTime)
						os_log_error(_audioPlayerNodeLog, "End of audio event processed %.2f msec late", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - hostTime)) / 1e6);
#if DEBUG
					else
						os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(hostTime - now)) / 1e6);
#endif /* DEBUG */

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:audioWillEndAtHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode audioWillEndAtHostTime:hostTime];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing host time for end of audio event");
				break;

			default:
				os_log_fault(_audioPlayerNodeLog, "Unknown render event command: %u", header.mCommand);
				break;
		}
	}

};

} /* namespace */

#pragma mark -

/// The default ring buffer capacity in frames
constexpr AVAudioFrameCount kDefaultRingBufferFrameCapacity = 16384;

@interface SFBAudioPlayerNode ()
{
@private
	AudioPlayerNode::unique_ptr _impl;
}
@end

@implementation SFBAudioPlayerNode

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioPlayerNodeErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBAudioPlayerNodeErrorCodeInternalError:
					return NSLocalizedString(@"An internal player error occurred.", @"");
				case SFBAudioPlayerNodeErrorCodeFormatNotSupported:
					return NSLocalizedString(@"The format is invalid, unknown, or unsupported.", @"");
			}
		}
		return nil;
	}];
}

- (instancetype)init
{
	return [self initWithSampleRate:44100 channels:2];
}

- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels
{
	return [self initWithFormat:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:sampleRate channels:channels]];
}

- (instancetype)initWithFormat:(AVAudioFormat *)format
{
	return [self initWithFormat:format ringBufferSize:kDefaultRingBufferFrameCapacity];
}

- (instancetype)initWithFormat:(AVAudioFormat *)format ringBufferSize:(uint32_t)ringBufferSize
{
	NSParameterAssert(format != nil);
	NSParameterAssert(format.isStandard);

	std::unique_ptr<AudioPlayerNode> impl;

	try {
		impl = std::make_unique<AudioPlayerNode>(format, ringBufferSize);
	}

	catch(const std::exception& e) {
		os_log_error(_audioPlayerNodeLog, "Unable to create std::unique_ptr<AudioPlayerNode>: %{public}s", e.what());
		return nil;
	}

	if((self = [super initWithFormat:format renderBlock:impl->mRenderBlock])) {
		_impl = std::move(impl);
		_impl->mNode = self;

		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
		if(!attr) {
			os_log_error(_audioPlayerNodeLog, "Unable to create dispatch_queue_attr_t: dispatch_queue_attr_make_with_qos_class() failed");
			return nil;
		}

		_delegateQueue = dispatch_queue_create_with_target("AudioPlayerNode.Delegate", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!_delegateQueue) {
			os_log_error(_audioPlayerNodeLog, "Unable to create dispatch_queue_t: dispatch_queue_create_with_target() failed");
			return nil;
		}
	}

	return self;
}

- (void)dealloc
{
	_impl.reset();
}

#pragma mark - Format Information

- (AVAudioFormat *)renderingFormat
{
	return _impl->RenderingFormat();
}

- (BOOL)supportsFormat:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	return _impl->SupportsFormat(format);
}

#pragma mark - Queue Management

- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _impl->EnqueueDecoder(decoder, true, error);
}

- (BOOL)resetAndEnqueueDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _impl->EnqueueDecoder(decoder, true, error);
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;
	return _impl->EnqueueDecoder(decoder, false, error);
}

- (BOOL)enqueueDecoder:(id <SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	return _impl->EnqueueDecoder(decoder, false, error);
}

- (id <SFBPCMDecoding>)dequeueDecoder
{
	return _impl->DequeueDecoder();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _impl->CurrentDecoder();
}

- (void)cancelCurrentDecoder
{
	_impl->CancelCurrentDecoder();
}

- (void)clearQueue
{
	_impl->ClearQueue();
}

- (BOOL)queueIsEmpty
{
	return _impl->QueueIsEmpty();
}

// AVAudioNode override
- (void)reset
{
	[super reset];
	_impl->Reset();
}

#pragma mark - Playback Control

- (void)play
{
	_impl->Play();
}

- (void)pause
{
	_impl->Pause();
}

- (void)stop
{
	_impl->Stop();
	// Stop() calls Reset() internally so there is no need for [self reset]
	[super reset];
}

- (void)togglePlayPause
{
	_impl->TogglePlayPause();
}

#pragma mark - State

- (BOOL)isPlaying
{
	return _impl->IsPlaying();
}

- (BOOL)isReady
{
	return _impl->IsReady();
}

#pragma mark - Playback Properties

- (SFBAudioPlayerNodePlaybackPosition)playbackPosition
{
	return _impl->PlaybackPosition();
}

- (SFBAudioPlayerNodePlaybackTime)playbackTime
{
	return _impl->PlaybackTime();
}

- (BOOL)getPlaybackPosition:(SFBAudioPlayerNodePlaybackPosition *)playbackPosition andTime:(SFBAudioPlayerNodePlaybackTime *)playbackTime
{
	return _impl->GetPlaybackPositionAndTime(playbackPosition, playbackTime);
}

#pragma mark - Seeking

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	return _impl->SeekForward(secondsToSkip);
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	return _impl->SeekBackward(secondsToSkip);
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	return _impl->SeekToTime(timeInSeconds);
}

- (BOOL)seekToPosition:(double)position
{
	return _impl->SeekToPosition(position);
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	return _impl->SeekToFrame(frame);
}

- (BOOL)supportsSeeking
{
	return _impl->SupportsSeeking();
}

@end
