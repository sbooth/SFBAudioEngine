//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
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

#import <mach/mach_time.h>
#import <os/log.h>

#import "SFBAudioPlayerNode.h"

#import "SFBAudioRingBuffer.hpp"
#import "SFBRingBuffer.hpp"
#import "SFBUnfairLock.hpp"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "SFBTimeUtilities.hpp"

const NSTimeInterval SFBUnknownTime = -1;
NSErrorDomain const SFBAudioPlayerNodeErrorDomain = @"org.sbooth.AudioEngine.AudioPlayerNode";

namespace {

#pragma mark - Shared State

os_log_t _audioPlayerNodeLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

#pragma mark - Decoder State

/// State for tracking/syncing decoding progress
struct DecoderState {
	using atomic_ptr = std::atomic<DecoderState *>;
	static_assert(atomic_ptr::is_always_lock_free, "Lock-free std::atomic<DecoderState *> required");

	static const AVAudioFrameCount 	kDefaultFrameCapacity 	= 1024;
	static const int64_t			kInvalidFramePosition 	= -1;

	enum eDecoderStateFlags : unsigned int {
		eCancelDecoding 	= 1u << 0,
		eDecodingStarted 	= 1u << 1,
		eDecodingComplete 	= 1u << 2,
		eRenderingStarted 	= 1u << 3,
		eRenderingComplete 	= 1u << 4,
		eMarkedForRemoval 	= 1u << 5,
	};

	/// Monotonically increasing instance counter
	const uint64_t			mSequenceNumber 	= sSequenceNumber++;

	/// Decoder state flags
	std::atomic_uint 		mFlags 				= 0;
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

	/// Decodes audio from the source representation to PCM
	id <SFBPCMDecoding> 	mDecoder 			= nil;
	/// Converts audio from the decoder's processing format to another PCM variant at the same sample rate
	AVAudioConverter 		*mConverter 		= nil;
	/// Buffer used internally for buffering during conversion
	AVAudioPCMBuffer 		*mDecodeBuffer 		= nil;

	/// Next sequence number to use
	static uint64_t			sSequenceNumber;

	DecoderState(id <SFBPCMDecoding> decoder, AVAudioFormat *format, AVAudioFrameCount frameCapacity = kDefaultFrameCapacity)
	: mFrameLength(decoder.frameLength), mDecoder(decoder)
	{
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

		if(AVAudioFramePosition framePosition = decoder.framePosition; framePosition != 0) {
			mFramesDecoded.store(framePosition);
			mFramesConverted.store(framePosition);
			mFramesRendered.store(framePosition);
		}
	}

	inline AVAudioFramePosition FramePosition() const noexcept
	{
		int64_t seek = mFrameToSeek.load();
		return seek == kInvalidFramePosition ? mFramesRendered.load() : seek;
	}

	inline AVAudioFramePosition FrameLength() const noexcept
	{
		return mFrameLength.load();
	}

	bool DecodeAudio(AVAudioPCMBuffer *buffer, NSError **error = nullptr) noexcept
	{
#if DEBUG
		assert(buffer.frameCapacity == mDecodeBuffer.frameCapacity);
#endif

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

		return true;
	}

