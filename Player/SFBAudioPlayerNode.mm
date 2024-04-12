//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <atomic>
#import <cmath>
#import <mutex>
#import <queue>
#import <thread>

#import <mach/mach_time.h>
#import <os/log.h>

#import "SFBAudioPlayerNode.h"

#import "SFBAudioRingBuffer.hpp"
#import "SFBRingBuffer.hpp"
#import "SFBUnfairLock.hpp"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"

const NSTimeInterval SFBUnknownTime = -1;
NSErrorDomain const SFBAudioPlayerNodeErrorDomain = @"org.sbooth.AudioEngine.AudioPlayerNode";

namespace {

#pragma mark - Time Utilities

// These functions are probably unnecessarily complicated because
// on Intel processors mach_timebase_info is always 1/1. However,
// on PPC it is either 1000000000/33333335 or 1000000000/25000000 so
// naively multiplying by .numer then dividing by .denom may result in
// integer overflow. To avoid the possibility double is used here, but
// __int128 would be an alternative.

/// Returns the number of host ticks per nanosecond
double HostTicksPerNano()
{
	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return static_cast<double>(timebase_info.numer) / static_cast<double>(timebase_info.denom);
}

/// Returns the number of nanoseconds per host tick
double NanosPerHostTick()
{
	mach_timebase_info_data_t timebase_info;
	auto result = mach_timebase_info(&timebase_info);
	assert(result == KERN_SUCCESS);
	return static_cast<double>(timebase_info.denom) / static_cast<double>(timebase_info.numer);
}

/// The number of host ticks per nanosecond
const double kHostTicksPerNano = HostTicksPerNano();
/// The number of nanoseconds per host tick
const double kNanosPerHostTick = NanosPerHostTick();

/// Converts \c ns nanoseconds to host ticks and returns the result
inline uint64_t ConvertNanosToHostTicks(double ns) noexcept
{
	return static_cast<uint64_t>(ns * kNanosPerHostTick);
}

/// Converts \c s seconds to host ticks and returns the result
inline uint64_t ConvertSecondsToHostTicks(double s) noexcept
{
	return ConvertNanosToHostTicks(s * NSEC_PER_SEC);
}

/// Converts \c t host ticks to nanoseconds and returns the result
inline double ConvertHostTicksToNanos(uint64_t t) noexcept
{
	return static_cast<double>(t) * kHostTicksPerNano;
}

#pragma mark - Shared State

os_log_t _audioPlayerNodeLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

#pragma mark - Flags

enum eAudioPlayerNodeFlags : unsigned int {
	eAudioPlayerNodeFlagIsPlaying					= 1u << 0,
	eAudioPlayerNodeFlagOutputIsMuted				= 1u << 1,
	eAudioPlayerNodeFlagMuteRequested				= 1u << 2,
	eAudioPlayerNodeFlagRingBufferNeedsReset		= 1u << 3,
	eAudioPlayerNodeFlagStopDecoderThread			= 1u << 4,
};

enum eAudioPlayerNodeRenderEventRingBufferCommands : uint32_t {
	eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted	= 1,
	eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete	= 2,
	eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio			= 3
};

#pragma mark - Constants

const AVAudioFrameCount 	kRingBufferFrameCapacity 	= 16384;
const AVAudioFrameCount 	kRingBufferChunkSize 		= 2048;
const size_t 				kDecoderStateArraySize		= 8;
const int64_t				kInvalidFramePosition 		= -1;

#pragma mark - Decoder State

/// State data for tracking/syncing decoding progress
struct DecoderStateData {
	using atomic_ptr = std::atomic<DecoderStateData *>;

	static const AVAudioFrameCount kDefaultBufferSize = 1024;

	enum eDecoderStateDataFlags : unsigned int {
		eCancelDecodingFlag		= 1u << 0,
		eDecodingStartedFlag	= 1u << 1,
		eDecodingCompleteFlag	= 1u << 2,
		eRenderingStartedFlag	= 1u << 3,
		eRenderingCompleteFlag	= 1u << 4,
		eMarkedForRemovalFlag 	= 1u << 5
	};

	/// Monotonically increasing instance counter
	const uint64_t			mSequenceNumber;

	/// Decoder state data flags
	std::atomic_uint 		mFlags;
	/// The number of frames decoded
	std::atomic_int64_t 	mFramesDecoded;
	/// The number of frames converted
	std::atomic_int64_t 	mFramesConverted;
	/// The number of frames rendered
	std::atomic_int64_t 	mFramesRendered;
	/// The total number of audio frames
	std::atomic_int64_t 	mFrameLength;
	/// The desired seek offset
	std::atomic_int64_t 	mFrameToSeek;

