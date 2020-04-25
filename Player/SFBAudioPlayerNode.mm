/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <array>
#import <atomic>
#import <queue>
#import <thread>

#import <os/log.h>
#if DEBUG
#import <mach/mach_time.h>
#endif

#import "SFBAudioPlayerNode.h"

#import "AudioRingBuffer.h"
#import "SFBAudioDecoder.h"

@interface SFBAudioPlayerNode ()
- (void *)decoderThreadEntry;
- (void *)notifierThreadEntry;
@end

namespace {

#pragma mark - Flags

	enum eAudioPlayerNodeFlags : unsigned int {
		eAudioPlayerNodeFlagIsPlaying					= 1u << 0,
		eAudioPlayerNodeFlagOutputIsMuted				= 1u << 1,
		eAudioPlayerNodeFlagMuteRequested				= 1u << 2,
		eAudioPlayerNodeFlagRingBufferNeedsReset		= 1u << 3,
		eAudioPlayerNodeFlagStopDecoderThread			= 1u << 4,
		eAudioPlayerNodeFlagStopNotifierThread			= 1u << 5
	};

	enum eAudioPlayerNodeNotifierFlags : unsigned int {
		eAudioPlayerNodeNotifierFlagRenderingStarted	= 1u << 1,
		eAudioPlayerNodeNotifierFlagRenderingFinished	= 1u << 2
	};

#pragma mark - Thread entry points

	void * DecoderThreadEntry(void *arg)
	{
		pthread_setname_np("org.sbooth.AudioEngine.PlayerNode.DecoderThread");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)arg;
		return [playerNode decoderThreadEntry];
	}

	void * NotifierThreadEntry(void *arg)
	{
		pthread_setname_np("org.sbooth.AudioEngine.PlayerNode.NotifierThread");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)arg;
		return [playerNode notifierThreadEntry];
	}

#pragma mark - Constants

	const AVAudioFrameCount 	kRingBufferFrameCapacity 	= 16384;
	const AVAudioFrameCount 	kRingBufferChunkSize 		= 2048;
	const size_t 				kDecoderArraySize 			= 10;