	/// Seeks to the frame specified by \c mFrameToSeek
	bool PerformSeek() noexcept
	{
		AVAudioFramePosition seekOffset = mFrameToSeek.load();

		os_log_debug(_audioPlayerNodeLog, "Seeking to frame %lld in %{public}@ ", seekOffset, mDecoder);

		if([mDecoder seekToFrame:seekOffset error:nil])
			// Reset the converter to flush any buffers
			[mConverter reset];
		else
			os_log_debug(_audioPlayerNodeLog, "Error seeking to frame %lld", seekOffset);

		AVAudioFramePosition newFrame = mDecoder.framePosition;
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

const size_t kDecoderStateArraySize = 8;
using DecoderStateArray = std::array<DecoderState::atomic_ptr, kDecoderStateArraySize>;

/// Returns the element in \c decoders with the smallest sequence number that has not completed rendering and has not been marked for removal
DecoderState * GetActiveDecoderStateWithSmallestSequenceNumber(const DecoderStateArray& decoders) noexcept
{
	DecoderState *result = nullptr;
	for(auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(auto flags = decoderState->mFlags.load(); flags & DecoderState::eMarkedForRemoval || flags & DecoderState::eRenderingComplete)
			continue;

		if(!result)
			result = decoderState;
		else if(decoderState->mSequenceNumber < result->mSequenceNumber)
			result = decoderState;
	}

	return result;
}

/// Returns the element in \c decoders with the smallest sequence number greater than \c sequenceNumber that has not completed rendering and has not been marked for removal
DecoderState * GetActiveDecoderStateFollowingSequenceNumber(const DecoderStateArray& decoders, const uint64_t& sequenceNumber) noexcept
{
	DecoderState *result = nullptr;
	for(auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(auto flags = decoderState->mFlags.load(); flags & DecoderState::eMarkedForRemoval || flags & DecoderState::eRenderingComplete)
			continue;

		if(!result && decoderState->mSequenceNumber > sequenceNumber)
			result = decoderState;
		else if(result && decoderState->mSequenceNumber > sequenceNumber && decoderState->mSequenceNumber < result->mSequenceNumber)
			result = decoderState;
	}

	return result;
}

/// Returns the element in \c decoders with sequence number equal to \c sequenceNumber that has not been marked for removal
DecoderState * GetDecoderStateWithSequenceNumber(const DecoderStateArray& decoders, const uint64_t& sequenceNumber) noexcept
{
	for(auto& atomic_ptr : decoders) {
		auto decoderState = atomic_ptr.load();
		if(!decoderState)
			continue;

		if(auto flags = decoderState->mFlags.load(); flags & DecoderState::eMarkedForRemoval)
			continue;

		if(decoderState->mSequenceNumber == sequenceNumber)
			return decoderState;
	}

	return nullptr;
}

#pragma mark - AudioPlayerNode

void collector_finalizer_f(void *context)
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

	static const AVAudioFrameCount 	kRingBufferChunkSize 	= 2048;

	enum eAudioPlayerNodeFlags : unsigned int {
		eIsPlaying 				= 1u << 0,
		eOutputIsMuted 			= 1u << 1,
		eMuteRequested 			= 1u << 2,
		eRingBufferNeedsReset 	= 1u << 3,
	};

	enum eEventCommands : uint32_t {
		eEventDecodingStarted 		= 1,
		eEventDecodingComplete 		= 2,
		eEventDecodingCanceled 		= 3,
		eEventRenderingStarted 		= 4,
		eEventRenderingComplete 	= 5,
		eEventEndOfAudio 			= 6,
		eEventError 				= 7,
	};

	/// Weak reference to owning \c SFBAudioPlayerNode instance
	__weak SFBAudioPlayerNode 		*mNode 					= nil;

	/// The render block supplying audio
	AVAudioSourceNodeRenderBlock 	mRenderBlock 			= nullptr;

private:
	/// Ring buffer used to transfer audio from the decoding queue to the IOProc
	SFB::AudioRingBuffer			mAudioRingBuffer 		= {};

	/// The format of the audio supplied by \c mRenderBlock
	AVAudioFormat 					*mRenderingFormat		= nil;


	/// Active decoders and associated state
	DecoderStateArray 				*mActiveDecoders 		= nullptr;

	/// Decoders enqueued for playback that are not yet active
	DecoderQueue 					mQueuedDecoders 		= {};

	/// Lock used to protect access to \c mQueuedDecoders
	mutable SFB::UnfairLock			mQueueLock;


	/// Dispatch queue used for decoding
	dispatch_queue_t				mDecodingQueue 			= nullptr;

	/// Dispatch semaphore used for communication with the decoding queue
	dispatch_semaphore_t			mDecodingSemaphore 		= nullptr;


	/// Dispatch group  used to track in-progress decoding and delegate messages
	dispatch_group_t 				mDispatchGroup 			= nullptr;


	/// Ring buffer used to communicate decoding and render related events
	SFB::RingBuffer					mEventRingBuffer;

	/// Dispatch source processing events from \c mEventRingBuffer
	dispatch_source_t				mEventProcessor 		= nullptr;


	/// Dispatch source deleting decoder state data with \c eMarkedForRemoval
	dispatch_source_t				mCollector 				= nullptr;


	/// AudioPlayerNode flags
	std::atomic_uint 				mFlags 					= 0;
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");


	/// Counter used for unique keys to \c dispatch_queue_set_specific
	std::atomic_uint64_t 			mDispatchKeyCounter 	= 1;

public:
	AudioPlayerNode(AVAudioFormat *format, uint32_t ringBufferSize)
	: mRenderingFormat(format)
	{
		os_log_debug(_audioPlayerNodeLog, "<AudioPlayerNode: %p> created with render block format %{public}@", this, mRenderingFormat);

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

			// ========================================
			// 1. Determine how many audio frames are available to read in the ring buffer
			AVAudioFrameCount framesAvailableToRead = static_cast<AVAudioFrameCount>(mAudioRingBuffer.FramesAvailableToRead());

			// ========================================
			// 2. Output silence if a) the node isn't playing, b) the node is muted, or c) the ring buffer is empty
			if(!(mFlags.load() & eIsPlaying) || mFlags.load() & eOutputIsMuted || framesAvailableToRead == 0) {
				auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(frameCount);
				for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
					std::memset(outputData->mBuffers[i].mData, 0, byteCountToZero);
					outputData->mBuffers[i].mDataByteSize = byteCountToZero;
				}

				*isSilence = YES;
				return noErr;
			}

			// ========================================
			// 3. Read as many frames as available from the ring buffer
			AVAudioFrameCount framesToRead = std::min(framesAvailableToRead, frameCount);
			AVAudioFrameCount framesRead = static_cast<AVAudioFrameCount>(mAudioRingBuffer.Read(outputData, framesToRead));
			if(framesRead != framesToRead)
				os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

			// ========================================
			// 4. If the ring buffer didn't contain as many frames as requested fill the remainder with silence
			if(framesRead != frameCount) {
				os_log_debug(_audioPlayerNodeLog, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);

				auto framesOfSilence = frameCount - framesRead;
				auto byteCountToSkip = mAudioRingBuffer.Format().FrameCountToByteSize(framesRead);
				auto byteCountToZero = mAudioRingBuffer.Format().FrameCountToByteSize(framesOfSilence);
				for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
					std::memset(static_cast<int8_t *>(outputData->mBuffers[i].mData) + byteCountToSkip, 0, byteCountToZero);
					outputData->mBuffers[i].mDataByteSize += byteCountToZero;
				}
			}