	friend struct PlayerNodeImpl;

private:
	/// Decodes audio from the source representation to PCM
	id <SFBPCMDecoding> 	mDecoder;
	/// Converts audio from the decoder's processing format to another PCM variant at the same sample rate
	AVAudioConverter 		*mConverter;
	/// Buffer used internally for buffering during conversion
	AVAudioPCMBuffer 		*mDecodeBuffer;
	/// Next sequence number to use
	static uint64_t			sSequenceNumber;

public:
	DecoderStateData(id <SFBPCMDecoding> decoder, AVAudioFormat *format, AVAudioFrameCount frameCapacity = kDefaultBufferSize)
	: mSequenceNumber(sSequenceNumber++), mFlags(0), mFramesDecoded(0), mFramesConverted(0), mFramesRendered(0), mFrameLength(decoder.frameLength), mFrameToSeek(kInvalidFramePosition), mDecoder(decoder), mConverter(nil), mDecodeBuffer(nil)
	{
		mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
		// The logic in this class assumes no SRC is performed by mConverter
		assert(mConverter.inputFormat.sampleRate == mConverter.outputFormat.sampleRate);
		mDecodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.inputFormat frameCapacity:frameCapacity];

		AVAudioFramePosition framePosition = decoder.framePosition;
		if(framePosition != 0) {
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

	bool DecodeAudio(AVAudioPCMBuffer *buffer, NSError **error = nullptr)
	{
#if DEBUG
		assert(buffer.frameCapacity == mDecodeBuffer.frameCapacity);
#endif

		if(![mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:mDecodeBuffer.frameCapacity error:error])
			return false;

		if(mDecodeBuffer.frameLength == 0) {
			mFlags.fetch_or(eDecodingCompleteFlag);
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
	bool PerformSeek()
	{
		AVAudioFramePosition seekOffset = mFrameToSeek.load();

		os_log_debug(_audioPlayerNodeLog, "Seeking to frame %lld", seekOffset);

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

uint64_t DecoderStateData::sSequenceNumber = 0;
using DecoderQueue = std::queue<id <SFBPCMDecoding>>;

#pragma mark - PlayerNodeState

/// SFBAudioPlayerNode implementation
struct PlayerNodeImpl
{
	using unique_ptr = std::unique_ptr<PlayerNodeImpl>;

	/// Weak reference to owning Obj-C SFBAudioPlayerNode instance
	__weak SFBAudioPlayerNode 		*_node;

private:
	/// The lock used to protect access to \c _queuedDecoders
	SFB::UnfairLock					_queueLock;
	/// Decoders enqueued for playback
	DecoderQueue 					_queuedDecoders;

	// Decoding thread variables
	std::thread 					_decodingThread;
	dispatch_semaphore_t			_decodingSemaphore;

	/// Queue used for sending delegate messages
	dispatch_queue_t				_notificationQueue;

	/// Dispatch source processing render events from \c _renderEventsRingBuffer
	dispatch_source_t				_renderEventsProcessor;

	/// Dispatch source deleting decoder state data with \c eMarkedForRemovalFlag
	dispatch_source_t				_collector;

	// Shared state accessed from multiple threads/queues
	std::atomic_uint 				_flags;
	SFB::AudioRingBuffer			_audioRingBuffer;
	SFB::RingBuffer					_renderEventsRingBuffer;
	DecoderStateData::atomic_ptr 	_decoderStateArray [kDecoderStateArraySize];

public:
	PlayerNodeImpl(AVAudioFormat *format, uint32_t ringBufferSize)
	{
		NSCParameterAssert(format != nil);
		NSCParameterAssert(format.isStandard);

		// _flags and _decoderStateArray are used in the render block so must be lock free
		assert(_flags.is_lock_free());
		assert(_decoderStateArray[0].is_lock_free());

		// Initialize the decoder array
		for(size_t i = 0; i < kDecoderStateArraySize; ++i)
			_decoderStateArray[i].store(nullptr);

		// Allocate the audio ring buffer and the rendering events ring buffer
		if(!_audioRingBuffer.Allocate(*(format.streamDescription), ringBufferSize)) {
			os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Allocate() failed");
			throw std::runtime_error("SFB::Audio::RingBuffer::Allocate() failed");
		}

		if(!_renderEventsRingBuffer.Allocate(256)) {
			os_log_error(_audioPlayerNodeLog, "SFB::RingBuffer::Allocate() failed");
			throw std::runtime_error("SFB::RingBuffer::Allocate() failed");
		}

		// Create the dispatch queue used for sending delegate messages
		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
		_notificationQueue = dispatch_queue_create_with_target("org.sbooth.AudioEngine.AudioPlayerNode.NotificationQueue", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
		if(!_notificationQueue) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_create_with_target failed");
			throw std::runtime_error("dispatch_queue_create_with_target failed");
		}

		_decodingSemaphore = dispatch_semaphore_create(0);
		if(!_decodingSemaphore) {
			os_log_error(_audioPlayerNodeLog, "dispatch_semaphore_create failed");
			throw std::runtime_error("dispatch_semaphore_create failed");
		}

		// Set up render events processing for delegate notifications
		_renderEventsProcessor = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
		if(!_renderEventsProcessor) {
			os_log_error(_audioPlayerNodeLog, "dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		dispatch_source_set_event_handler(_renderEventsProcessor, ^{
			while(_renderEventsRingBuffer.BytesAvailableToRead() >= 4) {
				uint32_t cmd;
				/*auto bytesRead =*/ _renderEventsRingBuffer.Read(&cmd, 4);

				switch(cmd) {
					case eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted:
						if(_renderEventsRingBuffer.BytesAvailableToRead() >= (8 + 8)) {
							uint64_t sequenceNumber, hostTime;
							/*bytesRead =*/ _renderEventsRingBuffer.Read(&sequenceNumber, 8);
							/*bytesRead =*/ _renderEventsRingBuffer.Read(&hostTime, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_error(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing", sequenceNumber);
								break;
							}

							os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for \"%{public}@\"", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC, [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

							if([_node.delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)])
								dispatch_async_and_wait(_notificationQueue, ^{
									[_node.delegate audioPlayerNode:_node renderingWillStart:decoderState->mDecoder atHostTime:hostTime];
								});

							if([_node.delegate respondsToSelector:@selector(audioPlayerNode:renderingStarted:)]) {
								id<SFBPCMDecoding> decoder = decoderState->mDecoder;
								dispatch_time_t notificationTime = hostTime;
								dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
									double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
									double tolerance = 1000 / _audioRingBuffer.Format().mSampleRate;
									if(abs(delta) > tolerance)
										os_log_debug(_audioPlayerNodeLog, "Rendering started notification for \"%{public}@\" arrived %.2f msec %s", [[NSFileManager defaultManager] displayNameAtPath:decoder.inputSource.url.path], delta, delta > 0 ? "late" : "early");
#endif

									[_node.delegate audioPlayerNode:_node renderingStarted:decoder];
								});
							}
						}
						else
							os_log_error(_audioPlayerNodeLog, "Missing data for eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted");
						break;

					case eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete:
						if(_renderEventsRingBuffer.BytesAvailableToRead() >= (8 + 8)) {
							uint64_t sequenceNumber, hostTime;
							/*bytesRead =*/ _renderEventsRingBuffer.Read(&sequenceNumber, 8);
							/*bytesRead =*/ _renderEventsRingBuffer.Read(&hostTime, 8);

							auto decoderState = GetDecoderStateWithSequenceNumber(sequenceNumber);
							if(!decoderState) {
								os_log_error(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing", sequenceNumber);
								break;
							}

							os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for \"%{public}@\"", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC, [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

							if([_node.delegate respondsToSelector:@selector(audioPlayerNode:renderingComplete:)]) {
								// Store a strong reference to `decoderState->mDecoder` for use in the notification block
								// Otherwise the collector could collect `decoderState` before the block is invoked
								// resulting in a `nil` decoder being passed in -audioPlayerNode:renderingComplete:
								// with a possible subsequent EXC_BAD_ACCESS from messaging a non-optional `nil` object
								id<SFBPCMDecoding> decoder = decoderState->mDecoder;
								dispatch_time_t notificationTime = hostTime;
								dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
									double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
									double tolerance = 1000 / _audioRingBuffer.Format().mSampleRate;
									if(abs(delta) > tolerance)
										os_log_debug(_audioPlayerNodeLog, "Rendering complete notification for \"%{public}@\" arrived %.2f msec %s", [[NSFileManager defaultManager] displayNameAtPath:decoder.inputSource.url.path], delta, delta > 0 ? "late" : "early");
#endif

									[_node.delegate audioPlayerNode:_node renderingComplete:decoder];
								});
							}

							// The last action performed with a decoder that has completed rendering is this notification
							decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);
							dispatch_source_merge_data(_collector, 1);
						}
						else
							os_log_error(_audioPlayerNodeLog, "Missing data for eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete");
						break;

					case eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio:
						if(_renderEventsRingBuffer.BytesAvailableToRead() >= 8) {
							uint64_t hostTime;
							/*bytesRead =*/ _renderEventsRingBuffer.Read(&hostTime, 8);

							os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC);

							if([_node.delegate respondsToSelector:@selector(audioPlayerNodeEndOfAudio:)]) {
								dispatch_time_t notificationTime = hostTime;
								dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
									double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
									double tolerance = 1000 / _audioRingBuffer.Format().mSampleRate;
									if(abs(delta) > tolerance)
										os_log_debug(_audioPlayerNodeLog, "End of audio notification arrived %.2f msec %s", delta, delta > 0 ? "late" : "early");
#endif

									[_node.delegate audioPlayerNodeEndOfAudio:_node];
								});
							}
						}
						else
							os_log_error(_audioPlayerNodeLog, "Missing data for eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio");
						break;
				}
			}
		});

		// Start processing render events
		dispatch_activate(_renderEventsProcessor);

		// Set up the collector
		_collector = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0));
		if(!_collector) {
			os_log_error(_audioPlayerNodeLog, "dispatch_source_create failed");
			throw std::runtime_error("dispatch_source_create failed");
		}