#pragma mark - Decoder State

	//! State data for tracking/syncing decoding progress
	struct DecoderStateData {
		using shared_ptr = std::shared_ptr<DecoderStateData>;

		static const AVAudioFrameCount kDefaultBufferSize = 1024;

		enum eDecoderStateDataFlags : unsigned int {
			eStopDecodingFlag		= 1u << 0,
			eDecodingStartedFlag	= 1u << 1,
			eDecodingFinishedFlag	= 1u << 2,
			eRenderingStartedFlag	= 1u << 3,
			eRenderingFinishedFlag	= 1u << 4,
			eMarkedForRemovalFlag 	= 1u << 5
		};

		const int64_t			mSequenceNumber;	//!< Monotonically increasing instance counter

		std::atomic_uint 		mFlags; 			//!< Decoder state data flags
		std::atomic_int64_t 	mFramesDecoded; 	//!< The number of frames decoded in the converter's *input* sample rate
		std::atomic_int64_t 	mFramesConverted;	//!< The number of frames converted in the converter's *output* sample rate
		std::atomic_int64_t 	mFramesRendered;	//!< The number of frames rendered in the converter's *output* sample rate
		std::atomic_int64_t 	mFrameLength;		//!< The total number of audio frames, in the decoder's sample rate
		std::atomic_int64_t 	mFrameToSeek;		//!< The desired seek offset, in the converter's *output* sample rate

		std::atomic_uint64_t 	mStartHostTime;		//!< The timestamp when the first frame should be rendered

//	private:
		id <SFBPCMDecoding> 	mDecoder; 			//!< Decodes audio from the source representation to PCM
		AVAudioConverter 		*mConverter;		//!< Converts audio from the decoder's processing format to PCM
	private:
		AVAudioPCMBuffer 		*mDecodeBuffer;		//!< Buffer used internally for buffering during conversion
		static int64_t			sSequenceNumber; 	//!< Next sequence number to use

	public:
		DecoderStateData(id <SFBPCMDecoding> decoder, AVAudioFormat *format, AVAudioFrameCount frameCapacity = kDefaultBufferSize)
			: mSequenceNumber(sSequenceNumber++), mFlags(0), mFramesDecoded(0), mFramesConverted(0), mFramesRendered(0), mFrameLength(decoder.frameLength), mFrameToSeek(-1), mStartHostTime(0), mDecoder(decoder), mConverter(nil), mDecodeBuffer(nil)
		{
			mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
			AVAudioFrameCount conversionBufferLength = (AVAudioFrameCount)((mConverter.inputFormat.sampleRate / mConverter.outputFormat.sampleRate) * frameCapacity);
			mDecodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.inputFormat frameCapacity:conversionBufferLength];
		}

		inline AVAudioFramePosition FramePosition() const
		{
			int64_t seek = mFrameToSeek.load();
			int64_t rendered = mFramesRendered.load();
			return seek == -1 ? rendered : seek;
		}

		inline AVAudioFramePosition FrameLength() const
		{
			double inputSampleRate = mConverter.inputFormat.sampleRate;
			double outputSampleRate = mConverter.outputFormat.sampleRate;
			if(inputSampleRate == outputSampleRate)
				return mFrameLength.load();
			else
				return (AVAudioFramePosition)(mFrameLength.load() * outputSampleRate / inputSampleRate);
		}

		bool DecodeAudio(AVAudioPCMBuffer *buffer, NSError **error = nullptr)
		{
			__block NSError *err = nil;
			AVAudioConverterOutputStatus status = [mConverter convertToBuffer:buffer error:error withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
				if(!(mFlags.load() & eDecodingStartedFlag))
					mFlags.fetch_or(eDecodingStartedFlag);

				BOOL result = [mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:inNumberOfPackets error:&err];
				if(!result && err)
					os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);

				this->mFramesDecoded.fetch_add(mDecodeBuffer.frameLength);

				if(result && mDecodeBuffer.frameLength == 0) {
					mFlags.fetch_or(eDecodingFinishedFlag);
					*outStatus = AVAudioConverterInputStatus_EndOfStream;
				}
				else
					*outStatus = AVAudioConverterInputStatus_HaveData;

				return mDecodeBuffer;
			}];

			mFramesConverted.fetch_add(buffer.frameLength);

			if(status == AVAudioConverterOutputStatus_Error) {
				if(error)
					*error = err;
				return false;
			}

			return true;
		}

		inline bool SeekRequested() const
		{
			return mFrameToSeek.load() != -1;
		}

		inline void RequestSeekToFrame(AVAudioFramePosition frame)
		{
			mFrameToSeek.store(frame);
		}

		//! Seeks to the desired frame  in the converter's *output* sample rate
		bool PerformSeek()
		{
			AVAudioFramePosition seekOffset = mFrameToSeek.load();

			double sampleRateRatio = mConverter.inputFormat.sampleRate / mConverter.outputFormat.sampleRate;
			AVAudioFramePosition adjustedSeekOffset = (AVAudioFramePosition)(seekOffset * sampleRateRatio);

			if(adjustedSeekOffset == seekOffset)
				os_log_debug(OS_LOG_DEFAULT, "Seeking to frame %lld", adjustedSeekOffset);
			else
				os_log_debug(OS_LOG_DEFAULT, "Seek to frame %lld requested, actually seeking to frame %lld", seekOffset, adjustedSeekOffset);

			if([mDecoder seekToFrame:adjustedSeekOffset error:nil])
				// Reset the converter to flush any buffers
				[mConverter reset];
			else
				os_log_debug(OS_LOG_DEFAULT, "Error seeking to frame %lld", adjustedSeekOffset);

			AVAudioFramePosition newFrame = mDecoder.framePosition;
			if(newFrame != adjustedSeekOffset) {
				os_log_debug(OS_LOG_DEFAULT, "Inaccurate seek to frame %lld, got %lld", adjustedSeekOffset, newFrame);
				seekOffset = (AVAudioFramePosition)(newFrame / sampleRateRatio);
			}

			// Update the seek request
			mFrameToSeek.exchange(-1);

			// Update the frame counters accordingly
			// A seek is handled in essentially the same way as initial playback
			if(newFrame != -1) {
				mFramesDecoded.exchange(newFrame);
				mFramesConverted.exchange(seekOffset);
				mFramesRendered.exchange(seekOffset);
			}

			return newFrame != -1;
		}

		inline bool DecodingFinished() const
		{
			return mFlags.load() & eDecodingFinishedFlag;
		}

		inline bool RenderingFinished() const
		{
			return mFlags.load() & eRenderingFinishedFlag;
		}

	};

	int64_t DecoderStateData::sSequenceNumber = 0;
	using DecoderQueue = std::queue<id <SFBPCMDecoding>>;

	/// Returns the element in `decoders` with the smallest sequence number that has not finished rendering
	template<size_t N>
	DecoderStateData::shared_ptr GetActiveDecoderStateWithSmallestSequenceNumber(const std::array<DecoderStateData::shared_ptr, N>& decoderStateArray)
	{
		DecoderStateData::shared_ptr result;
		for(size_t i = 0; i < decoderStateArray.size(); ++i) {
			auto decoderState = std::atomic_load(&decoderStateArray[i]);
			if(!decoderState)
				continue;

			auto flags = decoderState->mFlags.load();
			if(flags & DecoderStateData::eMarkedForRemovalFlag || flags & DecoderStateData::eRenderingFinishedFlag)
				continue;

			if(!result)
				result = decoderState;
			else if(decoderState->mSequenceNumber < result->mSequenceNumber)
				result = decoderState;
		}

		return result;
	}

	/// Returns the element in `decoders` with the smallest sequence number has finished rendering
	template<size_t N>
	DecoderStateData::shared_ptr GetInactiveDecoderStateWithSmallestSequenceNumber(const std::array<DecoderStateData::shared_ptr, N>& decoderStateArray)
	{
		DecoderStateData::shared_ptr result;
		for(size_t i = 0; i < decoderStateArray.size(); ++i) {
			auto decoderState = std::atomic_load(&decoderStateArray[i]);
			if(!decoderState)
				continue;

			auto flags = decoderState->mFlags.load();
			if(flags & DecoderStateData::eMarkedForRemovalFlag || !(flags & DecoderStateData::eRenderingFinishedFlag))
				continue;

			if(!result)
				result = decoderState;
			else if(decoderState->mSequenceNumber < result->mSequenceNumber)
				result = decoderState;
		}

		return result;
	}

	/// Returns the element in `decoders` with the smallest sequence number greater than `sequenceNumber` that has not finished rendering
	template<size_t N>
	DecoderStateData::shared_ptr GetActiveDecoderStateFollowingSequenceNumber(const std::array<DecoderStateData::shared_ptr, N>& decoderStateArray, const int64_t& sequenceNumber)
	{
		DecoderStateData::shared_ptr result;
		for(size_t i = 0; i < decoderStateArray.size(); ++i) {
			auto decoderState = std::atomic_load(&decoderStateArray[i]);
			if(!decoderState)
				continue;

			auto flags = decoderState->mFlags.load();
			if(flags & DecoderStateData::eMarkedForRemovalFlag || flags & DecoderStateData::eRenderingFinishedFlag)
				continue;

			if(!result && decoderState->mSequenceNumber > sequenceNumber)
				result = decoderState;
			else if(result && decoderState->mSequenceNumber > sequenceNumber && decoderState->mSequenceNumber < result->mSequenceNumber)
				result = decoderState;
		}

		return result;
	}

