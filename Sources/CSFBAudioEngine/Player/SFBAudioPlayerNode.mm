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
#endif /* DEBUG */

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

	enum eDecoderStateFlags : unsigned int {
		eCancelDecoding 	= 1u << 0,
		eDecodingStarted 	= 1u << 1,
		eDecodingComplete 	= 1u << 2,
		eRenderingStarted 	= 1u << 3,
		eRenderingComplete 	= 1u << 4,
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
#endif /* DEBUG */

		if(![mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:mDecodeBuffer.frameCapacity error:error])
			return false;

		if(mDecodeBuffer.frameLength == 0) {
			mFlags.fetch_or(eDecodingComplete);
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
			mFlags.fetch_or(eDecodingComplete);

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

uint64_t DecoderState::sSequenceNumber = 0;
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

		if(const auto flags = decoderState->mFlags.load(); flags & DecoderState::eRenderingComplete)
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

		if(const auto flags = decoderState->mFlags.load(); flags & DecoderState::eRenderingComplete)
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
struct AudioPlayerNode
{
	using unique_ptr = std::unique_ptr<AudioPlayerNode>;

	/// The minimum number of frames to write to the ring buffer
	static constexpr AVAudioFrameCount 	kRingBufferChunkSize 	= 2048;

	enum eAudioPlayerNodeFlags : unsigned int {
		eIsPlaying 				= 1u << 0,
		eOutputIsMuted 			= 1u << 1,
		eMuteRequested 			= 1u << 2,
		eRingBufferNeedsReset 	= 1u << 3,
	};

	// MARK: Events

	/// An event command and timestamp
	template <typename T, typename = std::enable_if_t<std::is_same_v<std::underlying_type_t<T>, uint32_t>>>
	struct EventHeader
	{
		/// The event command
		T mCommand;
		/// The event timestamp in host time
		uint64_t mTimestamp = SFB::GetCurrentHostTime();
	};

	// MARK: Decoding Events

	/// Decoding queue events
	enum class eDecodingEventCommand : uint32_t {
		eDecodingStarted 	= 1,
		eDecodingComplete 	= 2,
		eDecodingCanceled 	= 3,
		eDecodingError 		= 4,
	};

	/// A decoding event command and timestamp
	using DecodingEventHeader = EventHeader<eDecodingEventCommand>;

	/// A decoding event
	template <eDecodingEventCommand C>
	struct DecodingEvent
	{
		/// Event-specific data
		struct Payload
		{
			/// The decoder sequence number for the event
			uint64_t mDecoderSequenceNumber;
		};

		/// The event command for this event type
		constexpr static auto sEventCommand = C;

		/// The event command and timestamp
		DecodingEventHeader 	mHeader;
		/// Event-specific data
		Payload 				mPayload;

		/// Constructs a decoding event with the timestamp set to the current host time
		/// - parameter decoderSequenceNumber: The decoder sequence number for the event
		DecodingEvent(uint64_t decoderSequenceNumber) noexcept
		: mHeader{C}, mPayload{decoderSequenceNumber}
		{}
	};

	/// A decoding started event
	using DecodingStartedEvent = DecodingEvent<eDecodingEventCommand::eDecodingStarted>;
	/// A decoding complete event
	using DecodingCompleteEvent = DecodingEvent<eDecodingEventCommand::eDecodingComplete>;
	/// A decoding canceled event
	using DecodingCanceledEvent = DecodingEvent<eDecodingEventCommand::eDecodingCanceled>;

	/// A decoding error event
	struct DecodingErrorEvent
	{
		/// Event-specific data
		struct Payload
		{
			/// A key for an `NSError` object in dispatch queue-specific data
			uint64_t mKey;
		};

		/// The event command for this event type
		constexpr static auto sEventCommand = eDecodingEventCommand::eDecodingError;

		/// The event command and timestamp
		DecodingEventHeader 	mHeader;
		/// Event-specific data
		Payload 				mPayload;

		/// Constructs a decoding error event with the timestamp set to the current host time
		/// - parameter key: The key for a dispatch queue-specific `NSError` object
		DecodingErrorEvent(uint64_t key) noexcept
		: mHeader{sEventCommand}, mPayload{key}
		{}
	};

	// MARK: Render Events

	/// Render block events
	enum class eRenderEventCommand : uint32_t {
		eRenderingStarted 		= 1,
		eRenderingComplete 		= 2,
		eEndOfAudio				= 3,
	};

	/// A rendering event command and timestamp
	using RenderingEventHeader = EventHeader<eRenderEventCommand>;

	/// A rendering event
	template <eRenderEventCommand C>
	struct RenderingEvent
	{
		/// Event-specific data
		struct Payload
		{
			/// The decoder sequence number for the event
			uint64_t mDecoderSequenceNumber;
			/// The rendering host time
			uint64_t mHostTime;
		};

		/// The event command for this event type
		constexpr static auto sEventCommand = C;

		/// The event command and timestamp
		RenderingEventHeader 	mHeader;
		/// Event-specific data
		Payload 				mPayload;

		/// Constructs a rendering event with the timestamp set to the current host time
		/// - parameter decoderSequenceNumber: The decoder sequence number for the event
		/// - parameter hostTime: The rendering host time
		RenderingEvent(uint64_t decoderSequenceNumber, uint64_t hostTime) noexcept
		: mHeader{C}, mPayload{decoderSequenceNumber, hostTime}
		{}
	};

	using RenderingStartedEvent = RenderingEvent<eRenderEventCommand::eRenderingStarted>;
	using RenderingCompleteEvent = RenderingEvent<eRenderEventCommand::eRenderingComplete>;

	/// An end of audio event
	struct EndOfAudioEvent
	{
		/// Event-specific data
		struct Payload
		{
			/// The end of audio host time
			uint64_t mHostTime;
		};

		/// The event command for this event type
		constexpr static auto sEventCommand = eRenderEventCommand::eEndOfAudio;

		/// The event command and timestamp
		RenderingEventHeader 		mHeader;
		/// Event-specific data
		Payload 				mPayload;

		/// Constructs an end of audio event with the timestamp set to the current host time
		/// - parameter hostTime: The host time when audio ends
		EndOfAudioEvent(uint64_t hostTime) noexcept
		: mHeader{sEventCommand}, mPayload{hostTime}
		{}
	};

	/// Weak reference to owning `SFBAudioPlayerNode` instance
	__weak SFBAudioPlayerNode 		*mNode 					= nil;

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

	/// Dispatch group used to track in-progress decoding and delegate messages
	dispatch_group_t 				mDispatchGroup 			= nullptr;

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

		os_log_debug(_audioPlayerNodeLog, "Created <AudioPlayerNode: %p> with rendering format %{public}@", this, SFB::StringDescribingAVAudioFormat(mRenderingFormat));

		// MARK: Rendering
		mRenderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
			// ========================================
			// Pre-rendering actions

			// ========================================
			// 0. Mute output if requested
			if(mFlags.load() & eMuteRequested) {
				mFlags.fetch_or(eOutputIsMuted);
				mFlags.fetch_and(~eMuteRequested);
				dispatch_semaphore_signal(mDecodingSemaphore);
			}

			// ========================================
			// Rendering

			// N.B. The ring buffer must not be read from or written to when eOutputIsMuted is set
			// because the decoding queue could be performing non-thread safe operations

			// ========================================
			// 1. Output silence if the node isn't playing or is muted
			if(const auto flags = mFlags.load(); !(flags & eIsPlaying) || flags & eOutputIsMuted) {
				auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
				SetAudioBufferListToZero(outputData, 0, byteCountToZero);
				*isSilence = YES;
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
				*isSilence = YES;
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

				if(!(decoderState->mFlags.load() & DecoderState::eRenderingStarted)) {
					decoderState->mFlags.fetch_or(DecoderState::eRenderingStarted);

					// Submit the rendering started event
					const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
					const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTime(frameOffset / mAudioRingBuffer.Format().mSampleRate);

					const RenderingStartedEvent event{decoderState->mSequenceNumber, hostTime};
					if(mRenderEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for RenderingStartedEvent");
				}

				decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
				framesRemainingToDistribute -= framesFromThisDecoder;

				if((decoderState->mFlags.load() & DecoderState::eDecodingComplete) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
					decoderState->mFlags.fetch_or(DecoderState::eRenderingComplete);

					// Submit the rendering complete event
					const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
					const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTime(frameOffset / mAudioRingBuffer.Format().mSampleRate);

					const RenderingCompleteEvent event{decoderState->mSequenceNumber, hostTime};
					if(mRenderEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for RenderingCompleteEvent");
				}

				if(framesRemainingToDistribute == 0)
					break;

				decoderState = GetActiveDecoderStateFollowingSequenceNumber(decoderState->mSequenceNumber);
			}

			// ========================================
			// 9. If there are no active decoders schedule the end of audio notification

			decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
			if(!decoderState) {
				const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTime(framesRead / mAudioRingBuffer.Format().mSampleRate);

				const EndOfAudioEvent event{hostTime};
				if(mRenderEventRingBuffer.WriteValue(event))
					dispatch_source_merge_data(mEventProcessor, 1);
				else
					os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for EndOfAudioEvent");
			}

			return noErr;
		};

		// Allocate the audio ring buffer moving audio from the decoder queue to the render block
		if(!mAudioRingBuffer.Allocate(*(mRenderingFormat.streamDescription), ringBufferSize)) {
			os_log_error(_audioPlayerNodeLog, "Unable to create audio ring buffer: SFB::Audio::RingBuffer::Allocate failed");
			throw std::runtime_error("SFB::Audio::RingBuffer::Allocate failed");
		}

		// Allocate the event ring buffers

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

		mDecodingSemaphore = dispatch_semaphore_create(0);
		if(!mDecodingSemaphore) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decoding dispatch semaphore: dispatch_semaphore_create failed");
			throw std::runtime_error("dispatch_semaphore_create failed");
		}

		mDispatchGroup = dispatch_group_create();
		if(!mDispatchGroup) {
			os_log_error(_audioPlayerNodeLog, "Unable to create dispatch group: dispatch_group_create failed");
			throw std::runtime_error("dispatch_group_create failed");
		}

		// MARK: Event Processing

		// Create the dispatch source used for event processing and delegate messaging
		mEventProcessor = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
		if(!mEventProcessor) {
			os_log_error(_audioPlayerNodeLog, "Unable to create event processing dispatch source: dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		dispatch_source_set_event_handler(mEventProcessor, ^{
			auto decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
			auto renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();

			// Process all pending decode and render events in timestamp order
			for(;;) {
				// Nothing left to do
				if(!decodeEventHeader && !renderEventHeader)
					return;
				// Process the decode event
				else if(decodeEventHeader && !renderEventHeader) {
					ProcessDecodeEvent(*decodeEventHeader);
					decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
				}
				// Process the render event
				else if(!decodeEventHeader && renderEventHeader) {
					ProcessRenderEvent(*renderEventHeader);
					renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();
				}
				// The decode event has an earlier timestamp; process it
				else if(decodeEventHeader->mTimestamp < renderEventHeader->mTimestamp) {
					ProcessDecodeEvent(*decodeEventHeader);
					decodeEventHeader = mDecodeEventRingBuffer.ReadValue<DecodingEventHeader>();
				}
				// The render event has an earlier timestamp
				else {
					ProcessRenderEvent(*renderEventHeader);
					renderEventHeader = mRenderEventRingBuffer.ReadValue<RenderingEventHeader>();
				}
			}
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
	}

	~AudioPlayerNode()
	{
		// Cancel any further event processing
		dispatch_source_cancel(mEventProcessor);

		// Cancel decoding
		CancelCurrentDecoder();
		dispatch_semaphore_signal(mDecodingSemaphore);

		const auto timeout = dispatch_group_wait(mDispatchGroup, /*DISPATCH_TIME_FOREVER*/dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2));
		if(timeout)
			os_log_fault(_audioPlayerNodeLog, "<AudioPlayerNode: %p> timeout waiting for dispatch group blocks to complete", this);

		os_log_debug(_audioPlayerNodeLog, "<AudioPlayerNode: %p> destroyed", this);
	}

#pragma mark - Playback Control

	void Play() noexcept
	{
		mFlags.fetch_or(eIsPlaying);
	}

	void Pause() noexcept
	{
		mFlags.fetch_and(~eIsPlaying);
	}

	void Stop() noexcept
	{
		mFlags.fetch_and(~eIsPlaying);
		Reset();
	}

	void TogglePlayPause() noexcept
	{
		mFlags.fetch_xor(eIsPlaying);
	}

#pragma mark - Playback State

	bool IsPlaying() const noexcept
	{
		return (mFlags.load() & eIsPlaying) != 0;
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
#endif /* DEBUG */

		// Gapless playback requires the same number of channels at the same sample rate with the same channel layout
		auto channelLayoutsAreEquivalent = AVAudioChannelLayoutsAreEquivalent(format.channelLayout, mRenderingFormat.channelLayout);
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
			ClearQueue();
			if(auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(); decoderState)
				decoderState->mFlags.fetch_or(DecoderState::eCancelDecoding);
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
			decoderState->mFlags.fetch_or(DecoderState::eCancelDecoding);
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
					const DecodingErrorEvent event{mDispatchKeyCounter.fetch_add(1)};

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingErrorEvent");

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
					const DecodingErrorEvent event{mDispatchKeyCounter.fetch_add(1)};

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

					if(mDecodeEventRingBuffer.WriteValue(event))
						dispatch_source_merge_data(mEventProcessor, 1);
					else
						os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingErrorEvent");

					return;
				}

				// Process the decoder until canceled or complete
				for(;;) {
					// If a seek is pending request a ring buffer reset
					if(decoderState->HasPendingSeek())
						mFlags.fetch_or(eRingBufferNeedsReset);

					// Reset the ring buffer if required, to prevent audible artifacts
					if(mFlags.load() & eRingBufferNeedsReset) {
						mFlags.fetch_and(~eRingBufferNeedsReset);

						// Ensure output is muted before performing operations on the ring buffer that aren't thread-safe
						if(!(mFlags.load() & eOutputIsMuted)) {
							if(mNode.engine.isRunning) {
								mFlags.fetch_or(eMuteRequested);

								// The render block will clear eMuteRequested and set eOutputIsMuted
								while(!(mFlags.load() & eOutputIsMuted))
									dispatch_semaphore_wait(mDecodingSemaphore, DISPATCH_TIME_FOREVER);
							}
							else
								mFlags.fetch_or(eOutputIsMuted);
						}

						// Perform seek if one is pending
						if(decoderState->HasPendingSeek())
							decoderState->PerformPendingSeek();

						// Reset() is not thread-safe but the render block is outputting silence
						mAudioRingBuffer.Reset();

						// Clear the mute flag
						mFlags.fetch_and(~eOutputIsMuted);
					}

					if(decoderState->mFlags.load() & DecoderState::eCancelDecoding) {
						os_log_debug(_audioPlayerNodeLog, "Canceling decoding for %{public}@", decoderState->mDecoder);

						mFlags.fetch_or(eRingBufferNeedsReset);

						// Submit the decoding canceled event
						const DecodingCanceledEvent event{decoderState->mSequenceNumber};
						if(mDecodeEventRingBuffer.WriteValue(event))
							dispatch_source_merge_data(mEventProcessor, 1);
						else
							os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingCanceledEvent");

						return;
					}

					// Decode and write chunks to the ring buffer
					while(mAudioRingBuffer.FramesAvailableToWrite() >= kRingBufferChunkSize) {
						if(!(decoderState->mFlags.load() & DecoderState::eDecodingStarted)) {
							os_log_debug(_audioPlayerNodeLog, "Decoding started for %{public}@", decoderState->mDecoder);

							decoderState->mFlags.fetch_or(DecoderState::eDecodingStarted);

							// Submit the decoding started event
							const DecodingStartedEvent event{decoderState->mSequenceNumber};
							if(mDecodeEventRingBuffer.WriteValue(event))
								dispatch_source_merge_data(mEventProcessor, 1);
							else
								os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingStartedEvent");
						}

						// Decode audio into the buffer, converting to the rendering format in the process
						if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
							os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);

							if(error) {
								// Submit the error event
								const DecodingErrorEvent event{mDispatchKeyCounter.fetch_add(1)};

								dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(event.mPayload.mKey), (__bridge_retained void *)error, &release_nserror_f);

								if(mDecodeEventRingBuffer.WriteValue(event))
									dispatch_source_merge_data(mEventProcessor, 1);
								else
									os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingErrorEvent");
							}
						}

						// Write the decoded audio to the ring buffer for rendering
						const auto framesWritten = mAudioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
						if(framesWritten != buffer.frameLength)
							os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Write() failed");

						if(decoderState->mFlags.load() & DecoderState::eDecodingComplete) {
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

							// Submit the decoding complete event
							const DecodingCompleteEvent event{decoderState->mSequenceNumber};
							if(mDecodeEventRingBuffer.WriteValue(event))
								dispatch_source_merge_data(mEventProcessor, 1);
							else
								os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::WriteValue failed for DecodingCompleteEvent");

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

	// MARK: - Event Processing

	void ProcessDecodeEvent(const DecodingEventHeader& header) noexcept
	{
		switch(header.mCommand) {
			case DecodingStartedEvent::sEventCommand:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecodingStartedEvent::Payload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for DecodingStartedEvent", eventPayload->mDecoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node decodingStarted:decoderState->mDecoder];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for DecodingStartedEvent");
				break;

			case DecodingCompleteEvent::sEventCommand:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecodingCompleteEvent::Payload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for DecodingCompleteEvent", eventPayload->mDecoderSequenceNumber);
						break;
					}

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node decodingComplete:decoderState->mDecoder];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for DecodingCompleteEvent");
				break;

			case DecodingCanceledEvent::sEventCommand:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecodingCanceledEvent::Payload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for DecodingCanceledEvent", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto decoder = decoderState->mDecoder;
					const auto partiallyRendered = (decoderState->mFlags & DecoderState::eRenderingStarted) == DecoderState::eRenderingStarted;
					DeleteDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:partiallyRendered:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node decodingCanceled:decoder partiallyRendered:partiallyRendered];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for DecodingCanceledEvent");
				break;

			case DecodingErrorEvent::sEventCommand:
				if(const auto eventPayload = mDecodeEventRingBuffer.ReadValue<DecodingErrorEvent::Payload>(); eventPayload) {
					NSError *error = (__bridge NSError *)dispatch_queue_get_specific(mNode.delegateQueue, reinterpret_cast<void *>(eventPayload->mKey));
					if(!error) {
						os_log_fault(_audioPlayerNodeLog, "Dispatch value for key %llu missing for DecodingErrorEvent", eventPayload->mKey);
						break;
					}

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(eventPayload->mKey), nullptr, nullptr);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node encounteredError:error];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for DecodingErrorEvent");
				break;

			default:
				os_log_fault(_audioPlayerNodeLog, "Unknown decode event command: %u", header.mCommand);
				break;
		}
	}

	void ProcessRenderEvent(const RenderingEventHeader& header) noexcept
	{
		switch(header.mCommand) {
			case RenderingStartedEvent::sEventCommand:
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<RenderingStartedEvent::Payload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for RenderingStartedEvent", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will start event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6, decoderState->mDecoder);
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6, decoderState->mDecoder);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node renderingWillStart:decoderState->mDecoder atHostTime:eventPayload->mHostTime];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for RenderingStartedEvent");
				break;

			case RenderingCompleteEvent::sEventCommand:
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<RenderingCompleteEvent::Payload>(); eventPayload) {
					const auto decoderState = GetDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
					if(!decoderState) {
						os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for RenderingCompleteEvent", eventPayload->mDecoderSequenceNumber);
						break;
					}

					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "Rendering will complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6, decoderState->mDecoder);
					else
						os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6, decoderState->mDecoder);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillComplete:atHostTime:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node renderingWillComplete:decoderState->mDecoder atHostTime:eventPayload->mHostTime];
							dispatch_group_leave(mDispatchGroup);
						});
					}

					DeleteDecoderStateWithSequenceNumber(eventPayload->mDecoderSequenceNumber);
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for RenderingCompleteEvent");
				break;

			case EndOfAudioEvent::sEventCommand:
				if(const auto eventPayload = mRenderEventRingBuffer.ReadValue<EndOfAudioEvent::Payload>(); eventPayload) {
					const auto now = SFB::GetCurrentHostTime();
					if(now > eventPayload->mHostTime)
						os_log_error(_audioPlayerNodeLog, "End of audio event processed %.2f msec late", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(now - eventPayload->mHostTime)) / 1e6);
					else
						os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", static_cast<double>(SFB::ConvertHostTimeToNanoseconds(eventPayload->mHostTime - now)) / 1e6);

					if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:audioWillEndAtHostTime:)]) {
						auto node = mNode;
						dispatch_group_enter(mDispatchGroup);
						dispatch_async_and_wait(node.delegateQueue, ^{
							[node.delegate audioPlayerNode:node audioWillEndAtHostTime:eventPayload->mHostTime];
							dispatch_group_leave(mDispatchGroup);
						});
					}
				}
				else
					os_log_fault(_audioPlayerNodeLog, "Missing data for EndOfAudioEvent");
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