		dispatch_source_set_event_handler(_collector, ^{
			for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
				auto decoderState = _decoderStateArray[i].load();
				if(!decoderState || !(decoderState->mFlags.load() & DecoderStateData::eMarkedForRemovalFlag))
					continue;

				// See comment in -decoderThreadEntry on why I believe it's safe to use store() and not a CAS loop
				os_log_debug(_audioPlayerNodeLog, "Collecting decoder for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
				_decoderStateArray[i].store(nullptr);
				delete decoderState;
			}
		});

		// Start collecting
		dispatch_activate(_collector);

		// Launch the decoding thread
		try {
			_decodingThread = std::thread(&PlayerNodeImpl::DecoderThreadEntry, this);
		}

		catch(const std::exception& e) {
			os_log_error(_audioPlayerNodeLog, "Unable to create decoding thread: %{public}s", e.what());
			throw std::runtime_error("Unable to create decoding thread");
		}
	}

	~PlayerNodeImpl()
	{
		ClearQueue();
		CancelCurrentDecoder();

		_flags.fetch_or(eAudioPlayerNodeFlagStopDecoderThread);
		dispatch_semaphore_signal(_decodingSemaphore);
		_decodingThread.join();

		for(size_t i = 0; i < kDecoderStateArraySize; ++i)
			delete _decoderStateArray[i].exchange(nullptr);
	}