			// ========================================
			// 5. If there is adequate space in the ring buffer for another chunk signal the decoding queue
			AVAudioFrameCount framesAvailableToWrite = static_cast<AVAudioFrameCount>(mAudioRingBuffer.FramesAvailableToWrite());
			if(framesAvailableToWrite >= kRingBufferChunkSize)
				dispatch_semaphore_signal(mDecodingSemaphore);

			// ========================================
			// Post-rendering actions

			// ========================================
			// 6. There is nothing more to do if no frames were rendered
			if(framesRead == 0)
				return noErr;

			// ========================================
			// 7. Perform bookkeeping to apportion the rendered frames appropriately
			//
			// framesRead contains the number of valid frames that were rendered
			// However, these could have come from any number of decoders depending on buffer sizes
			// So it is necessary to split them up here

			AVAudioFrameCount framesRemainingToDistribute = framesRead;

			auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
			while(decoderState) {
				AVAudioFrameCount decoderFramesRemaining = static_cast<AVAudioFrameCount>(decoderState->mFramesConverted.load() - decoderState->mFramesRendered.load());
				AVAudioFrameCount framesFromThisDecoder = std::min(decoderFramesRemaining, framesRemainingToDistribute);

				if(!(decoderState->mFlags.load() & DecoderState::eRenderingStarted)) {
					decoderState->mFlags.fetch_or(DecoderState::eRenderingStarted);

					// Submit the rendering started event
					const uint32_t cmd = eEventRenderingStarted;
					const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
					const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTicks(frameOffset / mAudioRingBuffer.Format().mSampleRate);

					uint8_t bytesToWrite [4 + 8 + 8];
					std::memcpy(bytesToWrite, &cmd, 4);
					std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
					std::memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
					mEventRingBuffer.Write(bytesToWrite, 4 + 8 + 8);
					dispatch_source_merge_data(mEventProcessor, 1);
				}

				decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
				framesRemainingToDistribute -= framesFromThisDecoder;

				if((decoderState->mFlags.load() & DecoderState::eDecodingComplete) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
					decoderState->mFlags.fetch_or(DecoderState::eRenderingComplete);

					// Submit the rendering complete event
					const uint32_t cmd = eEventRenderingComplete;
					const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
					const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTicks(frameOffset / mAudioRingBuffer.Format().mSampleRate);

					uint8_t bytesToWrite [4 + 8 + 8];
					std::memcpy(bytesToWrite, &cmd, 4);
					std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
					std::memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
					mEventRingBuffer.Write(bytesToWrite, 4 + 8 + 8);
					dispatch_source_merge_data(mEventProcessor, 1);
				}

				if(framesRemainingToDistribute == 0)
					break;

				decoderState = GetActiveDecoderStateFollowingSequenceNumber(decoderState->mSequenceNumber);
			}

			// ========================================
			// 8. If there are no active decoders schedule the end of audio notification

			decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
			if(!decoderState) {
				const uint32_t cmd = eEventEndOfAudio;
				const uint64_t hostTime = timestamp->mHostTime + SFB::ConvertSecondsToHostTicks(framesRead / mAudioRingBuffer.Format().mSampleRate);

				uint8_t bytesToWrite [4 + 8];
				std::memcpy(bytesToWrite, &cmd, 4);
				std::memcpy(bytesToWrite + 4, &hostTime, 8);
				mEventRingBuffer.Write(bytesToWrite, 4 + 8);
				dispatch_source_merge_data(mEventProcessor, 1);
			}

