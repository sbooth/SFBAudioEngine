//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
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

#pragma mark AudioBufferList Utilities

/// Zeroes a range of bytes in `bufferList`
/// - attention: `bufferList` must contain non-interleaved audio data
/// - parameter bufferList: The destination audio buffer list
/// - parameter byteOffset: The byte offset in `bufferList` to begin writing
/// - parameter byteCount: The maximum number of bytes per non-interleaved buffer to write
void SetAudioBufferListToZero(AudioBufferList * const _Nonnull bufferList, uint32_t byteOffset, uint32_t byteCount) noexcept
{
#if DEBUG
	assert(bufferList != nullptr);
#endif // DEBUG

	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		if(byteOffset > bufferList->mBuffers[i].mDataByteSize)
			continue;
		auto buffer = reinterpret_cast<uintptr_t>(bufferList->mBuffers[i].mData) + byteOffset;
		std::memset(reinterpret_cast<void *>(buffer), 0, std::min(byteCount, bufferList->mBuffers[i].mDataByteSize - byteOffset));
	}
}

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
#endif // DEBUG

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

		if(auto framePosition = decoder.framePosition; framePosition != 0) {
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
#endif // DEBUG

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
using DecoderQueue = std::queue<id <SFBPCMDecoding>>;

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

/// An event consisting of a header and payload
template <typename T, typename P, typename = std::enable_if_t<std::is_trivially_copyable_v<P> && std::is_default_constructible_v<P>>>
struct Event {
	/// The event header
	EventHeader<T> mHeader;
	/// Event-specific data
	P mPayload;

	/// Constructs an empty event
	Event() noexcept(std::is_nothrow_default_constructible_v<P>) = default;

	/// Constructs an event
	/// - parameter command: The command for the event
	/// - parameter a: The payload for the event
	template <typename... A>
	Event(T command, A&&... a) noexcept(std::is_nothrow_constructible_v<P, A...>)
	: mHeader{command}, mPayload{std::forward<A>(a)...}
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

/// A decoding event
template <typename P>
using DecodingEvent = Event<DecodingEventCommand, P>;

#pragma mark Rendering Events

/// Render block events
enum class RenderingEventCommand : uint32_t {
	eStarted 		= 1,
	eComplete 		= 2,
	eEndOfAudio		= 3,
};

/// A rendering event command and identification number
using RenderingEventHeader = EventHeader<RenderingEventCommand>;

/// A rendering event
template <typename P>
using RenderingEvent = Event<RenderingEventCommand, P>;

#pragma mark Event Payloads

/// An event payload consisting of a decoder sequence number
struct DecoderSequenceNumberPayload {
	/// The decoder sequence number for the event
	uint64_t mDecoderSequenceNumber;
};

/// An event payload consisting of an event key in dispatch queue-specific data
struct DispatchKeyPayload {
	/// A key for an object in dispatch queue-specific data
	uint64_t mKey;
};

/// An event payload consisting of a decoder sequence number and host time
struct DecoderSequenceNumberAndHostTimePayload {
	/// The decoder sequence number for the event
	uint64_t mDecoderSequenceNumber;
	/// The host time for the event
	uint64_t mHostTime;
};

/// An event payload consisting of a host time
struct HostTimePayload {
	/// The host time for the event
	uint64_t mHostTime;
};

#pragma mark - AudioPlayerNode

void event_processor_finalizer_f(void *context)
{
	if(auto decoders = static_cast<DecoderStateArray *>(context); decoders) {
		for(auto& atomic_ptr : *decoders)
			delete atomic_ptr.exchange(nullptr);
		delete decoders;
	}
}

void release_nserror_f(void *context)
{
	(void)(__bridge_transfer NSError *)context;
}

/// SFBAudioPlayerNode implementation
struct AudioPlayerNode {
	using unique_ptr = std::unique_ptr<AudioPlayerNode>;

	/// The minimum number of frames to write to the ring buffer
	static constexpr AVAudioFrameCount 	kRingBufferChunkSize 	= 2048;

	enum AudioPlayerNodeFlags : unsigned int {
		eFlagIsPlaying 				= 1u << 0,
		eFlagOutputIsMuted 			= 1u << 1,
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
	dispatch_semaphore_t			mDecodingSemaphore 		= nullptr;

	/// Ring buffer used to communicate events from the decoding queue
	SFB::RingBuffer					mDecodeEventRingBuffer;
	/// Ring buffer used to communicate events from the render block
	SFB::RingBuffer					mRenderEventRingBuffer;

	/// Dispatch source processing events from `mDecodeEventRingBuffer` and `mRenderEventRingBuffer`
	dispatch_source_t				mEventProcessor 		= nullptr;

	/// Dispatch group used to track decoding and event processing
	dispatch_group_t 				mDispatchGroup			= nullptr;

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
#endif // DEBUG

		os_log_debug(_audioPlayerNodeLog, "Created <AudioPlayerNode: %p> with rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(mRenderingFormat));

		mDispatchGroup = dispatch_group_create();
		if(!mDispatchGroup) {
			os_log_error(_audioPlayerNodeLog, "Unable to create dispatch group: dispatch_group_create failed");
			throw std::runtime_error("dispatch_group_create failed");
		}

		// ========================================
		// Decoding Setup

		mDecodingSemaphore = dispatch_semaphore_create(0);
		if(!mDecodingSemaphore) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decoding dispatch semaphore: dispatch_semaphore_create failed");
			throw std::runtime_error("dispatch_semaphore_create failed");
		}

		// Create the dispatch queue used for decoding
		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
		if(!attr) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_attr_make_with_qos_class failed");
			throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
		}

		mDecodingQueue = dispatch_queue_create_with_target("org.sbooth.AudioEngine.AudioPlayerNode.Decoder", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!mDecodingQueue) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decoding dispatch queue: dispatch_queue_create_with_target failed");
			throw std::runtime_error("dispatch_queue_create_with_target failed");
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

		// The decode event ring buffer is written to by the decoding queue and read from by the event processor
		if(!mDecodeEventRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decode event ring buffer: SFB::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate failed");
		}

		// The render event ring buffer is written to by the render block and read from by the event processor
		if(!mRenderEventRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create render event ring buffer: SFB::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate failed");
		}

		// Create the dispatch source used for event processing and delegate messaging
		mEventProcessor = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
		if(!mEventProcessor) {
			os_log_error(_audioPlayerNodeLog, "Unable to create event processing dispatch source: dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		dispatch_source_set_event_handler(mEventProcessor, ^{
			if(dispatch_source_testcancel(mEventProcessor))
				return;

			dispatch_group_enter(mDispatchGroup);
			ProcessPendingEvents();
			dispatch_group_leave(mDispatchGroup);
		});

		// Allocate and initialize the decoder state array
		mActiveDecoders = new DecoderStateArray;
		for(auto& atomic_ptr : *mActiveDecoders)
			atomic_ptr.store(nullptr);

		// The event processor takes ownership of `mActiveDecoders` and the finalizer is responsible
		// for deleting any allocated decoder state it contains as well as the array itself
		dispatch_set_context(mEventProcessor, mActiveDecoders);
		dispatch_set_finalizer_f(mEventProcessor, &event_processor_finalizer_f);

		// Start processing events
		dispatch_activate(mEventProcessor);
	}

	~AudioPlayerNode()
	{
		Stop();

		// Wait for `DequeueAndProcessDecoder()` to return and for and event processing to complete
		/*const auto timeout =*/ dispatch_group_wait(mDispatchGroup, DISPATCH_TIME_FOREVER);

		// Cancel any further event processing
		dispatch_source_cancel(mEventProcessor);

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
		dispatch_semaphore_signal(mDecodingSemaphore);

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
#endif // DEBUG

		// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
		auto channelLayoutsAreEquivalent = AVAudioChannelLayoutsAreEquivalent(format.channelLayout, mRenderingFormat.channelLayout);
		return format.channelCount == mRenderingFormat.channelCount && format.sampleRate == mRenderingFormat.sampleRate && channelLayoutsAreEquivalent;
	}

#pragma mark - Queue Management

	bool EnqueueDecoder(id <SFBPCMDecoding> _Nonnull decoder, bool reset, NSError **error) noexcept
	{
#if DEBUG
		assert(decoder != nil);
#endif // DEBUG

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
			ClearQueue();
			if(auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(); decoderState)
				decoderState->mFlags.fetch_or(DecoderState::eFlagCancelDecoding);
		}

		os_log_info(_audioPlayerNodeLog, "Enqueuing %{public}@ on <AudioPlayerNode: %p>", decoder, this);

		{
			std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
			mQueuedDecoders.push(decoder);
		}

		DequeueAndProcessDecoder();

		return true;
	}

	id <SFBPCMDecoding> _Nullable DequeueDecoder() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		id <SFBPCMDecoding> decoder = nil;
		if(!mQueuedDecoders.empty()) {
			decoder = mQueuedDecoders.front();
			mQueuedDecoders.pop();
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
			decoderState->mFlags.fetch_or(DecoderState::eFlagCancelDecoding);
			dispatch_semaphore_signal(mDecodingSemaphore);
		}
	}

	void ClearQueue() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		while(!mQueuedDecoders.empty())
			mQueuedDecoders.pop();
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

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number that has not completed rendering and has not been marked for removal
	DecoderState * _Nullable GetActiveDecoderStateWithSmallestSequenceNumber() const noexcept
	{
		return ::GetActiveDecoderStateWithSmallestSequenceNumber(*mActiveDecoders);
	}

	/// Returns the decoder state in `mActiveDecoders` with the smallest sequence number greater than `sequenceNumber` that has not completed rendering and has not been marked for removal
	DecoderState * _Nullable GetActiveDecoderStateFollowingSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetActiveDecoderStateFollowingSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

	/// Returns the decoder state in `mActiveDecoders` with sequence number equal to `sequenceNumber` that has not been marked for removal
	DecoderState * _Nullable GetDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetDecoderStateWithSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

	void DeleteDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) noexcept
	{
		::DeleteDecoderStateWithSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

#pragma mark - Decoding

	void DequeueAndProcessDecoder() noexcept
	{
		dispatch_group_async(mDispatchGroup, mDecodingQueue, ^{
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
					const DecodingEvent<DispatchKeyPayload> event{DecodingEventCommand::eError, mDispatchKeyCounter.fetch_add(1)};

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding error event");

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
						// event processor. The event processor assigns nullptr to slots holding existing non-null
						// values marked for removal while this code assigns non-null values to slots
						// holding nullptr.
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

				os_log_debug(_audioPlayerNodeLog, "Dequeued %{public}@, processing format %{public}@", decoderState->mDecoder, SFB::StringDescribingAVAudioFormat(decoderState->mDecoder.processingFormat));

				// Allocate the buffer that is the intermediary between the decoder state and the ring buffer
				AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mRenderingFormat frameCapacity:kRingBufferChunkSize];
				if(!buffer) {
					os_log_error(_audioPlayerNodeLog, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", SFB::StringDescribingAVAudioFormat(mRenderingFormat), kRingBufferChunkSize);

					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain
														 code:SFBAudioPlayerNodeErrorCodeInternalError
													 userInfo:@{ NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"An internal error occurred in AudioPlayerNode.", @""),
																 NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Error creating AVAudioPCMBuffer", @""),
															  }];

					// Submit the error event
					const DecodingEvent<DispatchKeyPayload> event{DecodingEventCommand::eError, mDispatchKeyCounter.fetch_add(1)};

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding error event");

					return;
				}

				// Process the decoder until canceled or complete
				for(;;) {
					// If a seek is pending request a ring buffer reset
					if(decoderState->HasPendingSeek())
						mFlags.fetch_or(eFlagRingBufferNeedsReset);

					// Reset the ring buffer if required, to prevent audible artifacts
					if(mFlags.load() & eFlagRingBufferNeedsReset) {
						mFlags.fetch_and(~eFlagRingBufferNeedsReset);

						// Ensure output is muted before performing operations on the ring buffer that aren't thread-safe
						if(!(mFlags.load() & eFlagOutputIsMuted)) {
							if(mNode.engine.isRunning) {
								mFlags.fetch_or(eFlagMuteRequested);

								// The render block will clear eMuteRequested and set eOutputIsMuted
								while(!(mFlags.load() & eFlagOutputIsMuted))
									dispatch_semaphore_wait(mDecodingSemaphore, DISPATCH_TIME_FOREVER);
							}
							else
								mFlags.fetch_or(eFlagOutputIsMuted);
						}

						// Perform seek if one is pending
						if(decoderState->HasPendingSeek())
							decoderState->PerformPendingSeek();

						// Reset() is not thread-safe but the render block is outputting silence
						mAudioRingBuffer.Reset();

						// Clear the mute flag
						mFlags.fetch_and(~eFlagOutputIsMuted);
					}

					if(decoderState->mFlags.load() & DecoderState::eFlagCancelDecoding) {
						os_log_debug(_audioPlayerNodeLog, "Canceling decoding for %{public}@", decoderState->mDecoder);

						mFlags.fetch_or(eFlagRingBufferNeedsReset);

						// Submit the decoding canceled event
						const DecodingEvent<DecoderSequenceNumberPayload> event{DecodingEventCommand::eCanceled, decoderState->mSequenceNumber};
						if(mDecodeEventRingBuffer.WriteValue(event))
							dispatch_source_merge_data(mEventProcessor, 1);
						else
							os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding canceled event");

						return;
					}

					// Decode and write chunks to the ring buffer
					while(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize) {
						if(!(decoderState->mFlags.load() & DecoderState::eFlagDecodingStarted)) {
							os_log_debug(_audioPlayerNodeLog, "Decoding started for %{public}@", decoderState->mDecoder);

							decoderState->mFlags.fetch_or(DecoderState::eFlagDecodingStarted);

							// Submit the decoding started event
							const DecodingEvent<DecoderSequenceNumberPayload> event{DecodingEventCommand::eStarted, decoderState->mSequenceNumber};
							if(mDecodeEventRingBuffer.WriteValue(event))
								dispatch_source_merge_data(mEventProcessor, 1);
							else
								os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding started event");
						}

						// Decode audio into the buffer, converting to the rendering format in the process
						if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
							os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);

							if(error) {
								// Submit the error event
								const DecodingEvent<DispatchKeyPayload> event{DecodingEventCommand::eError, mDispatchKeyCounter.fetch_add(1)};

								dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

								if(mDecodeEventRingBuffer.WriteValue(event))
									dispatch_source_merge_data(mEventProcessor, 1);
								else
									os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding error event");
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
							const DecodingEvent<DecoderSequenceNumberPayload> event{DecodingEventCommand::eComplete, decoderState->mSequenceNumber};
							if(mDecodeEventRingBuffer.WriteValue(event))
								dispatch_source_merge_data(mEventProcessor, 1);
							else
								os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for decoding complete event");

							os_log_debug(_audioPlayerNodeLog, "Decoding complete for %{public}@", decoderState->mDecoder);

							return;
						}
					}

					// Wait for additional space in the ring buffer or for another event signal
					dispatch_semaphore_wait(mDecodingSemaphore, DISPATCH_TIME_FOREVER);
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
		// 0. Mute output if requested
		if(mFlags.load() & eFlagMuteRequested) {
			mFlags.fetch_or(eFlagOutputIsMuted);
			mFlags.fetch_and(~eFlagMuteRequested);
			dispatch_semaphore_signal(mDecodingSemaphore);
		}

		// ========================================
		// Rendering

		// N.B. The ring buffer must not be read from or written to when eOutputIsMuted is set
		// because the decoding queue could be performing non-thread safe operations

		// ========================================
		// 1. Output silence if the node isn't playing or is muted
		if(const auto flags = mFlags.load(); !(flags & eFlagIsPlaying) || flags & eFlagOutputIsMuted) {
			auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
			SetAudioBufferListToZero(outputData, 0, byteCountToZero);
			isSilence = YES;
			return noErr;
		}

		// ========================================
		// 2. Determine how many audio frames are available to read in the ring buffer
		const auto framesAvailableToRead = static_cast<AVAudioFrameCount>(mAudioRingBuffer.FramesAvailableToRead());

		// ========================================
		// 3. Output silence if the ring buffer is empty
		if(framesAvailableToRead == 0) {
			auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
			SetAudioBufferListToZero(outputData, 0, byteCountToZero);
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
#endif // DEBUG

			const auto framesOfSilence = frameCount - framesRead;
			const auto byteCountToSkip = mAudioRingBuffer.Format().FrameCountToByteSize(framesRead);
			const auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(framesOfSilence);
			SetAudioBufferListToZero(outputData, byteCountToSkip, byteCountToZero);
		}

		// ========================================
		// 6. If there is adequate space in the ring buffer for another chunk signal the decoding queue
		if(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize)
			dispatch_semaphore_signal(mDecodingSemaphore);

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
				const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(frameOffset / mAudioRingBuffer.Format().mSampleRate);

				const RenderingEvent<DecoderSequenceNumberAndHostTimePayload> event{RenderingEventCommand::eStarted, decoderState->mSequenceNumber, hostTime};
				if(mRenderEventRingBuffer.WriteValue(event))
					dispatch_source_merge_data(mEventProcessor, 1);
				else
					os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for rendering started event");
			}

			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
			framesRemainingToDistribute -= framesFromThisDecoder;

			if((decoderState->mFlags.load() & DecoderState::eFlagDecodingComplete) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
				decoderState->mFlags.fetch_or(DecoderState::eFlagRenderingComplete);

				// Submit the rendering complete event
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(frameOffset / mAudioRingBuffer.Format().mSampleRate);

				const RenderingEvent<DecoderSequenceNumberAndHostTimePayload> event{RenderingEventCommand::eComplete, decoderState->mSequenceNumber, hostTime};
				if(mRenderEventRingBuffer.WriteValue(event))
					dispatch_source_merge_data(mEventProcessor, 1);
				else
					os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for rendering complete event");
			}

			if(framesRemainingToDistribute == 0)
				break;

			decoderState = GetActiveDecoderStateFollowingSequenceNumber(decoderState->mSequenceNumber);
		}

		// ========================================
		// 9. If there are no active decoders schedule the end of audio notification

		decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState) {
			const uint64_t hostTime = timestamp.mHostTime + SFB::ConvertSecondsToHostTime(framesRead / mAudioRingBuffer.Format().mSampleRate);

			const RenderingEvent<HostTimePayload> event{RenderingEventCommand::eEndOfAudio, hostTime};
			if(mRenderEventRingBuffer.WriteValue(event))
				dispatch_source_merge_data(mEventProcessor, 1);
			else
				os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for end of audio event");
		}

		return noErr;
	}

	// MARK: - Event Processing

	void ProcessPendingEvents() noexcept
	{
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
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecoderSequenceNumberPayload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding started event", eventPayload->mDecoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingStarted:decoderState->mDecoder];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for decoding started event");
				break;

			case DecodingEventCommand::eComplete:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecoderSequenceNumberPayload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding complete event", eventPayload->mDecoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingComplete:decoderState->mDecoder];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for decoding complete event");
				break;

			case DecodingEventCommand::eCanceled:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecoderSequenceNumberPayload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for decoding canceled event", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto decoder = decoderState->mDecoder;
					const auto partiallyRendered = (decoderState->mFlags & DecoderState::eFlagRenderingStarted) == DecoderState::eFlagRenderingStarted;
					DeleteDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:partiallyRendered:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode decodingCanceled:decoder partiallyRendered:partiallyRendered];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for decoding canceled event");
				break;

			case DecodingEventCommand::eError:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DispatchKeyPayload>(); eventPayload) {
					NSError *error = (__bridge NSError *)dispatch_queue_get_specific(mNode.delegateQueue, reinterpret_cast<void *>(eventPayload->mKey));
					if(!error) {
						os_log_fault(_audioPlayerNodeLog, "Dispatch value for key %llu missing for decoding error event", eventPayload->mKey);
						break;
					}

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(eventPayload->mKey), nullptr, nullptr);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode encounteredError:error];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for decoding error event");
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
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<DecoderSequenceNumberAndHostTimePayload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for rendering started event", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will start event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6, decoderState->mDecoder);
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6, decoderState->mDecoder);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode renderingWillStart:decoderState->mDecoder atHostTime:eventPayload->mHostTime];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for rendering started event");
				break;

			case RenderingEventCommand::eComplete:
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<DecoderSequenceNumberAndHostTimePayload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for rendering complete event", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6, decoderState->mDecoder);
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6, decoderState->mDecoder);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillComplete:atHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode renderingWillComplete:decoderState->mDecoder atHostTime:eventPayload->mHostTime];
						});
					}

					DeleteDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for rendering complete event");
				break;

			case RenderingEventCommand::eEndOfAudio:
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<HostTimePayload>(); eventPayload) {
					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "End of audio event processed %.2f msec late", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6);
					else
						os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:audioWillEndAtHostTime:)]) {
						dispatch_async_and_wait(mNode.delegateQueue, ^{
							[mNode.delegate audioPlayerNode:mNode audioWillEndAtHostTime:eventPayload->mHostTime];
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for end of audio event");
				break;

			default:
				os_log_fault(_audioPlayerNodeLog, "Unknown render event command: %u", header.mCommand);
				break;
		}
	}

};

} // namespace

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
		if(userInfoKey == NSLocalizedDescriptionKey) {
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

		_delegateQueue = dispatch_queue_create_with_target("org.sbooth.AudioEngine.AudioPlayerNode.DelegateQueue", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
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
	return _impl->SupportsFormat(format);
}

#pragma mark - Queue Management

- (BOOL)resetAndEnqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self resetAndEnqueueDecoder:decoder error:error];
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

	return [self enqueueDecoder:decoder error:error];
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