#if DEBUG
	inline uint64_t ConvertHostTimeToNanos(uint64_t t)
	{
		static mach_timebase_info_data_t timebase_info;
		mach_timebase_info(&timebase_info);
		return (t * timebase_info.numer) / timebase_info.denom;
	}
#endif

}

#pragma mark -

@interface SFBAudioPlayerNode ()
{
@private
	// Decoding
	std::atomic_uint 		_flags;
	std::thread 			_decoderThread;
	dispatch_semaphore_t	_decoderSemaphore;
	dispatch_queue_t		_queue;				//!< The dispatch queue used to access `_queuedDecoders`
	DecoderQueue 			_queuedDecoders;	//!< AudioDecoders enqueued for playback
	SFB::Audio::RingBuffer	_ringBuffer;

	// Notification
	std::thread 			_notifierThread;
	dispatch_queue_t		_notificationQueue;
	dispatch_semaphore_t	_notifierSemaphore;
	std::atomic_uint 		_notifierFlags;
	std::atomic_uint64_t	_notifierEventTimestamp;

	// Collector
	dispatch_source_t		_collector;

	// State
	std::array<DecoderStateData::shared_ptr, kDecoderArraySize> _decoderStateArray;
}
@end

@implementation SFBAudioPlayerNode

- (instancetype)initWithFormat:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);

	AVAudioSourceNodeRenderBlock renderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {

		// ========================================
		// Pre-rendering actions

		// ========================================
		// 0. Mute output if requested
		if(self->_flags.load() & eAudioPlayerNodeFlagMuteRequested) {
			self->_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);
			self->_flags.fetch_and(~eAudioPlayerNodeFlagMuteRequested);
			dispatch_semaphore_signal(self->_decoderSemaphore);
		}

		// ========================================
		// Rendering

		// ========================================
		// 1. Determine how many audio frames are available to read in the ring buffer
		AVAudioFrameCount framesAvailableToRead = (AVAudioFrameCount)self->_ringBuffer.GetFramesAvailableToRead();

		// ========================================
		// 2. Output silence if a) the node isn't playing, b) the node is muted, or c) the ring buffer is empty
		if(!(self->_flags.load() & eAudioPlayerNodeFlagIsPlaying) || self->_flags.load() & eAudioPlayerNodeFlagOutputIsMuted || framesAvailableToRead == 0) {
			size_t byteCountToZero = self->_ringBuffer.GetFormat().FrameCountToByteCount(frameCount);
			for(UInt32 bufferIndex = 0; bufferIndex < outputData->mNumberBuffers; ++bufferIndex) {
				memset(outputData->mBuffers[bufferIndex].mData, self->_ringBuffer.GetFormat().IsDSD() ? 0xF : 0, byteCountToZero);
				outputData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
			}

			*isSilence = YES;
			return noErr;
		}

		// ========================================
		// 3. Read as many frames as available from the ring buffer
		AVAudioFrameCount framesToRead = std::min(framesAvailableToRead, frameCount);
		AVAudioFrameCount framesRead = (AVAudioFrameCount)self->_ringBuffer.Read(outputData, framesToRead);
		if(framesRead != framesToRead)
			os_log_error(OS_LOG_DEFAULT, "SFB::Audio::RingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

		// ========================================
		// 4. If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
			os_log_debug(OS_LOG_DEFAULT, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);

			auto framesOfSilence = frameCount - framesRead;
			auto byteCountToSkip = self->_ringBuffer.GetFormat().FrameCountToByteCount(framesRead);
			auto byteCountToZero = self->_ringBuffer.GetFormat().FrameCountToByteCount(framesOfSilence);
			for(UInt32 bufferIndex = 0; bufferIndex < outputData->mNumberBuffers; ++bufferIndex) {
				memset((int8_t *)outputData->mBuffers[bufferIndex].mData + byteCountToSkip, self->_ringBuffer.GetFormat().IsDSD() ? 0xF : 0, byteCountToZero);
			}
		}

		// ========================================
		// 5. If there is adequate space in the ring buffer for another chunk signal the decoding thread
		AVAudioFrameCount framesAvailableToWrite = (AVAudioFrameCount)self->_ringBuffer.GetFramesAvailableToWrite();
		if(framesAvailableToWrite >= kRingBufferChunkSize)
			dispatch_semaphore_signal(self->_decoderSemaphore);

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

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(self->_decoderStateArray);
		while(decoderState) {
			auto sequenceNumber = decoderState->mSequenceNumber;

			AVAudioFrameCount decoderFramesRemaining = (AVAudioFrameCount)(decoderState->mFramesConverted.load() - decoderState->mFramesRendered.load());
			AVAudioFrameCount framesFromThisDecoder = std::min(decoderFramesRemaining, framesRead);

			if(!(decoderState->mFlags.load() & DecoderStateData::eRenderingStartedFlag)) {
				// Schedule the rendering started notification
				self->_notifierEventTimestamp.store(timestamp->mHostTime);
				self->_notifierFlags.fetch_or(eAudioPlayerNodeNotifierFlagRenderingStarted);
				dispatch_semaphore_signal(self->_notifierSemaphore);

				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingStartedFlag);
				decoderState->mStartHostTime.store(timestamp->mHostTime);
			}

			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);

			if((decoderState->mFlags.load() & DecoderStateData::eDecodingFinishedFlag) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()/* && !(eDecoderStateDataFlagRenderingFinished & decoderState->mFlags.load())*/) {
				// Schedule the rendering finished notification
				self->_notifierEventTimestamp.store(timestamp->mHostTime);
				self->_notifierFlags.fetch_or(eAudioPlayerNodeNotifierFlagRenderingFinished);
				dispatch_semaphore_signal(self->_notifierSemaphore);

				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingFinishedFlag);