#pragma mark - Playback Control

	inline void Play() noexcept
	{
		_flags.fetch_or(eAudioPlayerNodeFlagIsPlaying);
	}

	inline void Pause() noexcept
	{
		_flags.fetch_and(~eAudioPlayerNodeFlagIsPlaying);
	}

	void Stop() noexcept
	{
		_flags.fetch_and(~eAudioPlayerNodeFlagIsPlaying);
		Reset();
	}

	inline void TogglePlayPause() noexcept
	{
		_flags.fetch_xor(eAudioPlayerNodeFlagIsPlaying);
	}

#pragma mark - Playback State

	inline bool IsPlaying() noexcept
	{
		return (_flags.load() & eAudioPlayerNodeFlagIsPlaying) != 0;
	}

	bool IsReady() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? true : false;
	}

	id<SFBPCMDecoding> CurrentDecoder() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? decoderState->mDecoder : nil;
	}

#pragma mark - Playback Properties

	SFBAudioPlayerNodePlaybackPosition PlaybackPosition() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .framePosition = SFBUnknownFramePosition, .frameLength = SFBUnknownFrameLength };

		return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
	}

	SFBAudioPlayerNodePlaybackTime PlaybackTime() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState)
			return { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		SFBAudioPlayerNodePlaybackTime playbackTime = { .currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime };

		int64_t framePosition = decoderState->FramePosition();
		int64_t frameLength = decoderState->FrameLength();

		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		if(sampleRate > 0) {
			if(framePosition != SFBUnknownFramePosition)
				playbackTime.currentTime = framePosition / sampleRate;
			if(frameLength != SFBUnknownFrameLength)
				playbackTime.totalTime = frameLength / sampleRate;
		}

		return playbackTime;
	}

	bool GetPlaybackPositionAndTime(SFBAudioPlayerNodePlaybackPosition *playbackPosition, SFBAudioPlayerNodePlaybackTime *playbackTime) noexcept
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
			double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
			if(sampleRate > 0) {
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
		AVAudioFramePosition targetFrame = framePosition + (AVAudioFramePosition)(secondsToSkip * sampleRate);

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
		AVAudioFramePosition targetFrame = framePosition - (AVAudioFramePosition)(secondsToSkip * sampleRate);

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
		AVAudioFramePosition targetFrame = (AVAudioFramePosition)(timeInSeconds * sampleRate);

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
		dispatch_semaphore_signal(_decodingSemaphore);

		return true;
	}

	bool SupportsSeeking() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		return decoderState ? decoderState->mDecoder.supportsSeeking : false;
	}