			return noErr;
		};

		// Allocate the audio ring buffer moving audio from the decoder queue to the render block
		if(!mAudioRingBuffer.Allocate(*(mRenderingFormat.streamDescription), ringBufferSize)) {
			os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Allocate() failed");
			throw std::runtime_error("SFB::Audio::RingBuffer::Allocate() failed");
		}

		// Allocate the event ring buffer
		if(!mEventRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::Allocate() failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate() failed");
		}

		mDecodingSemaphore = dispatch_semaphore_create(0);
		if(!mDecodingSemaphore) {
			os_log_error(_audioPlayerNodeLog, "dispatch_semaphore_create() failed");
			throw std::runtime_error("dispatch_semaphore_create() failed");
		}

		mDispatchGroup = dispatch_group_create();
		if(!mDispatchGroup) {
			os_log_error(_audioPlayerNodeLog, "dispatch_group_create() failed");
			throw std::runtime_error("dispatch_group_create() failed");
		}

		// MARK: Event Processing

		// Create the dispatch source used for event processing and delegate messaging
		mEventProcessor = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
		if(!mEventProcessor) {
			os_log_error(_audioPlayerNodeLog, "dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		dispatch_source_set_event_handler(mEventProcessor, ^{
			while(mEventRingBuffer.BytesAvailableToRead() >= 4) {
				uint32_t cmd;
				/*auto bytesRead =*/ mEventRingBuffer.Read(&cmd, 4);

				switch(cmd) {
					case eEventDecodingStarted:
						if(mEventRingBuffer.BytesAvailableToRead() >= 8) {
							uint64_t sequenceNumber;
							/*bytesRead =*/ mEventRingBuffer.Read(&sequenceNumber, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for eEventDecodingStarted", sequenceNumber);
								break;
							}

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node decodingStarted:decoderState->mDecoder];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventDecodingStarted");
						break;

					case eEventDecodingComplete:
						if(mEventRingBuffer.BytesAvailableToRead() >= 8) {
							uint64_t sequenceNumber;
							/*bytesRead =*/ mEventRingBuffer.Read(&sequenceNumber, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for eEventDecodingComplete", sequenceNumber);
								break;
							}

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node decodingComplete:decoderState->mDecoder];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventDecodingComplete");
						break;

					case eEventDecodingCanceled:
						if(mEventRingBuffer.BytesAvailableToRead() >= (8 + 1)) {
							uint64_t sequenceNumber;
							uint8_t partiallyRendered;
							/*bytesRead =*/ mEventRingBuffer.Read(&sequenceNumber, 8);
							/*bytesRead =*/ mEventRingBuffer.Read(&partiallyRendered, 1);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for eEventDecodingCanceled", sequenceNumber);
								break;
							}

							auto decoder = decoderState->mDecoder;

							decoderState->mFlags.fetch_or(DecoderState::eMarkedForRemoval);
							dispatch_source_merge_data(mCollector, 1);

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:partiallyRendered:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node decodingCanceled:decoder partiallyRendered:(partiallyRendered ? YES : NO)];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventDecodingCanceled");
						break;

					case eEventRenderingStarted:
						if(mEventRingBuffer.BytesAvailableToRead() >= (8 + 8)) {
							uint64_t sequenceNumber, hostTime;
							/*bytesRead =*/ mEventRingBuffer.Read(&sequenceNumber, 8);
							/*bytesRead =*/ mEventRingBuffer.Read(&hostTime, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for eEventRenderingStarted", sequenceNumber);
								break;
							}

							const auto now = mach_absolute_time();
							if(now > hostTime)
								os_log_error(_audioPlayerNodeLog, "Rendering will start event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTicksToNanos(now - hostTime)) / NSEC_PER_MSEC, decoderState->mDecoder);
							else
								os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTicksToNanos(hostTime - now)) / NSEC_PER_MSEC, decoderState->mDecoder);

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node renderingWillStart:decoderState->mDecoder atHostTime:hostTime];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventRenderingStarted");
						break;

					case eEventRenderingComplete:
						if(mEventRingBuffer.BytesAvailableToRead() >= (8 + 8)) {
							uint64_t sequenceNumber, hostTime;
							/*bytesRead =*/ mEventRingBuffer.Read(&sequenceNumber, 8);
							/*bytesRead =*/ mEventRingBuffer.Read(&hostTime, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_fault(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing for eEventRenderingComplete", sequenceNumber);
								break;
							}

							const auto now = mach_absolute_time();
							if(now > hostTime)
								os_log_error(_audioPlayerNodeLog, "Rendering will complete event processed %.2f msec late for %{public}@", static_cast<double>(SFB::ConvertHostTicksToNanos(now - hostTime)) / NSEC_PER_MSEC, decoderState->mDecoder);
							else
								os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for %{public}@", static_cast<double>(SFB::ConvertHostTicksToNanos(hostTime - now)) / NSEC_PER_MSEC, decoderState->mDecoder);

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillComplete:atHostTime:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node renderingWillComplete:decoderState->mDecoder atHostTime:hostTime];
									dispatch_group_leave(mDispatchGroup);
								});
							}