//				decoderState = nullptr;
			}

			framesRemainingToDistribute -= framesFromThisDecoder;

			if(!framesRemainingToDistribute)
				break;

			decoderState = GetActiveDecoderStateFollowingSequenceNumber(self->_decoderStateArray, sequenceNumber);
		}

		return noErr;
	};

	if((self = [super initWithFormat:format renderBlock:renderBlock])) {
#if 0
		// See the comments in SFBAudioPlayer -setupEngineForGaplessPlaybackOfFormat:
		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice while 1156 is AVAudioSourceNode's default
		AVAudioFrameCount maximumFramesToRender = (AVAudioFrameCount)ceil(512 * (format.sampleRate / 44100));
		if(self.AUAudioUnit.maximumFramesToRender < maximumFramesToRender) {
			os_log_debug(OS_LOG_DEFAULT, "SFBAudioPlayerNode: Setting maximumFramesToRender to %u", maximumFramesToRender);
			self.AUAudioUnit.maximumFramesToRender = maximumFramesToRender;
		}
#endif
		_queue = dispatch_queue_create("org.sbooth.AudioEngine.PlayerNode.DecoderQueueAccessQueue", DISPATCH_QUEUE_SERIAL);
		if(!_queue) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		_notificationQueue = dispatch_queue_create("org.sbooth.AudioEngine.PlayerNode.NotificationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_notificationQueue) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		_decoderSemaphore = dispatch_semaphore_create(0);
		if(!_decoderSemaphore) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_semaphore_create failed");
			return nil;
		}

		_notifierSemaphore = dispatch_semaphore_create(0);
		if(!_notifierSemaphore) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_semaphore_create failed");
			return nil;
		}

		// Setup the collector
		_collector = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0));
		if(!_collector) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_source_create failed");
			return nil;
		}

		dispatch_source_set_timer(_collector, DISPATCH_TIME_NOW, NSEC_PER_SEC * 10, NSEC_PER_SEC * 2);
		dispatch_source_set_event_handler(_collector, ^{
			for(size_t i = 0; i < self->_decoderStateArray.size(); ++i) {
				auto decoderState = std::atomic_load(&self->_decoderStateArray[i]);
				if(!decoderState || !(decoderState->mFlags.load() & DecoderStateData::eMarkedForRemovalFlag))
					continue;

				os_log_debug(OS_LOG_DEFAULT, "Collecting decoder \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
				std::atomic_store(&self->_decoderStateArray[i], DecoderStateData::shared_ptr{});
			}
		});

		// Start collecting
		dispatch_resume(_collector);

		// Launch the threads
		try {
			_decoderThread = std::thread(DecoderThreadEntry, (__bridge void *)self);
			_notifierThread = std::thread(NotifierThreadEntry, (__bridge void *)self);
		}

		catch(const std::exception& e) {
			os_log_error(OS_LOG_DEFAULT, "Unable to create thread: %{public}s", e.what());
			return nil;
		}

		_renderingFormat = format;
		_ringBuffer.Allocate(_renderingFormat.streamDescription, kRingBufferFrameCapacity);
	}

	return self;
}