#pragma mark - Internals

	bool EnqueueDecoder(id <SFBPCMDecoding>decoder, bool reset, NSError **error) noexcept
	{
		NSCParameterAssert(decoder != nil);

		os_log_info(_audioPlayerNodeLog, "Enqueuing \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoder.inputSource.url.path]);

		if(!decoder.isOpen && ![decoder openReturningError:error])
			return false;

		if(!SupportsFormat(decoder.processingFormat)) {
			os_log_error(_audioPlayerNodeLog, "Unsupported decoder processing format: %{public}@", decoder.processingFormat);

			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioPlayerNodeErrorDomain
												 code:SFBAudioPlayerNodeErrorFormatNotSupported
						descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
												  url:decoder.inputSource.url
										failureReason:NSLocalizedString(@"Unsupported file format", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's format is not supported by this player.", @"")];

			return false;
		}

		if(reset) {
			ClearQueue();
			auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
			if(decoderState)
				decoderState->mFlags.fetch_or(DecoderStateData::eCancelDecodingFlag);
		}

		{
			std::lock_guard<SFB::UnfairLock> lock(_queueLock);
			_queuedDecoders.push(decoder);
		}

		dispatch_semaphore_signal(_decodingSemaphore);

		return true;
	}

#pragma mark - Rendering

	/// Renders audio from the current decoder to @c outputData
	///
	OSStatus Render(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) noexcept
	{
		// ========================================
		// Pre-rendering actions

		// ========================================
		// 0. Mute output if requested
		if(_flags.load() & eAudioPlayerNodeFlagMuteRequested) {
			_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);
			_flags.fetch_and(~eAudioPlayerNodeFlagMuteRequested);
			dispatch_semaphore_signal(_decodingSemaphore);
		}

		// ========================================
		// Rendering

		// ========================================
		// 1. Determine how many audio frames are available to read in the ring buffer
		AVAudioFrameCount framesAvailableToRead = static_cast<AVAudioFrameCount>(_audioRingBuffer.FramesAvailableToRead());

		// ========================================
		// 2. Output silence if a) the node isn't playing, b) the node is muted, or c) the ring buffer is empty
		if(!(_flags.load() & eAudioPlayerNodeFlagIsPlaying) || _flags.load() & eAudioPlayerNodeFlagOutputIsMuted || framesAvailableToRead == 0) {
			auto byteCountToZero = _audioRingBuffer.Format().FrameCountToByteSize(frameCount);
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
		AVAudioFrameCount framesRead = static_cast<AVAudioFrameCount>(_audioRingBuffer.Read(outputData, framesToRead));
		if(framesRead != framesToRead)
			os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

		// ========================================
		// 4. If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
			os_log_debug(_audioPlayerNodeLog, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);

			auto framesOfSilence = frameCount - framesRead;
			auto byteCountToSkip = _audioRingBuffer.Format().FrameCountToByteSize(framesRead);
			auto byteCountToZero = _audioRingBuffer.Format().FrameCountToByteSize(framesOfSilence);
			for(UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
				std::memset(static_cast<int8_t *>(outputData->mBuffers[i].mData) + byteCountToSkip, 0, byteCountToZero);
				outputData->mBuffers[i].mDataByteSize += byteCountToZero;
			}
		}

		// ========================================
		// 5. If there is adequate space in the ring buffer for another chunk signal the decoding thread
		AVAudioFrameCount framesAvailableToWrite = static_cast<AVAudioFrameCount>(_audioRingBuffer.FramesAvailableToWrite());
		if(framesAvailableToWrite >= kRingBufferChunkSize)
			dispatch_semaphore_signal(_decodingSemaphore);

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

			if(!(decoderState->mFlags.load() & DecoderStateData::eRenderingStartedFlag)) {
				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingStartedFlag);

				// Schedule the rendering started notification
				const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted;
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(frameOffset / _audioRingBuffer.Format().mSampleRate);

				uint8_t bytesToWrite [4 + 8 + 8];
				std::memcpy(bytesToWrite, &cmd, 4);
				std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
				std::memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
				_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8 + 8);
				dispatch_source_merge_data(_renderEventsProcessor, 1);
			}

			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
			framesRemainingToDistribute -= framesFromThisDecoder;

			if((decoderState->mFlags.load() & DecoderStateData::eDecodingCompleteFlag) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingCompleteFlag);

				// Schedule the rendering complete notification
				const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete;
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(frameOffset / _audioRingBuffer.Format().mSampleRate);

				uint8_t bytesToWrite [4 + 8 + 8];
				std::memcpy(bytesToWrite, &cmd, 4);
				std::memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
				std::memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
				_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8 + 8);
				dispatch_source_merge_data(_renderEventsProcessor, 1);
			}

			if(framesRemainingToDistribute == 0)
				break;

			decoderState = GetActiveDecoderStateFollowingSequenceNumber(decoderState->mSequenceNumber);
		}

		// ========================================
		// 8. If there are no active decoders schedule the end of audio notification

		decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(!decoderState) {
			const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio;
			const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(framesRead / _audioRingBuffer.Format().mSampleRate);

			uint8_t bytesToWrite [4 + 8];
			std::memcpy(bytesToWrite, &cmd, 4);
			std::memcpy(bytesToWrite + 4, &hostTime, 8);
			_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8);
			dispatch_source_merge_data(_renderEventsProcessor, 1);
		}

		return noErr;
	}

