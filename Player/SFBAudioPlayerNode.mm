/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <algorithm>
#import <atomic>
#import <cmath>
#import <queue>
#import <thread>

#import <mach/mach_time.h>
#import <os/log.h>

#import "SFBAudioPlayerNode.h"

#import "AudioRingBuffer.h"
#import "NSError+SFBURLPresentation.h"
#import "RingBuffer.h"
#import "SFBAudioDecoder.h"

NSErrorDomain const SFBAudioPlayerNodeErrorDomain = @"org.sbooth.AudioEngine.AudioPlayerNode";

@interface SFBAudioPlayerNode ()
- (void *)decoderThreadEntry;
- (void *)notifierThreadEntry;
@end

namespace {

	os_log_t _audioPlayerNodeLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayerNode");

#pragma mark - Flags

	enum eAudioPlayerNodeFlags : unsigned int {
		eAudioPlayerNodeFlagIsPlaying					= 1u << 0,
		eAudioPlayerNodeFlagOutputIsMuted				= 1u << 1,
		eAudioPlayerNodeFlagMuteRequested				= 1u << 2,
		eAudioPlayerNodeFlagRingBufferNeedsReset		= 1u << 3,
		eAudioPlayerNodeFlagStopDecoderThread			= 1u << 4,
		eAudioPlayerNodeFlagStopNotifierThread			= 1u << 5
	};

	enum eAudioPlayerNodeRenderEventRingBufferCommands : uint32_t {
		eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted	= 1,
		eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete	= 2,
		eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio			= 3
	};

#pragma mark - Thread entry points

	void * DecoderThreadEntry(void *arg)
	{
		pthread_setname_np("org.sbooth.AudioEngine.AudioPlayerNode.DecoderThread");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)arg;
		return [playerNode decoderThreadEntry];
	}

	void * NotifierThreadEntry(void *arg)
	{
		pthread_setname_np("org.sbooth.AudioEngine.AudioPlayerNode.NotifierThread");
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

		SFBAudioPlayerNode *playerNode = (__bridge SFBAudioPlayerNode *)arg;
		return [playerNode notifierThreadEntry];
	}

#pragma mark - Constants

	const AVAudioFrameCount 	kRingBufferFrameCapacity 	= 16384;
	const AVAudioFrameCount 	kRingBufferChunkSize 		= 2048;
	const size_t 				kDecoderStateArraySize		= 8;

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

		const uint64_t			mSequenceNumber;	///< Monotonically increasing instance counter

		std::atomic_uint 		mFlags; 			///< Decoder state data flags
		std::atomic_int64_t 	mFramesDecoded; 	///< The number of frames decoded in the converter's *input* sample rate
		std::atomic_int64_t 	mFramesConverted;	///< The number of frames converted in the converter's *output* sample rate
		std::atomic_int64_t 	mFramesRendered;	///< The number of frames rendered in the converter's *output* sample rate
		std::atomic_int64_t 	mFrameLength;		///< The total number of audio frames, in the decoder's sample rate
		std::atomic_int64_t 	mFrameToSeek;		///< The desired seek offset, in the converter's *output* sample rate