							decoderState->mFlags.fetch_or(DecoderState::eMarkedForRemoval);
							dispatch_source_merge_data(mCollector, 1);
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventRenderingComplete");
						break;

					case eEventEndOfAudio:
						if(mEventRingBuffer.BytesAvailableToRead() >= 8) {
							uint64_t hostTime;
							/*bytesRead =*/ mEventRingBuffer.Read(&hostTime, 8);

							const auto now = mach_absolute_time();
							if(now > hostTime)
								os_log_error(_audioPlayerNodeLog, "End of audio event processed %.2f msec late", static_cast<double>(SFB::ConvertHostTicksToNanos(now - hostTime)) / NSEC_PER_MSEC);
							else
								os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", static_cast<double>(SFB::ConvertHostTicksToNanos(hostTime - now)) / NSEC_PER_MSEC);

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:audioWillEndAtHostTime:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node audioWillEndAtHostTime:hostTime];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventEndOfAudio");
						break;

					case eEventError:
						if(mEventRingBuffer.BytesAvailableToRead() >= 8) {
							uint64_t key;
							/*bytesRead =*/ mEventRingBuffer.Read(&key, 8);

							NSError *error = (__bridge NSError *)dispatch_queue_get_specific(mNode.delegateQueue, reinterpret_cast<void *>(key));
							if(!error) {
								os_log_fault(_audioPlayerNodeLog, "Dispatch value for key %llu missing for eEventError", key);
								break;
							}

							dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), nullptr, nullptr);

							if([mNode.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)]) {
								auto node = mNode;
								dispatch_group_enter(mDispatchGroup);
								dispatch_async_and_wait(mNode.delegateQueue, ^{
									[node.delegate audioPlayerNode:node encounteredError:error];
									dispatch_group_leave(mDispatchGroup);
								});
							}
						}
						else
							os_log_fault(_audioPlayerNodeLog, "Missing data for eEventDecodingComplete");
						break;
				}
			}
		});

		// Start processing events
		dispatch_activate(mEventProcessor);

		// Set up the collector
		mCollector = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0));
		if(!mCollector) {
			os_log_error(_audioPlayerNodeLog, "dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		// Allocate and initialize the decoder state array
		mActiveDecoders = new DecoderStateArray;
		for(auto& atomic_ptr : *mActiveDecoders)
			atomic_ptr.store(nullptr);

		// The collector takes ownership of `mActiveDecoders` and the finalizer is responsible
		// for deleting any allocated decoder state it contains as well as the array itself
		dispatch_set_context(mCollector, mActiveDecoders);
		dispatch_set_finalizer_f(mCollector, &collector_finalizer_f);

		dispatch_source_set_event_handler(mCollector, ^{
			auto context = dispatch_get_context(mCollector);
			auto decoders = static_cast<DecoderStateArray *>(context);

			for(auto& atomic_ptr : *decoders) {
				auto decoderState = atomic_ptr.load();
				if(!decoderState || !(decoderState->mFlags.load() & DecoderState::eMarkedForRemoval))
					continue;

				os_log_debug(_audioPlayerNodeLog, "Deleting decoder state for %{public}@", decoderState->mDecoder);
				delete atomic_ptr.exchange(nullptr);
			}
		});

		// Start collecting
		dispatch_activate(mCollector);

		// Create the dispatch queue used for decoding
		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
		if(!attr) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_attr_make_with_qos_class() failed");
			throw std::runtime_error("dispatch_queue_attr_make_with_qos_class() failed");
		}

		mDecodingQueue = dispatch_queue_create_with_target("org.sbooth.AudioEngine.AudioPlayerNode.Decoder", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!mDecodingQueue) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_create_with_target() failed");
			throw std::runtime_error("dispatch_queue_create_with_target() failed");
		}
	}

	~AudioPlayerNode()
	{
		// Cancel any further event processing
		dispatch_source_cancel(mEventProcessor);

		// Cancel decoding
		CancelCurrentDecoder();
		dispatch_semaphore_signal(mDecodingSemaphore);

		auto timeout = dispatch_group_wait(mDispatchGroup, /*DISPATCH_TIME_FOREVER*/dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));
		if(timeout)
			os_log_fault(_audioPlayerNodeLog, "<AudioPlayerNode: %p> timeout waiting for dispatch group blocks to complete", this);

		os_log_debug(_audioPlayerNodeLog, "<AudioPlayerNode: %p> destroyed", this);
	}