#pragma mark - Format Information

	inline bool SupportsFormat(AVAudioFormat *format) const noexcept
	{
		// Gapless playback requires the same number of channels at the same sample rate as the ring buffer
		auto renderingFormat = _audioRingBuffer.Format();
		return format.channelCount == renderingFormat.ChannelCount() && format.sampleRate == renderingFormat.mSampleRate;
	}

#pragma mark Queue Management

	void CancelCurrentDecoder() noexcept
	{
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber();
		if(decoderState) {
			decoderState->mFlags.fetch_or(DecoderStateData::eCancelDecodingFlag);
			dispatch_semaphore_signal(_decodingSemaphore);
		}
	}

	void ClearQueue() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(_queueLock);
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	}

	bool QueueIsEmpty() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(_queueLock);
		return _queuedDecoders.empty();
	}

	id <SFBPCMDecoding> DequeueDecoder() noexcept
	{
		std::lock_guard<SFB::UnfairLock> lock(_queueLock);
		id <SFBPCMDecoding> decoder = nil;
		if(!_queuedDecoders.empty()) {
			decoder = _queuedDecoders.front();
			_queuedDecoders.pop();
		}
		return decoder;
	}

	void Reset() noexcept
	{
		ClearQueue();
		CancelCurrentDecoder();
	}

private:

	/// Returns the decoder with the smallest sequence number that has not completed rendering and has not been marked for removal
	DecoderStateData * GetActiveDecoderStateWithSmallestSequenceNumber() const noexcept
	{
		DecoderStateData *result = nullptr;
		for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
			auto decoderState = _decoderStateArray[i].load();
			if(!decoderState)
				continue;

			auto flags = decoderState->mFlags.load();
			if(flags & DecoderStateData::eMarkedForRemovalFlag || flags & DecoderStateData::eRenderingCompleteFlag)
				continue;

			if(!result)
				result = decoderState;
			else if(decoderState->mSequenceNumber < result->mSequenceNumber)
				result = decoderState;
		}

		return result;
	}

	/// Returns the decoder with the smallest sequence number greater than \c sequenceNumber that has not completed rendering and has not been marked for removal
	DecoderStateData * GetActiveDecoderStateFollowingSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		DecoderStateData *result = nullptr;
		for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
			auto decoderState = _decoderStateArray[i].load();
			if(!decoderState)
				continue;

			auto flags = decoderState->mFlags.load();
			if(flags & DecoderStateData::eMarkedForRemovalFlag || flags & DecoderStateData::eRenderingCompleteFlag)
				continue;

			if(!result && decoderState->mSequenceNumber > sequenceNumber)
				result = decoderState;
			else if(result && decoderState->mSequenceNumber > sequenceNumber && decoderState->mSequenceNumber < result->mSequenceNumber)
				result = decoderState;
		}

		return result;
	}

	/// Returns the decoder with the sequence number equal to \c sequenceNumber that has not been marked for removal
	DecoderStateData * GetDecoderStateWithSequenceNumber(const uint64_t& sequenceNumber) const noexcept
	{
		for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
			auto decoderState = _decoderStateArray[i].load();
			if(!decoderState)
				continue;

			if(decoderState->mFlags.load() & DecoderStateData::eMarkedForRemovalFlag)
				continue;

			if(decoderState->mSequenceNumber == sequenceNumber)
				return decoderState;
		}

		return nullptr;
	}