//	private:
		id <SFBPCMDecoding> 	mDecoder; 			///< Decodes audio from the source representation to PCM
		AVAudioConverter 		*mConverter;		///< Converts audio from the decoder's processing format to PCM
	private:
		AVAudioPCMBuffer 		*mDecodeBuffer;		///< Buffer used internally for buffering during conversion
		static uint64_t			sSequenceNumber; 	///< Next sequence number to use

	public:
		DecoderStateData(id <SFBPCMDecoding> decoder, AVAudioFormat *format, AVAudioFrameCount frameCapacity = kDefaultBufferSize)
			: mSequenceNumber(sSequenceNumber++), mFlags(0), mFramesDecoded(0), mFramesConverted(0), mFramesRendered(0), mFrameLength(decoder.frameLength), mFrameToSeek(-1), mDecoder(decoder), mConverter(nil), mDecodeBuffer(nil)
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
					os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", err);

				this->mFramesDecoded.fetch_add(mDecodeBuffer.frameLength);

				if(result && mDecodeBuffer.frameLength == 0) {
					mFlags.fetch_or(eDecodingCompleteFlag);
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

		/// Seeks to the desired frame  in the converter's *output* sample rate
		bool PerformSeek()
		{
			AVAudioFramePosition seekOffset = mFrameToSeek.load();

			double sampleRateRatio = mConverter.inputFormat.sampleRate / mConverter.outputFormat.sampleRate;
			AVAudioFramePosition adjustedSeekOffset = (AVAudioFramePosition)(seekOffset * sampleRateRatio);

			if(adjustedSeekOffset == seekOffset)
				os_log_debug(_audioPlayerNodeLog, "Seeking to frame %lld", adjustedSeekOffset);
			else
				os_log_debug(_audioPlayerNodeLog, "Seek to frame %lld requested, actually seeking to frame %lld", seekOffset, adjustedSeekOffset);

			if([mDecoder seekToFrame:adjustedSeekOffset error:nil])
				// Reset the converter to flush any buffers
				[mConverter reset];
			else
				os_log_debug(_audioPlayerNodeLog, "Error seeking to frame %lld", adjustedSeekOffset);

			AVAudioFramePosition newFrame = mDecoder.framePosition;
			if(newFrame != adjustedSeekOffset) {
				os_log_debug(_audioPlayerNodeLog, "Inaccurate seek to frame %lld, got %lld", adjustedSeekOffset, newFrame);
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

	};

	uint64_t DecoderStateData::sSequenceNumber = 0;
	using DecoderQueue = std::queue<id <SFBPCMDecoding>>;

	/// Returns the element in \c decoders with the smallest sequence number that has not completed rendering and has not been marked for removal
	DecoderStateData * GetActiveDecoderStateWithSmallestSequenceNumber(const DecoderStateData::atomic_ptr *decoderStateArray, const size_t& count)
	{
		DecoderStateData *result = nullptr;
		for(size_t i = 0; i < count; ++i) {
			auto decoderState = decoderStateArray[i].load();
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

	/// Returns the element in \c decoders with the smallest sequence number greater than \c sequenceNumber that has not completed rendering and has not been marked for removal
	DecoderStateData * GetActiveDecoderStateFollowingSequenceNumber(const DecoderStateData::atomic_ptr *decoderStateArray, const size_t& count, const uint64_t& sequenceNumber)
	{
		DecoderStateData *result = nullptr;
		for(size_t i = 0; i < count; ++i) {
			auto decoderState = decoderStateArray[i].load();
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

	/// Returns the element in \c decoders with the sequence number equal to \c sequenceNumber that has not been marked for removal
	DecoderStateData * GetDecoderStateWithSequenceNumber(const DecoderStateData::atomic_ptr *decoderStateArray, const size_t& count, const uint64_t& sequenceNumber)
	{
		for(size_t i = 0; i < count; ++i) {
			auto decoderState = decoderStateArray[i].load();
			if(!decoderState)
				continue;

			if(decoderState->mFlags.load() & DecoderStateData::eMarkedForRemovalFlag)
				continue;

			if(decoderState->mSequenceNumber == sequenceNumber)
				return decoderState;
		}

		return nullptr;
	}

#pragma mark - Time Utilities

	// These functions are probably unnecessarily complicated because
	// on Intel processors mach_timebase_info is always 1/1. However,
	// on PPC it is either 1000000000/33333335 or 1000000000/25000000 so
	// naively multiplying by .numer then dividing by .denom may result in
	// integer overflow. To avoid the possibility double is used here, but
	// __int128 would be an alternative.

	double HostTicksPerNano()
	{
		mach_timebase_info_data_t timebase_info;
		auto result = mach_timebase_info(&timebase_info);
		assert(result == KERN_SUCCESS);
		return (double)timebase_info.numer / (double)timebase_info.denom;
	}

	double NanosPerHostTick()
	{
		mach_timebase_info_data_t timebase_info;
		auto result = mach_timebase_info(&timebase_info);
		assert(result == KERN_SUCCESS);
		return (double)timebase_info.denom / (double)timebase_info.numer;
	}

	const double kHostTicksPerNano = HostTicksPerNano();
	const double kNanosPerHostTick = NanosPerHostTick();

	inline uint64_t ConvertNanosToHostTicks(double ns)
	{
		return (uint64_t)(ns * kNanosPerHostTick);
	}

	inline uint64_t ConvertSecondsToHostTicks(double s)
	{
		return ConvertNanosToHostTicks(s * NSEC_PER_SEC);
	}

#if DEBUG
	inline double ConvertHostTicksToNanos(uint64_t t)
	{
		return (double)t * kHostTicksPerNano;
	}
#endif

}

#pragma mark -

@interface SFBAudioPlayerNode ()
{
@private
	dispatch_queue_t				_queue;				///< The dispatch queue used to access \c _queuedDecoders
	DecoderQueue 					_queuedDecoders;	///< Decoders enqueued for playback

	// Decoding thread variables
	std::thread 					_decodingThread;
	dispatch_semaphore_t			_decodingSemaphore;

	// Notification thread variables
	std::thread 					_notifierThread;
	dispatch_semaphore_t			_notifierSemaphore;
	dispatch_queue_t				_notificationQueue;

	// Collector
	dispatch_source_t				_collector;

	// Shared state accessed from multiple threads/queues
	std::atomic_uint 				_flags;
	SFB::Audio::RingBuffer			_audioRingBuffer;
	SFB::RingBuffer					_renderEventsRingBuffer;
	DecoderStateData::atomic_ptr 	_decoderStateArray [kDecoderStateArraySize];
}
- (BOOL)performEnqueue:(id <SFBPCMDecoding>)decoder reset:(BOOL)reset error:(NSError **)error;
@end

@implementation SFBAudioPlayerNode

- (instancetype)initWithFormat:(AVAudioFormat *)format
{
	NSParameterAssert(format != nil);
	NSParameterAssert(!format.interleaved);

	AVAudioSourceNodeRenderBlock renderBlock = ^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp, AVAudioFrameCount frameCount, AudioBufferList *outputData) {

		// ========================================
		// Pre-rendering actions

		// ========================================
		// 0. Mute output if requested
		if(self->_flags.load() & eAudioPlayerNodeFlagMuteRequested) {
			self->_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);
			self->_flags.fetch_and(~eAudioPlayerNodeFlagMuteRequested);
			dispatch_semaphore_signal(self->_decodingSemaphore);
		}

		// ========================================
		// Rendering

		// ========================================
		// 1. Determine how many audio frames are available to read in the ring buffer
		AVAudioFrameCount framesAvailableToRead = (AVAudioFrameCount)self->_audioRingBuffer.GetFramesAvailableToRead();

		// ========================================
		// 2. Output silence if a) the node isn't playing, b) the node is muted, or c) the ring buffer is empty
		if(!(self->_flags.load() & eAudioPlayerNodeFlagIsPlaying) || self->_flags.load() & eAudioPlayerNodeFlagOutputIsMuted || framesAvailableToRead == 0) {
			size_t byteCountToZero = self->_audioRingBuffer.GetFormat().FrameCountToByteCount(frameCount);
			for(UInt32 bufferIndex = 0; bufferIndex < outputData->mNumberBuffers; ++bufferIndex) {
				memset(outputData->mBuffers[bufferIndex].mData, self->_audioRingBuffer.GetFormat().IsDSD() ? 0xF : 0, byteCountToZero);
				outputData->mBuffers[bufferIndex].mDataByteSize = (UInt32)byteCountToZero;
			}

			*isSilence = YES;
			return noErr;
		}

		// ========================================
		// 3. Read as many frames as available from the ring buffer
		AVAudioFrameCount framesToRead = std::min(framesAvailableToRead, frameCount);
		AVAudioFrameCount framesRead = (AVAudioFrameCount)self->_audioRingBuffer.Read(outputData, framesToRead);
		if(framesRead != framesToRead)
			os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Read failed: Requested %u frames, got %u", framesToRead, framesRead);

		// ========================================
		// 4. If the ring buffer didn't contain as many frames as requested fill the remainder with silence
		if(framesRead != frameCount) {
			os_log_debug(_audioPlayerNodeLog, "Insufficient audio in ring buffer: %u frames available, %u requested", framesRead, frameCount);

			auto framesOfSilence = frameCount - framesRead;
			auto byteCountToSkip = self->_audioRingBuffer.GetFormat().FrameCountToByteCount(framesRead);
			auto byteCountToZero = self->_audioRingBuffer.GetFormat().FrameCountToByteCount(framesOfSilence);
			for(UInt32 bufferIndex = 0; bufferIndex < outputData->mNumberBuffers; ++bufferIndex) {
				memset((int8_t *)outputData->mBuffers[bufferIndex].mData + byteCountToSkip, self->_audioRingBuffer.GetFormat().IsDSD() ? 0xF : 0, byteCountToZero);
			}
		}

		// ========================================
		// 5. If there is adequate space in the ring buffer for another chunk signal the decoding thread
		AVAudioFrameCount framesAvailableToWrite = (AVAudioFrameCount)self->_audioRingBuffer.GetFramesAvailableToWrite();
		if(framesAvailableToWrite >= kRingBufferChunkSize)
			dispatch_semaphore_signal(self->_decodingSemaphore);

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

		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(self->_decoderStateArray, kDecoderStateArraySize);
		while(decoderState) {
			AVAudioFrameCount decoderFramesRemaining = (AVAudioFrameCount)(decoderState->mFramesConverted.load() - decoderState->mFramesRendered.load());
			AVAudioFrameCount framesFromThisDecoder = std::min(decoderFramesRemaining, framesRead);

			if(!(decoderState->mFlags.load() & DecoderStateData::eRenderingStartedFlag)) {
				// Schedule the rendering started notification
				const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted;
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(frameOffset / self->_audioRingBuffer.GetFormat().mSampleRate);

				uint8_t bytesToWrite [4 + 8 + 8];
				memcpy(bytesToWrite, &cmd, 4);
				memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
				memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
				self->_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8 + 8 );
				dispatch_semaphore_signal(self->_notifierSemaphore);

				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingStartedFlag);
			}

			decoderState->mFramesRendered.fetch_add(framesFromThisDecoder);
			framesRemainingToDistribute -= framesFromThisDecoder;

			if((decoderState->mFlags.load() & DecoderStateData::eDecodingCompleteFlag) && decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load()) {
				// Schedule the rendering complete notification
				const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete;
				const uint32_t frameOffset = framesRead - framesRemainingToDistribute;
				const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(frameOffset / self->_audioRingBuffer.GetFormat().mSampleRate);

				uint8_t bytesToWrite [4 + 8 + 8];
				memcpy(bytesToWrite, &cmd, 4);
				memcpy(bytesToWrite + 4, &decoderState->mSequenceNumber, 8);
				memcpy(bytesToWrite + 4 + 8, &hostTime, 8);
				self->_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8 + 8);
				dispatch_semaphore_signal(self->_notifierSemaphore);

				decoderState->mFlags.fetch_or(DecoderStateData::eRenderingCompleteFlag);
			}

			if(framesRemainingToDistribute == 0)
				break;

			decoderState = GetActiveDecoderStateFollowingSequenceNumber(self->_decoderStateArray, kDecoderStateArraySize, decoderState->mSequenceNumber);
		}

		// ========================================
		// 8. If there are no active decoders schedule the end of audio notification

		decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(self->_decoderStateArray, kDecoderStateArraySize);
		if(!decoderState) {
			const uint32_t cmd = eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio;
			const uint64_t hostTime = timestamp->mHostTime + ConvertSecondsToHostTicks(framesRead / self->_audioRingBuffer.GetFormat().mSampleRate);

			uint8_t bytesToWrite [4 + 8];
			memcpy(bytesToWrite, &cmd, 4);
			memcpy(bytesToWrite + 4, &hostTime, 8);
			self->_renderEventsRingBuffer.Write(bytesToWrite, 4 + 8);
			dispatch_semaphore_signal(self->_notifierSemaphore);
		}

		return noErr;
	};

	if((self = [super initWithFormat:format renderBlock:renderBlock])) {
		os_log_info(_audioPlayerNodeLog, "Render block format: %{public}@", format);

		// _flags and _decoderStateArray are used in the render block so must be lock free
		assert(_flags.is_lock_free());
		assert(_decoderStateArray[0].is_lock_free());

		// Initialize the decoder array
		for(size_t i = 0; i < kDecoderStateArraySize; ++i)
			_decoderStateArray[i].store(nullptr);

		// Allocate the audio ring buffer and the rendering events ring buffer
		_renderingFormat = format;
		if(!_audioRingBuffer.Allocate(_renderingFormat.streamDescription, kRingBufferFrameCapacity)) {
			os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Allocate() failed");
			return nil;
		}

		_renderEventsRingBuffer.Allocate(256);

#if 0
		// See the comments in SFBAudioPlayer -setupEngineForGaplessPlaybackOfFormat:
		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice while 1156 is AVAudioSourceNode's default
		AVAudioFrameCount maximumFramesToRender = (AVAudioFrameCount)ceil(512 * (format.sampleRate / 44100));
		if(self.AUAudioUnit.maximumFramesToRender < maximumFramesToRender) {
			os_log_debug(_audioPlayerNodeLog, "Setting maximumFramesToRender to %u", maximumFramesToRender);
			self.AUAudioUnit.maximumFramesToRender = maximumFramesToRender;
		}
#endif
		_queue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayerNode.DecoderQueueIsolationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_queue) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_create failed");
			return nil;
		}

		_notificationQueue = dispatch_queue_create("org.sbooth.AudioEngine.AudioPlayerNode.NotificationQueue", DISPATCH_QUEUE_SERIAL);
		if(!_notificationQueue) {
			os_log_error(_audioPlayerNodeLog, "dispatch_queue_create failed");
			return nil;
		}

		_decodingSemaphore = dispatch_semaphore_create(0);
		if(!_decodingSemaphore) {
			os_log_error(_audioPlayerNodeLog, "dispatch_semaphore_create failed");
			return nil;
		}

		_notifierSemaphore = dispatch_semaphore_create(0);
		if(!_notifierSemaphore) {
			os_log_error(_audioPlayerNodeLog, "dispatch_semaphore_create failed");
			return nil;
		}

		// Setup the collector
		_collector = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0));
		if(!_collector) {
			os_log_error(_audioPlayerNodeLog, "dispatch_source_create failed");
			return nil;
		}

		dispatch_source_set_timer(_collector, DISPATCH_TIME_NOW, NSEC_PER_SEC * 10, NSEC_PER_SEC * 2);
		dispatch_source_set_event_handler(_collector, ^{
			for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
				auto decoderState = self->_decoderStateArray[i].load();
				if(!decoderState || !(decoderState->mFlags.load() & DecoderStateData::eMarkedForRemovalFlag))
					continue;

				// See comment in -decoderThreadEntry on why I believe it's safe to use store() and not a CAS loop
				os_log_debug(_audioPlayerNodeLog, "Collecting decoder for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
				self->_decoderStateArray[i].store(nullptr);
				delete decoderState;
			}
		});

		// Start collecting
		dispatch_resume(_collector);

		// Launch the threads
		try {
			_decodingThread = std::thread(DecoderThreadEntry, (__bridge void *)self);
			_notifierThread = std::thread(NotifierThreadEntry, (__bridge void *)self);
		}

		catch(const std::exception& e) {
			os_log_error(_audioPlayerNodeLog, "Unable to create thread: %{public}s", e.what());
			return nil;
		}
	}

	return self;
}