- (void)dealloc
{
	_flags.fetch_or(eAudioPlayerNodeFlagStopDecoderThread | eAudioPlayerNodeFlagStopNotifierThread);
	dispatch_semaphore_signal(_decoderSemaphore);
	dispatch_semaphore_signal(_notifierSemaphore);
	_decoderThread.join();
	_notifierThread.join();
	while(!_queuedDecoders.empty())
		_queuedDecoders.pop();
}

#pragma mark - Format Information

- (BOOL)supportsFormat:(AVAudioFormat *)format
{
	// Gapless playback requires the same number of channels at the same sample rate
	return format.channelCount == _renderingFormat.channelCount && format.sampleRate == _renderingFormat.sampleRate;
}

#pragma mark - Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self playDecoder:decoder error:error];
}

- (BOOL)playDecoder:(id<SFBPCMDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	if(![self supportsFormat:decoder.processingFormat])
		return NO;

	[self clearQueue];

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(decoderState)
		decoderState->mFlags.fetch_or(DecoderStateData::eStopDecodingFlag);

	dispatch_sync(_queue, ^{
		_queuedDecoders.push(decoder);
	});
	dispatch_semaphore_signal(_decoderSemaphore);

	return YES;

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

	if(![self supportsFormat:decoder.processingFormat])
		return NO;

	dispatch_sync(_queue, ^{
		_queuedDecoders.push(decoder);
	});

	return YES;
}