#pragma mark - Decoding

	void DecoderThreadEntry() noexcept
	{
		pthread_setname_np("org.sbooth.AudioEngine.AudioPlayerNode.DecoderThread");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

		os_log_debug(_audioPlayerNodeLog, "Decoder thread starting");

		AVAudioFormat *renderingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&_audioRingBuffer.Format()];

		while(!(_flags.load() & eAudioPlayerNodeFlagStopDecoderThread)) {
			// Dequeue and process the next decoder
			id <SFBPCMDecoding> decoder = DequeueDecoder();
			if(decoder) {
				// Create the decoder state
				auto decoderState = new (std::nothrow) DecoderStateData(decoder, renderingFormat, kRingBufferChunkSize);
				if(!decoderState) {
					os_log_error(_audioPlayerNodeLog, "Unable to allocate decoder state data");
					if([_node.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)])
						dispatch_async_and_wait(_notificationQueue, ^{
							[_node.delegate audioPlayerNode:_node encounteredError:[NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil]];
						});
					continue;
				}

				// Add the decoder state to the list of active decoders
				auto stored = false;
				do {
					for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
						auto current = _decoderStateArray[i].load();
						if(current)
							continue;

						// In essence _decoderStateArray is an SPSC queue with this thread as producer
						// and the collector as consumer, with the stored values used in between production
						// and consumption by any number of other threads/queues including the IOProc.
						//
						// Slots in _decoderStateArray are assigned values in two places: here and the
						// collector. The collector assigns nullptr to slots holding existing non-null
						// values marked for removal while this code assigns non-null values to slots
						// holding nullptr.
						// Since _decoderStateArray[i] was atomically loaded and has been verified not null,
						// it is safe to use store() instead of compare_exchange_strong() because this is the
						// only code that could have changed the slot to a non-null value and it is called solely
						// from the decoding thread.
						// There is the possibility that a non-null value was collected from the slot and the slot
						// was assigned nullptr in between load() and the check for null. If this happens the
						// assignment could have taken place but didn't.
						//
						// When _decoderStateArray is full this code either needs to wait for a slot to open up or fail.
						//
						// _decoderStateArray may be full when the capacity of _audioRingBuffer exceeds the
						// total number of audio frames for all the decoders in _decoderStateArray and audio is not
						// being consumed by the IOProc.
						// The default frame capacity for _audioRingBuffer is 16384. With 8 slots available in
						// _decoderStateArray, the average number of frames a decoder needs to contain for
						// all slots to be full is 2048. For audio at 8000 Hz that equates to 0.26 sec and at
						// 44,100 Hz 2048 frames equates to 0.05 sec.
						// This code elects to wait for a slot to open up instead of failing.
						// This isn't a concern in practice since the main use case for this class is music, not
						// sequential buffers of 0.05 sec. In normal use it's expected that slots 0 and 1 will
						// be the only ones used.
						_decoderStateArray[i].store(decoderState);
						stored = true;
						break;
					}

					if(!stored) {
						os_log_debug(_audioPlayerNodeLog, "No open slots in _decoderStateArray");
						struct timespec sleepyTime = {
							.tv_sec = 0,
							.tv_nsec = NSEC_PER_SEC / 20
						};
						nanosleep(&sleepyTime, nullptr);
					}
				} while(!stored);

				// In the event the render block output format and decoder processing
				// format don't match, conversion will be performed in DecoderStateData::DecodeAudio()

				os_log_debug(_audioPlayerNodeLog, "Dequeued decoder for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
				os_log_debug(_audioPlayerNodeLog, "Processing format: %{public}@", decoderState->mDecoder.processingFormat);

				AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:renderingFormat frameCapacity:kRingBufferChunkSize];

				while(!(_flags.load() & eAudioPlayerNodeFlagStopDecoderThread)) {
					// If a seek is pending reset the ring buffer
					if(decoderState->mFrameToSeek.load() != kInvalidFramePosition)
						_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);

					// Reset the ring buffer if required, to prevent audible artifacts
					if(_flags.load() & eAudioPlayerNodeFlagRingBufferNeedsReset) {
						_flags.fetch_and(~eAudioPlayerNodeFlagRingBufferNeedsReset);

						// Ensure output is muted before performing operations that aren't thread safe
						if(_node.engine.isRunning) {
							_flags.fetch_or(eAudioPlayerNodeFlagMuteRequested);

							// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
							while(_flags.load() & eAudioPlayerNodeFlagMuteRequested)
								dispatch_semaphore_wait(_decodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 100));
						}
						else
							_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);

						// Perform seek if one is pending
						if(decoderState->mFrameToSeek.load() != kInvalidFramePosition)
							decoderState->PerformSeek();

						// Reset() is not thread safe but the rendering thread is outputting silence
						_audioRingBuffer.Reset();

						// Clear the mute flag
						_flags.fetch_and(~eAudioPlayerNodeFlagOutputIsMuted);
					}

					// Determine how many frames are available in the ring buffer
					auto framesAvailableToWrite = _audioRingBuffer.FramesAvailableToWrite();

					// Force writes to the ring buffer to be at least kRingBufferChunkSize
					if(framesAvailableToWrite >= kRingBufferChunkSize && !(decoderState->mFlags.load() & DecoderStateData::eCancelDecodingFlag)) {
						if(!(decoderState->mFlags.load() & DecoderStateData::eDecodingStartedFlag)) {
							os_log_debug(_audioPlayerNodeLog, "Decoding started for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

							decoderState->mFlags.fetch_or(DecoderStateData::eDecodingStartedFlag);

							// Perform the decoding started notification
							if([_node.delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)])
								dispatch_async_and_wait(_notificationQueue, ^{
									[_node.delegate audioPlayerNode:_node decodingStarted:decoderState->mDecoder];
								});
						}

						// Decode audio into the buffer, converting to the bus format in the process
						NSError *error = nil;
						if(!decoderState->DecodeAudio(buffer, &error)) {
							os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);
							if(error && [_node.delegate respondsToSelector:@selector(audioPlayerNode:encounteredError:)])
								dispatch_async_and_wait(_notificationQueue, ^{
									[_node.delegate audioPlayerNode:_node encounteredError:error];
								});
						}

						// Write the decoded audio to the ring buffer for rendering
						auto framesWritten = _audioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
						if(framesWritten != buffer.frameLength)
							os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Write() failed");

						if(decoderState->mFlags.load() & DecoderStateData::eDecodingCompleteFlag) {
							// Some formats (MP3) may not know the exact number of frames in advance
							// without processing the entire file, which is a potentially slow operation
							decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

							// Perform the decoding complete notification
							if([_node.delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)])
								dispatch_async_and_wait(_notificationQueue, ^{
									[_node.delegate audioPlayerNode:_node decodingComplete:decoderState->mDecoder];
								});

							os_log_debug(_audioPlayerNodeLog, "Decoding complete for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

							break;
						}
					}
					else if(decoderState->mFlags.load() & DecoderStateData::eCancelDecodingFlag) {
						os_log_debug(_audioPlayerNodeLog, "Canceling decoding for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						BOOL partiallyRendered = (decoderState->mFlags.load() & DecoderStateData::eRenderingStartedFlag) ? YES : NO;
						id<SFBPCMDecoding> canceledDecoder = decoderState->mDecoder;

						_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);
						decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);
						dispatch_source_merge_data(_collector, 1);

						// Perform the decoding cancelled notification
						if([_node.delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:partiallyRendered:)])
							dispatch_async_and_wait(_notificationQueue, ^{
								[_node.delegate audioPlayerNode:_node decodingCanceled:canceledDecoder partiallyRendered:partiallyRendered];
							});

						break;
					}
					// Wait for additional space in the ring buffer
					else
						dispatch_semaphore_wait(_decodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10));
				}
			}
			// Wait for another decoder to be enqueued
			else
				dispatch_semaphore_wait(_decodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));
		}

		os_log_debug(_audioPlayerNodeLog, "Decoder thread terminating");
	}
};

}