#pragma mark - Playback Control

	inline void Play() noexcept
	{
		mFlags.fetch_or(eIsPlaying);
	}

	inline void Pause() noexcept
	{
		mFlags.fetch_and(~eIsPlaying);
	}

	void Stop() noexcept
	{
		mFlags.fetch_and(~eIsPlaying);
		Reset();
	}

	inline void TogglePlayPause() noexcept
	{
		mFlags.fetch_xor(eIsPlaying);
	}

#pragma mark - Playback State

	inline bool IsPlaying() const noexcept
	{
		return (mFlags.load() & eIsPlaying) != 0;
	}

	bool IsReady() const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? true : false;
	}

#pragma mark - Playback Properties

	SFBAudioPlayerNodePlaybackPosition PlaybackPosition() const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .framePosition = SFBUnknownFramePosition, .frameLength = SFBUnknownFrameLength };

		return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
	}

	SFBAudioPlayerNodePlaybackTime PlaybackTime() const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		SFBAudioPlayerNodePlaybackTime playbackTime = { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		int64_t framePosition = decoderState->FramePosition();
		int64_t frameLength = decoderState->FrameLength();

		if(double sampleRate = decoderState->mConverter.outputFormat.sampleRate; sampleRate > 0) {
			if(framePosition != SFBUnknownFramePosition)
				playbackTime.currentTime = framePosition / sampleRate;
			if(frameLength != SFBUnknownFrameLength)
				playbackTime.totalTime = frameLength / sampleRate;
		}

		return playbackTime;
	}

	bool GetPlaybackPositionAndTime(SFBAudioPlayerNodePlaybackPosition *playbackPosition, SFBAudioPlayerNodePlaybackTime *playbackTime) const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
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
			if(double sampleRate = decoderState->mConverter.outputFormat.sampleRate; sampleRate > 0) {
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

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		AVAudioFramePosition framePosition = decoderState->FramePosition();
		AVAudioFramePosition targetFrame = framePosition + static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);

		if(targetFrame >= decoderState->FrameLength())
			targetFrame = std::max(decoderState->FrameLength() - 1, 0ll);

		return SeekToFrame(targetFrame);
	}

	bool SeekBackward(NSTimeInterval secondsToSkip) noexcept
	{
		if(secondsToSkip < 0)
			secondsToSkip = 0;

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		AVAudioFramePosition framePosition = decoderState->FramePosition();
		AVAudioFramePosition targetFrame = framePosition - static_cast<AVAudioFramePosition>(secondsToSkip * sampleRate);

		if(targetFrame < 0)
			targetFrame = 0;

		return SeekToFrame(targetFrame);
	}

	bool SeekToTime(NSTimeInterval timeInSeconds) noexcept
	{
		if(timeInSeconds < 0)
			timeInSeconds = 0;

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		AVAudioFramePosition targetFrame = static_cast<AVAudioFramePosition>(timeInSeconds * sampleRate);

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

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return false;

		AVAudioFramePosition frameLength = decoderState->FrameLength();
		return SeekToFrame(static_cast<AVAudioFramePosition>(frameLength * position));
	}

	bool SeekToFrame(AVAudioFramePosition frame) noexcept
	{
		if(frame < 0)
			frame = 0;

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState || !decoderState->mDecoder.supportsSeeking)
			return false;

		if(frame >= decoderState->FrameLength())
			frame = std::max(decoderState->FrameLength() - 1, 0ll);

		decoderState->mFrameToSeek.store(frame);
		dispatch_semaphore_signal(mDecodingSemaphore);

		return true;
	}

	bool SupportsSeeking() const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? decoderState->mDecoder.supportsSeeking : false;
	}

#pragma mark - Format Information

	inline AVAudioFormat * RenderingFormat() const noexcept
	{
		return mRenderingFormat;
	}

	inline bool SupportsFormat(AVAudioFormat *format) const noexcept
	{
		// Gapless playback requires the same number of channels at the same sample rate
		return format.channelCount == mRenderingFormat.channelCount && format.sampleRate == mRenderingFormat.sampleRate;
	}