- (void)dealloc
{
	_flags.fetch_or(eAudioPlayerNodeFlagStopDecoderThread | eAudioPlayerNodeFlagStopNotifierThread);
	dispatch_semaphore_signal(_decodingSemaphore);
	dispatch_semaphore_signal(_notifierSemaphore);
	_decodingThread.join();
	_notifierThread.join();
	while(!_queuedDecoders.empty())
		_queuedDecoders.pop();

	// Force any decoders left hanging by the collector to end
	for(size_t i = 0; i < kDecoderStateArraySize; ++i) {
		if(_decoderStateArray[i])
			delete _decoderStateArray[i].exchange(nullptr);
	}
}

#pragma mark - Format Information

- (BOOL)supportsFormat:(AVAudioFormat *)format
{
	// Gapless playback requires the same number of channels at the same sample rate
	return format.channelCount == _renderingFormat.channelCount && format.sampleRate == _renderingFormat.sampleRate;
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
	return [self performEnqueue:decoder reset:YES error:error];
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
	return [self performEnqueue:decoder reset:NO error:error];
}

- (void)cancelCurrentDecoder
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(decoderState) {
		os_log_info(_audioPlayerNodeLog, "Skipping \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
		decoderState->mFlags.fetch_or(DecoderStateData::eCancelDecodingFlag);
		dispatch_semaphore_signal(_decodingSemaphore);
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

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(decoderState) {
		decoderState->mFlags.fetch_or(DecoderStateData::eCancelDecodingFlag);
		dispatch_semaphore_signal(_decodingSemaphore);
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

- (void)stop
{
	_flags.fetch_and(~eAudioPlayerNodeFlagIsPlaying);
	[self reset];
}

- (void)togglePlayPause
{
	_flags.fetch_xor(eAudioPlayerNodeFlagIsPlaying);
}

#pragma mark - Player State

- (BOOL)isPlaying
{
	return (_flags.load() & eAudioPlayerNodeFlagIsPlaying) != 0;
}

- (BOOL)isReady
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	return decoderState ? YES : NO;
}

- (id<SFBPCMDecoding>)currentDecoder
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	return decoderState ? decoderState->mDecoder : nil;
}

#pragma mark - Playback Properties

- (SFBAudioPlayerNodePlaybackPosition)playbackPosition
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return { .framePosition = -1, .frameLength = -1 };

	return { .framePosition = decoderState->FramePosition(), .frameLength = decoderState->FrameLength() };
}