- (void)skipToNext
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(decoderState) {
		decoderState->mFlags.fetch_or(DecoderStateData::eStopDecodingFlag);
		dispatch_semaphore_signal(_decoderSemaphore);
	}
}

- (void)clearQueue
{
	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});
}

- (BOOL)queueIsEmpty
{
	__block bool empty = true;
	dispatch_sync(_queue, ^{
		empty = _queuedDecoders.empty();
	});
	return empty;
}

- (void)reset
{
	[super reset];

	[self clearQueue];

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(decoderState) {
		decoderState->mFlags.fetch_or(DecoderStateData::eStopDecodingFlag);
		dispatch_semaphore_signal(_decoderSemaphore);
	}
}

#pragma mark - Playback Control

- (void)play
{
	_flags.fetch_or(eAudioPlayerNodeFlagIsPlaying);
}

- (void)pause
{
	_flags.fetch_and(~eAudioPlayerNodeFlagIsPlaying);
}

- (void)playPause
{
	_flags.fetch_xor(eAudioPlayerNodeFlagIsPlaying);
}

- (void)stop
{
	_flags.fetch_and(~eAudioPlayerNodeFlagIsPlaying);
	[self reset];
}

#pragma mark - Player State

- (BOOL)isPlaying
{
	return (_flags.load() & eAudioPlayerNodeFlagIsPlaying) != 0;
}

- (NSURL *)url
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	return decoderState ? decoderState->mDecoder.inputSource.url : nil;
}

- (id<SFBPCMDecoding>)decoder
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	return decoderState ? decoderState->mDecoder : nil;
}

#pragma mark - Playback Properties

- (SFBAudioPlayerNodePlaybackPosition)playbackPosition
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return { .framePosition = -1, .frameLength = -1 };

	return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
}

- (SFBAudioPlayerNodePlaybackTime)playbackTime
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return { .currentTime = -1, .totalTime = -1 };

	int64_t framePosition = decoderState->FramePosition();
	int64_t frameLength = decoderState->FrameLength();
	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	return { .currentTime = framePosition / sampleRate, .totalTime = frameLength / sampleRate };
}

- (BOOL)getPlaybackPosition:(SFBAudioPlayerNodePlaybackPosition *)playbackPosition andTime:(SFBAudioPlayerNodePlaybackTime *)playbackTime
{
	SFBAudioPlayerNodePlaybackPosition currentPlaybackPosition = { .framePosition = -1, .frameLength = -1 };
	SFBAudioPlayerNodePlaybackTime currentPlaybackTime = { .currentTime = -1, .totalTime = -1 };

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(decoderState) {
		currentPlaybackPosition = { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		currentPlaybackTime = { .currentTime = currentPlaybackPosition.framePosition / sampleRate, .totalTime = currentPlaybackPosition.frameLength / sampleRate };
	}

	if(playbackPosition)
		*playbackPosition = currentPlaybackPosition;
	if(playbackTime)
		*playbackTime = currentPlaybackTime;

	return decoderState != nullptr;
}

#pragma mark - Seeking

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	if(secondsToSkip <= 0)
		return NO;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition framePosition = decoderState->FramePosition();
	AVAudioFramePosition targetFrame = framePosition + (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame >= decoderState->FrameLength())
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	if(secondsToSkip <= 0)
		return NO;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition framePosition = decoderState->FramePosition();
	AVAudioFramePosition targetFrame = framePosition - (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame < 0)
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	if(timeInSeconds < 0)
		return NO;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition targetFrame = (AVAudioFramePosition)(timeInSeconds * sampleRate);

	if(targetFrame >= decoderState->FrameLength())
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToPosition:(float)position
{
	if(position < 0 || position >= 1)
		return NO;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return NO;

	AVAudioFramePosition frameLength = decoderState->FrameLength();
	return [self seekToFrame:(AVAudioFramePosition)(frameLength * position)];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	if(frame < 0)
		return NO;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	if(!decoderState)
		return NO;

	if(frame >= decoderState->FrameLength())
		return NO;

	decoderState->RequestSeekToFrame(frame);
	dispatch_semaphore_signal(_decoderSemaphore);

	return YES;

}

- (BOOL)supportsSeeking
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray);
	return decoderState ? decoderState->mDecoder.supportsSeeking : NO;
}