#pragma mark - Queue Management

	bool EnqueueDecoder(id <SFBPCMDecoding>decoder, bool reset, NSError **error) noexcept
	{
		if(!decoder.isOpen && ![decoder openReturningError:error])
			return false;

		if(!SupportsFormat(decoder.processingFormat)) {
			os_log_error(_audioPlayerNodeLog, "Unsupported decoder processing format: %{public}@", decoder.processingFormat);

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

	id <SFBPCMDecoding> DequeueDecoder() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(mQueueLock);
		id <SFBPCMDecoding> decoder = nil;
		if(!mQueuedDecoders.empty()) {
			decoder = mQueuedDecoders.front();
			mQueuedDecoders.pop();
		}
		return decoder;
	}

	id<SFBPCMDecoding> CurrentDecoder() const noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
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

	/// Returns the decoder state in \c mActiveDecoders with the smallest sequence number that has not completed rendering and has not been marked for removal
	inline DecoderState * GetActiveDecoderStateWithSmallestSequenceNumber() const noexcept
	{
		return ::GetActiveDecoderStateWithSmallestSequenceNumber(*mActiveDecoders);
	}

	/// Returns the decoder state  in \c mActiveDecoders with the smallest sequence number greater than \c sequenceNumber that has not completed rendering and has not been marked for removal
	inline DecoderState * GetActiveDecoderStateFollowingSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetActiveDecoderStateFollowingSequenceNumber(*mActiveDecoders, sequenceNumber);
	}

	/// Returns the decoder state  in \c mActiveDecoders with sequence number equal to \c sequenceNumber that has not been marked for removal
	inline DecoderState * GetDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		return ::GetDecoderStateWithSequenceNumber(*mActiveDecoders, sequenceNumber);
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
					const uint32_t cmd = eEventError;
					const uint64_t key = mDispatchKeyCounter.fetch_add(1);

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

					uint8_t bytesToWrite [4 + 8];
					std::memcpy(bytesToWrite, &cmd, 4);
					std::memcpy(bytesToWrite + 4, &key, 8);
					mEventRingBuffer.Write(bytesToWrite, 4 + 8);
					dispatch_source_merge_data(mEventProcessor, 1);

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
						// and the collector as consumer, with the stored values used in between production
						// and consumption by any number of other threads/queues including the IOProc.
						//
						// Slots in `mActiveDecoders` are assigned values in two places: here and the
						// collector. The collector assigns nullptr to slots holding existing non-null
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
						// being consumed by the IOProc.
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

				// In the event the render block output format and decoder processing
				// format don't match, conversion will be performed in DecoderState::DecodeAudio()

				os_log_debug(_audioPlayerNodeLog, "Dequeued %{public}@, processing format %{public}@", decoderState->mDecoder, decoderState->mDecoder.processingFormat);

				AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mRenderingFormat frameCapacity:kRingBufferChunkSize];
				if(!buffer) {
					os_log_error(_audioPlayerNodeLog, "Error creating AVAudioPCMBuffer with format %{public}@ and frame capacity %d", mRenderingFormat, kRingBufferChunkSize);

					NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain
														 code:SFBAudioPlayerNodeErrorCodeInternalError
													 userInfo:@{ NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"An internal error occurred in AudioPlayerNode.", @""),
																 NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Error creating AVAudioPCMBuffer", @""),
															  }];

					// Submit the error event
					const uint32_t cmd = eEventError;
					const uint64_t key = mDispatchKeyCounter.fetch_add(1);

					dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

					uint8_t bytesToWrite [4 + 8];
					std::memcpy(bytesToWrite, &cmd, 4);
					std::memcpy(bytesToWrite + 4, &key, 8);
					mEventRingBuffer.Write(bytesToWrite, 4 + 8);
					dispatch_source_merge_data(mEventProcessor, 1);

					return;
				}

				// Process the decoder until canceled or complete
				for(;;) {
					// If a seek is pending reset the ring buffer
					if(decoderState->mFrameToSeek.load() != DecoderState::kInvalidFramePosition)
						mFlags.fetch_or(eRingBufferNeedsReset);

					// Reset the ring buffer if required, to prevent audible artifacts
					if(mFlags.load() & eRingBufferNeedsReset) {
						mFlags.fetch_and(~eRingBufferNeedsReset);

						// Ensure output is muted before performing operations that aren't thread-safe
						if(mNode.engine.isRunning) {
							mFlags.fetch_or(eMuteRequested);

							// The IOProc will clear eMuteRequested when the current render cycle completes
							while(mFlags.load() & eMuteRequested)
								dispatch_semaphore_wait(mDecodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 100));
						}
						else
							mFlags.fetch_or(eOutputIsMuted);

						// Perform seek if one is pending
						if(decoderState->mFrameToSeek.load() != DecoderState::kInvalidFramePosition)
							decoderState->PerformSeek();

						// Reset() is not thread-safe but the IOProc is outputting silence
						mAudioRingBuffer.Reset();

						// Clear the mute flag
						mFlags.fetch_and(~eOutputIsMuted);
					}

					// Determine how many frames are available in the ring buffer
					auto framesAvailableToWrite = mAudioRingBuffer.FramesAvailableToWrite();

					// Force writes to the ring buffer to be at least kRingBufferChunkSize
					if(framesAvailableToWrite >= kRingBufferChunkSize) {
						if(!(decoderState->mFlags.load() & DecoderState::eDecodingStarted)) {
							os_log_debug(_audioPlayerNodeLog, "Decoding started for %{public}@", decoderState->mDecoder);

							decoderState->mFlags.fetch_or(DecoderState::eDecodingStarted);

							// Submit the decoding started event
							const uint32_t cmd = eEventDecodingStarted;

							uint8_t bytesToWrite [4 + 8];
							std::memcpy(bytesToWrite, &cmd, 4);
							std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
							mEventRingBuffer.Write(bytesToWrite, 4 + 8);
							dispatch_source_merge_data(mEventProcessor, 1);
						}

						// Decode audio into the buffer, converting to the bus format in the process
						if(NSError *error = nil; !decoderState->DecodeAudio(buffer, &error)) {
							os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);

							if(error) {
								// Submit the error event
								const uint32_t cmd = eEventError;
								const uint64_t key = mDispatchKeyCounter.fetch_add(1);

								dispatch_queue_set_specific(mNode.delegateQueue, reinterpret_cast<void *>(key), (__bridge_retained void *)error, &release_nserror_f);

								uint8_t bytesToWrite [4 + 8];
								std::memcpy(bytesToWrite, &cmd, 4);
								std::memcpy(bytesToWrite + 4, &key, 8);
								mEventRingBuffer.Write(bytesToWrite, 4 + 8);
								dispatch_source_merge_data(mEventProcessor, 1);
							}
						}

						// Write the decoded audio to the ring buffer for rendering
						auto framesWritten = mAudioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
						if(framesWritten != buffer.frameLength)
							os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Write() failed");

						if(decoderState->mFlags.load() & DecoderState::eDecodingComplete) {
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

							// Submit the decoding complete event
							const uint32_t cmd = eEventDecodingComplete;

							uint8_t bytesToWrite [4 + 8];
							std::memcpy(bytesToWrite, &cmd, 4);
							std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
							mEventRingBuffer.Write(bytesToWrite, 4 + 8);
							dispatch_source_merge_data(mEventProcessor, 1);

							os_log_debug(_audioPlayerNodeLog, "Decoding complete for %{public}@", decoderState->mDecoder);

							return;
						}
					}
					else if(decoderState->mFlags.load() & DecoderState::eCancelDecoding) {
						os_log_debug(_audioPlayerNodeLog, "Canceling decoding for %{public}@", decoderState->mDecoder);

						mFlags.fetch_or(eRingBufferNeedsReset);

						// Submit the decoding canceled event
						const uint32_t cmd = eEventDecodingCanceled;
						const uint8_t partiallyRendered = decoderState->mFlags.load() & DecoderState::eRenderingStarted;

						uint8_t bytesToWrite [4 + 8 + 1];
						std::memcpy(bytesToWrite, &cmd, 4);
						std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
						std::memcpy(bytesToWrite + 4 + 8, &partiallyRendered, 1);
						mEventRingBuffer.Write(bytesToWrite, 4 + 8 + 1);
						dispatch_source_merge_data(mEventProcessor, 1);

						return;
					}

					// Wait for additional space in the ring buffer
					dispatch_semaphore_wait(mDecodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10));
				}
			}
		});
	}

};

}

#pragma mark -

/// The default ring buffer capacity in frames
const AVAudioFrameCount kDefaultRingBufferFrameCapacity = 16384;

@interface SFBAudioPlayerNode ()
{
@private
	AudioPlayerNode::unique_ptr _impl;
}
@end

@implementation SFBAudioPlayerNode

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

#if 0
		// See the comments in SFBAudioPlayer -configureEngineForGaplessPlaybackOfFormat:
		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice while 1156 is AVAudioSourceNode's default
		AVAudioFrameCount maximumFramesToRender = static_cast<AVAudioFrameCount>(ceil(512 * (format.sampleRate / 44100)));
		if(self.AUAudioUnit.maximumFramesToRender < maximumFramesToRender) {
			os_log_debug(_audioPlayerNodeLog, "Setting maximumFramesToRender to %u", maximumFramesToRender);
			self.AUAudioUnit.maximumFramesToRender = maximumFramesToRender;
		}
#endif
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