#pragma mark -

@interface SFBAudioPlayerNode ()
{
@private
	PlayerNodeImpl::unique_ptr _impl;
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
	return [self initWithFormat:format ringBufferSize:kRingBufferFrameCapacity];
}

- (instancetype)initWithFormat:(AVAudioFormat *)format ringBufferSize:(uint32_t)ringBufferSize
{
	NSParameterAssert(format != nil);
	NSParameterAssert(format.isStandard);

	std::unique_ptr<PlayerNodeImpl> impl;

	try {
		impl = std::make_unique<PlayerNodeImpl>(format, ringBufferSize);
	}

	catch(const std::exception& e) {
		return nil;
	}

	auto ptr = impl.get();
	AVAudioSourceNodeRenderBlock renderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {
		return ptr->Render(isSilence, timestamp, frameCount, outputData);
	};

	if((self = [super initWithFormat:format renderBlock:renderBlock])) {
		os_log_info(_audioPlayerNodeLog, "Render block format: %{public}@", format);

		_impl = std::move(impl);
		_impl->_node = self;
		_renderingFormat = format;

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

- (id <SFBPCMDecoding>)dequeueDecoder
{
	return _impl->DequeueDecoder();
}

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

#pragma mark - Player State

- (BOOL)isPlaying
{
	return _impl->IsPlaying();
}

- (BOOL)isReady
{
	return _impl->IsReady();
}

- (id<SFBPCMDecoding>)currentDecoder
{
	return _impl->CurrentDecoder();
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