#pragma mark - Internals

- (void *)decoderThreadEntry
{
	os_log_debug(OS_LOG_DEFAULT, "Decoder thread starting");

	while(!(_flags.load() & eAudioPlayerNodeFlagStopDecoderThread)) {
		// Dequeue and process the next decoder
		__block id <SFBPCMDecoding> decoder = nil;
		dispatch_sync(_queue, ^{
			if(!_queuedDecoders.empty()) {
				decoder = _queuedDecoders.front();
				_queuedDecoders.pop();
			}
		});

		if(decoder) {
			// Create the decoder state
			auto decoderState = std::make_shared<DecoderStateData>(decoder, self->_renderingFormat, kRingBufferChunkSize);

			// Append the decoder state to the list of active decoders
			for(size_t i = 0; i < self->_decoderStateArray.size(); ++i) {
				auto current = std::atomic_load(&self->_decoderStateArray[i]);
				if(current)
					continue;

				std::atomic_store(&self->_decoderStateArray[i], decoderState);
				break;
			}

			// In the event the render block output format and decoder processing
			// format don't match, conversion will be performed in DecoderStateData::DecodeAudio()

			os_log_debug(OS_LOG_DEFAULT, "Decoding starting for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
			os_log_debug(OS_LOG_DEFAULT, "Decoder processing format: %{public}@", decoderState->mDecoder.processingFormat);
			os_log_debug(OS_LOG_DEFAULT, "Render block format: %{public}@", _renderingFormat);

			AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:self->_renderingFormat frameCapacity:kRingBufferChunkSize];

			while(!(_flags.load() & eAudioPlayerNodeFlagStopDecoderThread)) {
				// Reset the ring buffer if required, to prevent audible artifacts
				if(_flags.load() & eAudioPlayerNodeFlagRingBufferNeedsReset) {
					_flags.fetch_and(~eAudioPlayerNodeFlagRingBufferNeedsReset);

					// Ensure output is muted before performing operations that aren't thread safe
					if(self.engine.isRunning) {
						_flags.fetch_or(eAudioPlayerNodeFlagMuteRequested);

						// The rendering thread will clear eAudioPlayerFlagRequestMute when the current render cycle completes
						while(_flags.load() & eAudioPlayerNodeFlagMuteRequested)
							dispatch_semaphore_wait(_decoderSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 100));
					}
					else
						_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);

					// Perform seek if one is pending
					if(decoderState->SeekRequested())
						decoderState->PerformSeek();

					// Reset() is not thread safe but the rendering thread is outputting silence
					_ringBuffer.Reset();

					// Clear the mute flag
					_flags.fetch_and(~eAudioPlayerNodeFlagOutputIsMuted);
				}

				// Determine how many frames are available in the ring buffer
				auto framesAvailableToWrite = _ringBuffer.GetFramesAvailableToWrite();

				// Force writes to the ring buffer to be at least kRingBufferChunkSize
				if(framesAvailableToWrite >= kRingBufferChunkSize && !(decoderState->mFlags.load() & DecoderStateData::eStopDecodingFlag)) {
					// If a seek is pending reset the ring buffer first to prevent artifacts
					if(decoderState->SeekRequested()) {
						_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);
						continue;
					}

					if(!(decoderState->mFlags.load() & DecoderStateData::eDecodingStartedFlag)) {
						// Perform the decoding started notification
						if(_decodingStartedNotificationHandler)
							dispatch_sync(_notificationQueue, ^{
								_decodingStartedNotificationHandler(decoderState->mDecoder);
							});

						decoderState->mFlags.fetch_or(DecoderStateData::eDecodingStartedFlag);
					}

					// Decode audio into the buffer, converting to the bus format in the process
					NSError *error;
					if(!decoderState->DecodeAudio(buffer, &error))
						os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", error);

					// Write the decoded audio to the ring buffer for rendering
					auto framesWritten = _ringBuffer.Write(buffer.audioBufferList, buffer.frameLength);
					if(framesWritten != buffer.frameLength)
						os_log_error(OS_LOG_DEFAULT, "SFB::Audio::RingBuffer::Write failed");

					if(decoderState->mFlags.load() & DecoderStateData::eDecodingFinishedFlag) {
						os_log_debug(OS_LOG_DEFAULT, "Decoding finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						// Some formats (MP3) may not know the exact number of frames in advance
						// without processing the entire file, which is a potentially slow operation
						decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

						// Perform the decoding finished notification
						if(_decodingFinishedNotificationHandler)
							dispatch_sync(_notificationQueue, ^{
								_decodingFinishedNotificationHandler(decoderState->mDecoder);
							});

						break;
					}
				}
				else if(decoderState->mFlags.load() & DecoderStateData::eStopDecodingFlag) {
					os_log_debug(OS_LOG_DEFAULT, "Stopping decoding for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

					_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);
					decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);

					break;
				}
				// Wait for additional space in the ring buffer
				else
					dispatch_semaphore_wait(_decoderSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10));
			}
		}
		// Wait for another decoder to be enqueued
		else
			dispatch_semaphore_wait(_decoderSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));
	}

	os_log_debug(OS_LOG_DEFAULT, "Decoder thread terminating");

	return nullptr;
}