- (SFBAudioPlayerNodePlaybackTime)playbackTime
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
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

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
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
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition framePosition = decoderState->FramePosition();
	AVAudioFramePosition targetFrame = framePosition + (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame >= decoderState->FrameLength())
		targetFrame = std::max(decoderState->FrameLength() - 1, 0ll);

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	if(secondsToSkip < 0)
		secondsToSkip = 0;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition framePosition = decoderState->FramePosition();
	AVAudioFramePosition targetFrame = framePosition - (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame < 0)
		targetFrame = 0;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	if(timeInSeconds < 0)
		timeInSeconds = 0;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition targetFrame = (AVAudioFramePosition)(timeInSeconds * sampleRate);

	if(targetFrame >= decoderState->FrameLength())
		targetFrame = std::max(decoderState->FrameLength() - 1, 0ll);

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToPosition:(float)position
{
	if(position < 0)
		position = 0;
	else if(position >= 1)
		position = std::nextafter(1.f, 0.f);

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return NO;

	AVAudioFramePosition frameLength = decoderState->FrameLength();
	return [self seekToFrame:(AVAudioFramePosition)(frameLength * position)];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	if(frame < 0)
		frame = 0;

	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	if(!decoderState)
		return NO;

	if(frame >= decoderState->FrameLength())
		frame = std::max(decoderState->FrameLength() - 1, 0ll);

	decoderState->mFrameToSeek.store(frame);
	dispatch_semaphore_signal(_decodingSemaphore);

	return YES;
}