- (void *)notifierThreadEntry
{
	os_log_debug(OS_LOG_DEFAULT, "Notifier thread starting");

	while(!(_flags.load() & eAudioPlayerNodeFlagStopNotifierThread)) {

		auto flags = _notifierFlags.load();
		if(flags & eAudioPlayerNodeNotifierFlagRenderingStarted) {
			_notifierFlags.fetch_and(~eAudioPlayerNodeNotifierFlagRenderingStarted);

			if(!self->_renderingStartedNotificationHandler)
				break;

			auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(self->_decoderStateArray);

//			dispatch_time_t notificationTime = dispatch_time(_notifierEventTimestamp.load(), (int64_t)(self.outputPresentationLatency * NSEC_PER_SEC));
			dispatch_time_t notificationTime = _notifierEventTimestamp.load();
			dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
				uint64_t absTime = mach_absolute_time();
				double delta = ((double)ConvertHostTimeToNanos(absTime) - (double)ConvertHostTimeToNanos(notificationTime)) / NSEC_PER_SEC;
				os_log_debug(OS_LOG_DEFAULT, "Rendering started notification arrived %f sec %s", delta, delta > 0 ? "late" : "early");
#endif

				self->_renderingStartedNotificationHandler(decoderState->mDecoder);
			});
		}

		if(flags & eAudioPlayerNodeNotifierFlagRenderingFinished) {
			_notifierFlags.fetch_and(~eAudioPlayerNodeNotifierFlagRenderingFinished);

			// The last action performed with a decoder that has completed rendering is this notification
			auto decoderState = GetInactiveDecoderStateWithSmallestSequenceNumber(self->_decoderStateArray);
			decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);

			if(!self->_renderingFinishedNotificationHandler)
				break;

//			dispatch_time_t notificationTime = dispatch_time(_notifierEventTimestamp.load(), (int64_t)(self.outputPresentationLatency * NSEC_PER_SEC));
			dispatch_time_t notificationTime = _notifierEventTimestamp.load();
			dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
				uint64_t absTime = mach_absolute_time();
				double delta = ((double)ConvertHostTimeToNanos(absTime) - (double)ConvertHostTimeToNanos(notificationTime)) / NSEC_PER_SEC;
				os_log_debug(OS_LOG_DEFAULT, "Rendering finished notification arrived %f sec %s", delta, delta > 0 ? "late" : "early");
#endif

				self->_renderingFinishedNotificationHandler(decoderState->mDecoder);
			});
		}

		dispatch_semaphore_wait(_notifierSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));
	}

	os_log_debug(OS_LOG_DEFAULT, "Notifier thread terminating");

	return nullptr;
}

@end