- (BOOL)supportsSeeking
{
	auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
	return decoderState ? decoderState->mDecoder.supportsSeeking : NO;
}


#pragma mark - Internals

- (BOOL)performEnqueue:(id <SFBPCMDecoding>)decoder reset:(BOOL)reset error:(NSError **)error
{
	os_log_info(_audioPlayerNodeLog, "Enqueuing \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoder.inputSource.url.path]);

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	if(![self supportsFormat:decoder.processingFormat]) {
		os_log_error(_audioPlayerNodeLog, "Unsupported decoder processing format: %{public}@", decoder.processingFormat);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioPlayerNodeErrorDomain
											 code:SFBAudioPlayerNodeErrorFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
											  url:decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's format is not supported by this player.", @"")];

		return NO;
	}

	if(reset) {
		[self clearQueue];
		auto decoderState = GetActiveDecoderStateWithSmallestSequenceNumber(_decoderStateArray, kDecoderStateArraySize);
		if(decoderState)
			decoderState->mFlags.fetch_or(DecoderStateData::eCancelDecodingFlag);
	}

	dispatch_sync(_queue, ^{
		_queuedDecoders.push(decoder);
	});
	dispatch_semaphore_signal(_decodingSemaphore);

	return YES;
}

- (void *)decoderThreadEntry
{
	os_log_debug(_audioPlayerNodeLog, "Decoder thread starting");

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
			auto decoderState = new DecoderStateData(decoder, self->_renderingFormat, kRingBufferChunkSize);

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
					// 44,100 Hz 2048 frames equates 0.05 sec.
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
							dispatch_semaphore_wait(_decodingSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 100));
					}
					else
						_flags.fetch_or(eAudioPlayerNodeFlagOutputIsMuted);

					// Perform seek if one is pending
					if(decoderState->mFrameToSeek.load() != -1)
						decoderState->PerformSeek();

					// Reset() is not thread safe but the rendering thread is outputting silence
					_audioRingBuffer.Reset();

					// Clear the mute flag
					_flags.fetch_and(~eAudioPlayerNodeFlagOutputIsMuted);
				}

				// Determine how many frames are available in the ring buffer
				auto framesAvailableToWrite = _audioRingBuffer.GetFramesAvailableToWrite();

				// Force writes to the ring buffer to be at least kRingBufferChunkSize
				if(framesAvailableToWrite >= kRingBufferChunkSize && !(decoderState->mFlags.load() & DecoderStateData::eCancelDecodingFlag)) {
					// If a seek is pending reset the ring buffer first to prevent artifacts
					if(decoderState->mFrameToSeek.load() != -1) {
						_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);
						continue;
					}

					if(!(decoderState->mFlags.load() & DecoderStateData::eDecodingStartedFlag)) {
						os_log_debug(_audioPlayerNodeLog, "Decoding started for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						// Perform the decoding started notification
						if([_delegate respondsToSelector:@selector(audioPlayerNode:decodingStarted:)])
							dispatch_sync(_notificationQueue, ^{
								[_delegate audioPlayerNode:self decodingStarted:decoderState->mDecoder];
							});

						decoderState->mFlags.fetch_or(DecoderStateData::eDecodingStartedFlag);
					}

					// Decode audio into the buffer, converting to the bus format in the process
					NSError *error;
					if(!decoderState->DecodeAudio(buffer, &error))
						os_log_error(_audioPlayerNodeLog, "Error decoding audio: %{public}@", error);

					// Write the decoded audio to the ring buffer for rendering
					auto framesWritten = _audioRingBuffer.Write(buffer.audioBufferList, buffer.frameLength);
					if(framesWritten != buffer.frameLength)
						os_log_error(_audioPlayerNodeLog, "SFB::Audio::RingBuffer::Write() failed");

					if(decoderState->mFlags.load() & DecoderStateData::eDecodingCompleteFlag) {
						// Some formats (MP3) may not know the exact number of frames in advance
						// without processing the entire file, which is a potentially slow operation
						decoderState->mFrameLength.store(decoderState->mDecoder.frameLength);

						// Perform the decoding complete notification
						if([_delegate respondsToSelector:@selector(audioPlayerNode:decodingComplete:)])
							dispatch_sync(_notificationQueue, ^{
								[_delegate audioPlayerNode:self decodingComplete:decoderState->mDecoder];
							});

						os_log_debug(_audioPlayerNodeLog, "Decoding complete for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						break;
					}
				}
				else if(decoderState->mFlags.load() & DecoderStateData::eCancelDecodingFlag) {
					os_log_debug(_audioPlayerNodeLog, "Canceling decoding for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

					// Perform the decoding cancelled notification
					if([_delegate respondsToSelector:@selector(audioPlayerNode:decodingCanceled:)])
						dispatch_sync(_notificationQueue, ^{
							[_delegate audioPlayerNode:self decodingCanceled:decoderState->mDecoder];
						});

					_flags.fetch_or(eAudioPlayerNodeFlagRingBufferNeedsReset);
					decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);

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

	return nullptr;
}

- (void *)notifierThreadEntry
{
	os_log_debug(_audioPlayerNodeLog, "Notifier thread starting");

	while(!(_flags.load() & eAudioPlayerNodeFlagStopNotifierThread)) {

		if(self->_renderEventsRingBuffer.GetBytesAvailableToRead() >= 4) {
			uint32_t cmd;
			/*auto bytesRead =*/ self->_renderEventsRingBuffer.Read(&cmd, 4);

			switch(cmd) {
				case eAudioPlayerNodeRenderEventRingBufferCommandRenderingStarted:
					if(self->_renderEventsRingBuffer.GetBytesAvailableToRead() >= (8 + 8)) {
						uint64_t sequenceNumber, hostTime;
						/*bytesRead =*/ self->_renderEventsRingBuffer.Read(&sequenceNumber, 8);
						/*bytesRead =*/ self->_renderEventsRingBuffer.Read(&hostTime, 8);

						auto decoderState = GetDecoderStateWithSequenceNumber(self->_decoderStateArray, kDecoderStateArraySize, sequenceNumber);
						if(!decoderState) {
							os_log_error(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing", sequenceNumber);
							break;
						}

						os_log_debug(_audioPlayerNodeLog, "Rendering will start in %.2f msec for \"%{public}@\"", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC, [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						if([_delegate respondsToSelector:@selector(audioPlayerNode:renderingWillStart:atHostTime:)])
							dispatch_sync(_notificationQueue, ^{
								[_delegate audioPlayerNode:self renderingWillStart:decoderState->mDecoder atHostTime:hostTime];
							});

						if([_delegate respondsToSelector:@selector(audioPlayerNode:renderingStarted:)]) {
							dispatch_time_t notificationTime = hostTime;
							dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
								double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
								double tolerance = 1000 / self->_audioRingBuffer.GetFormat().mSampleRate;
								if(abs(delta) > tolerance)
									os_log_debug(_audioPlayerNodeLog, "Rendering started notification for \"%{public}@\" arrived %.2f msec %s", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path], delta, delta > 0 ? "late" : "early");
#endif

								[self->_delegate audioPlayerNode:self renderingStarted:decoderState->mDecoder];
							});
						}
					}
					else
						os_log_error(_audioPlayerNodeLog, "Ring buffer command data missing");
					break;

				case eAudioPlayerNodeRenderEventRingBufferCommandRenderingComplete:
					if(self->_renderEventsRingBuffer.GetBytesAvailableToRead() >= (8 + 8)) {
						uint64_t sequenceNumber, hostTime;
						/*bytesRead =*/ self->_renderEventsRingBuffer.Read(&sequenceNumber, 8);
						/*bytesRead =*/ self->_renderEventsRingBuffer.Read(&hostTime, 8);

						auto decoderState = GetDecoderStateWithSequenceNumber(self->_decoderStateArray, kDecoderStateArraySize, sequenceNumber);
						if(!decoderState) {
							os_log_error(_audioPlayerNodeLog, "Decoder state with sequence number %llu missing", sequenceNumber);
							break;
						}

						os_log_debug(_audioPlayerNodeLog, "Rendering will complete in %.2f msec for \"%{public}@\"", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC, [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						if([_delegate respondsToSelector:@selector(audioPlayerNode:renderingComplete:)]) {
							dispatch_time_t notificationTime = hostTime;
							dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
								double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
								double tolerance = 1000 / self->_audioRingBuffer.GetFormat().mSampleRate;
								if(abs(delta) > tolerance)
									os_log_debug(_audioPlayerNodeLog, "Rendering complete notification for \"%{public}@\" arrived %.2f msec %s", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path], delta, delta > 0 ? "late" : "early");
#endif

								[self->_delegate audioPlayerNode:self renderingComplete:decoderState->mDecoder];
							});
						}

						// The last action performed with a decoder that has completed rendering is this notification
						decoderState->mFlags.fetch_or(DecoderStateData::eMarkedForRemovalFlag);
					}
					else
						os_log_error(_audioPlayerNodeLog, "Ring buffer command data missing");
					break;

				case eAudioPlayerNodeRenderEventRingBufferCommandEndOfAudio:
					if(self->_renderEventsRingBuffer.GetBytesAvailableToRead() >= 8) {
						uint64_t hostTime;
						/*bytesRead =*/ self->_renderEventsRingBuffer.Read(&hostTime, 8);

						os_log_debug(_audioPlayerNodeLog, "End of audio in %.2f msec", (ConvertHostTicksToNanos(hostTime) - ConvertHostTicksToNanos(mach_absolute_time())) / NSEC_PER_MSEC);

						if([_delegate respondsToSelector:@selector(audioPlayerNodeEndOfAudio:)]) {
							dispatch_time_t notificationTime = hostTime;
							dispatch_after(notificationTime, _notificationQueue, ^{
#if DEBUG
								double delta = (ConvertHostTicksToNanos(mach_absolute_time()) - ConvertHostTicksToNanos(notificationTime)) / NSEC_PER_MSEC;
								double tolerance = 1000 / self->_audioRingBuffer.GetFormat().mSampleRate;
								if(abs(delta) > tolerance)
									os_log_debug(_audioPlayerNodeLog, "End of audio notification arrived %.2f msec %s", delta, delta > 0 ? "late" : "early");
#endif

								[self->_delegate audioPlayerNodeEndOfAudio:self];
							});
						}
					}
					else
						os_log_error(_audioPlayerNodeLog, "Ring buffer command data missing");
				break;
			}
		}

		dispatch_semaphore_wait(_notifierSemaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));
	}

	os_log_debug(_audioPlayerNodeLog, "Notifier thread terminating");

	return nullptr;
}

@end
