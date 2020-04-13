/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <atomic>
#import <queue>
#import <stack>
#import <thread>

#import <os/log.h>
#import <pthread.h>

#import "SFBAudioPlayer.h"

#import "SFBAudioDecoder+Internal.h"

@interface SFBAudioPlayer ()
- (void *)decoderThreadEntry;
@end

namespace {

	// ========================================
	// Flags
	enum eDecoderStateDataFlags : unsigned int {
		eDecoderStateDataFlagStopDecoding		= 1u << 0,
		eDecoderStateDataFlagDecodingStarted	= 1u << 1,
		eDecoderStateDataFlagSchedulingStarted	= 1u << 2,
		eDecoderStateDataFlagRenderingStarted	= 1u << 3,
		eDecoderStateDataFlagDecodingFinished	= 1u << 4,
		eDecoderStateDataFlagSchedulingFinished	= 1u << 5,
		eDecoderStateDataFlagRenderingFinished	= 1u << 6
	};

	enum eAudioPlayerFlags : unsigned int {
		eAudioPlayerFlagStopDecoding			= 1u << 0
	};

	// ========================================
	// Thread Management
	const integer_t DECODER_THREAD_IMPORTANCE = 6;

	//! Set the calling thread's timesharing to \c false  and importance to \c importance
	bool SetThreadPolicy(integer_t importance)
	{
		// Turn off timesharing
		thread_extended_policy_data_t extendedPolicy = {
			.timeshare = false
		};
		kern_return_t error = thread_policy_set(mach_thread_self(), THREAD_EXTENDED_POLICY, (thread_policy_t)&extendedPolicy, THREAD_EXTENDED_POLICY_COUNT);

		if(KERN_SUCCESS != error) {
			os_log_debug(OS_LOG_DEFAULT, "Couldn't set thread's extended policy: %{public}s", mach_error_string(error));
			return false;
		}

		// Give the thread the specified importance
		thread_precedence_policy_data_t precedencePolicy = {
			.importance = importance
		};
		error = thread_policy_set(mach_thread_self(), THREAD_PRECEDENCE_POLICY, (thread_policy_t)&precedencePolicy, THREAD_PRECEDENCE_POLICY_COUNT);

		if (error != KERN_SUCCESS) {
			os_log_debug(OS_LOG_DEFAULT, "Couldn't set thread's precedence policy: %{public}s", mach_error_string(error));
			return false;
		}

		return true;
	}

	//! Decoder thread entry point
	void * DecoderThreadEntry(void *arg)
	{
		pthread_setname_np("org.sbooth.AudioEngine.Decoder");

		// Make ourselves a high priority thread
		if(!SetThreadPolicy(DECODER_THREAD_IMPORTANCE))
			os_log_debug(OS_LOG_DEFAULT, "Couldn't set decoder thread importance");

		SFBAudioPlayer *player = (__bridge SFBAudioPlayer *)arg;
		return [player decoderThreadEntry];
	}

	// ========================================
	// Decoder State
	//! State data for tracking/syncing decoding progress
	struct DecoderStateData {
		using shared_ptr = std::shared_ptr<DecoderStateData>;
		using BufferStack = std::stack<AVAudioPCMBuffer *>;

		std::atomic_uint 	mFlags; 			//!< Decoder state data flags
		std::atomic_int64_t mFramesDecoded; 	//!< The number of frames decoded in the converter's *input* sample rate
		std::atomic_int64_t mFramesConverted;	//!< The number of frames converted in the converter's *output* sample rate
		std::atomic_int64_t mFramesScheduled;	//!< The number of frames scheduled in the converter's *output* sample rate
		std::atomic_int64_t mFramesRendered;	//!< The number of frames rendered in the converter's *output* sample rate
		std::atomic_int64_t mFrameLength;		//!< The total number of audio frames, in the decoder's sample rate
		std::atomic_int64_t mFrameToSeek;		//!< The desired seek offset, in the converter's *output* sample rate

//		std::atomic_int64_t mStartTime;			//!< The timestamp when the first frame should be rendered

//	private:
		SFBAudioDecoder 	*mDecoder; 			//!< Decodes audio from the source representation to PCM
		AVAudioConverter 	*mConverter;		//!< Converts audio from the decoder's processing format to PCM
	private:
		AVAudioPCMBuffer 	*mDecodeBuffer;		//!< Buffer used internally for buffering during conversion
		size_t 				mBufferCount;		//!< The number of buffers to allocate
		BufferStack 		mBuffers;			//!< Audio buffers for storing converted audio

	public:
		DecoderStateData(SFBAudioDecoder *decoder)
			: mFlags(0), mFramesDecoded(0), mFramesConverted(0), mFramesScheduled(0), mFramesRendered(0), mFrameLength(decoder.frameLength), mFrameToSeek(-1), mDecoder(decoder), mConverter(nil), mDecodeBuffer(nil), mBufferCount(0)
		{}

		~DecoderStateData()
		{
			assert(mBuffers.size() == mBufferCount);
		}

		inline AVAudioFramePosition ApparentFrame() const
		{
			int64_t seek = mFrameToSeek.load();
			int64_t rendered = mFramesRendered.load();
			return seek == -1 ? rendered : seek;
		}

		inline AVAudioFramePosition ApparentFrameLength() const
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
				if(!(mFlags.load() & eDecoderStateDataFlagDecodingStarted))
					mFlags.fetch_or(eDecoderStateDataFlagDecodingStarted);

				BOOL result = [mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:inNumberOfPackets error:&err];
				if(!result && err)
					os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);

				this->mFramesDecoded.fetch_add(mDecodeBuffer.frameLength);

				if(result && mDecodeBuffer.frameLength == 0) {
					mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished);
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
			mFrameToSeek.exchange(frame);
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

			AVAudioFramePosition newFrame = mDecoder.currentFrame;
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
				mFramesScheduled.exchange(seekOffset);
				mFramesRendered.exchange(seekOffset);
			}

			return newFrame != -1;
		}

		void AllocateBuffers(AVAudioFormat *format, size_t bufferCount = 10, AVAudioFrameCount frameCapacity = 1024)
		{
			// Deallocate existing buffers
			while(!mBuffers.empty())
				mBuffers.pop();

			mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
			AVAudioFrameCount conversionBufferLength = (AVAudioFrameCount)((mConverter.inputFormat.sampleRate / mConverter.outputFormat.sampleRate) * frameCapacity);
			mDecodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.inputFormat frameCapacity:conversionBufferLength];

			mBufferCount = bufferCount;
			for(size_t i = 0; i < mBufferCount; ++i)
				mBuffers.push([[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.outputFormat frameCapacity:frameCapacity]);
		}

		AVAudioPCMBuffer * DequeueBuffer()
		{
			if(!mBuffers.empty()) {
				AVAudioPCMBuffer *buffer = mBuffers.top();
				mBuffers.pop();
				return buffer;
			}
			return nil;
		}

		inline void ReturnBuffer(AVAudioPCMBuffer *buf)
		{
			mBuffers.push(buf);
		}
	};

	using DecoderQueue = std::queue<SFBAudioDecoder *>;

}

@interface SFBAudioPlayer ()
{
@private
	dispatch_queue_t				_engineQueue;		//!< The dispatch queue used to access `_engine`
	AVAudioEngine 					*_engine;			//!< The underlying AVAudioEngine instance
	AVAudioPlayerNode				*_player;			//!< The player from `_engine`
	std::atomic_uint				_flags;				//!< Player flags
	dispatch_queue_t				_queue;				//!< The dispatch queue used to access `_queuedDecoders`
	DecoderQueue 					_queuedDecoders;	//!< AudioDecoders enqueued for playback
	dispatch_semaphore_t			_semaphore;			//!< Semaphore used for communication between threads
	std::thread						_decoderThread;		//!< The high-priority decoding thread
	DecoderStateData::shared_ptr 	_decoderState; 		//!< The currently rendering decoder
}
- (void)audioEngineConfigurationChanged:(NSNotification *)notification;
@end

@implementation SFBAudioPlayer

- (instancetype)init
{
	if((self = [super init])) {
		_queue = dispatch_queue_create("org.sbooth.AudioEngine.Player.DecoderQueueAccessQueue", DISPATCH_QUEUE_SERIAL);
		if(_queue == NULL) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		_engineQueue = dispatch_queue_create("org.sbooth.AudioEngine.Player.AVAudioEngineAccessQueue", DISPATCH_QUEUE_SERIAL);
		if(_engineQueue == NULL) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
			return nil;
		}

		_semaphore = dispatch_semaphore_create(0);
		if(_semaphore == NULL) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_semaphore_create failed");
			return nil;
		}

		// Launch the decoding thread
		try {
			_decoderThread = std::thread(DecoderThreadEntry, (__bridge void *)self);
		}

		catch(const std::exception& e) {
			os_log_error(OS_LOG_DEFAULT, "Unable to create decoder thread: %{public}s", e.what());
			return nil;
		}

		// Create the audio processing graph
		_engine = [[AVAudioEngine alloc] init];
		_player = [[AVAudioPlayerNode alloc] init];
		[_engine attachNode:_player];
		[_engine connect:_player to:_engine.mainMixerNode format:nil];

		// Register for configuration change notifications
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(audioEngineConfigurationChanged:) name:AVAudioEngineConfigurationChangeNotification object:_engine];
	}
	return self;
}

- (void)dealloc
{
	_flags.fetch_or(eAudioPlayerFlagStopDecoding);
	dispatch_semaphore_signal(_semaphore);

	try {
		_decoderThread.join();
	}

	catch(const std::exception& e) {
		os_log_error(OS_LOG_DEFAULT, "Unable to join decoder thread: %{public}s", e.what());
	}

	while(!_queuedDecoders.empty())
		_queuedDecoders.pop();
}

#pragma mark Playlist Management

- (BOOL)playURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self playDecoder:decoder error:error];
}

- (BOOL)playDecoder:(SFBAudioDecoder *)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	if(![self stopReturningError:error])
		return NO;

	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
		_queuedDecoders.push(decoder);
	});
	dispatch_semaphore_signal(_semaphore);

	return [self playReturningError:error];
}

- (BOOL)enqueueURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
	if(!decoder)
		return NO;

	return [self enqueueDecoder:decoder error:error];
}

- (BOOL)enqueueDecoder:(SFBAudioDecoder *)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	dispatch_sync(_queue, ^{
		_queuedDecoders.push(decoder);
	});
	dispatch_semaphore_signal(_semaphore);

	return YES;
}

- (BOOL)skipToNext
{
	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState) {
		decoderState->mFlags.fetch_or(eDecoderStateDataFlagStopDecoding);
		dispatch_semaphore_signal(_semaphore);
	}

	dispatch_sync(_engineQueue, ^{
		BOOL wasPlaying = _player.isPlaying;
		[_player stop];
		if(wasPlaying)
			[_player play];
	});

	return YES;
}

- (void)clearQueue
{
	dispatch_sync(_queue, ^{
		while(!_queuedDecoders.empty())
			_queuedDecoders.pop();
	});
}

#pragma mark Playback Control

- (BOOL)playReturningError:(NSError **)error
{
	__block BOOL startedSuccessfully;
	__block NSError *err;
	dispatch_sync(_engineQueue, ^{
		[_engine prepare];
		startedSuccessfully = [_engine startAndReturnError:&err];
		if(startedSuccessfully)
			[_player play];
	});

	if(!startedSuccessfully && error)
		*error = err;

	return startedSuccessfully;
}

- (BOOL)pauseReturningError:(NSError **)error
{
	dispatch_sync(_engineQueue, ^{
		[_player pause];
		[_engine pause];
	});

	return YES;
}

- (BOOL)stopReturningError:(NSError **)error
{
	dispatch_sync(_engineQueue, ^{
		[_player stop];
		[_engine stop];
	});

	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState) {
		decoderState->mFlags.fetch_or(eDecoderStateDataFlagStopDecoding);
		dispatch_semaphore_signal(_semaphore);
	}

	return YES;
}

- (BOOL)playPauseReturningError:(NSError **)error
{
	return self.isPlaying ? [self pauseReturningError:error] : [self playReturningError:error];
}

#pragma mark Player State

- (BOOL)isRunning
{
	__block BOOL isRunning;
	dispatch_sync(_engineQueue, ^{
		isRunning = _engine.isRunning;
	});
	return isRunning;
}

- (BOOL)isPlaying
{
	__block BOOL isPlaying;
	dispatch_sync(_engineQueue, ^{
		isPlaying = _player.isPlaying;
	});
	return isPlaying;
}

- (NSURL *)url
{
	auto decoderState = std::atomic_load(&_decoderState);
	return decoderState ? decoderState->mDecoder.inputSource.url : nil;
}

- (id)representedObject
{
	auto decoderState = std::atomic_load(&_decoderState);
	return decoderState ? decoderState->mDecoder.representedObject : nil;
}

#pragma mark Playback Properties

- (AVAudioFramePosition)currentFrame
{
	return self.playbackPosition.currentFrame;
}

- (AVAudioFramePosition)totalFrames
{
	return self.playbackPosition.totalFrames;
}

- (SFBAudioPlayerPlaybackPosition)playbackPosition
{
	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState)
		return { .currentFrame = decoderState->ApparentFrame(), .totalFrames = decoderState->ApparentFrameLength() };
	return { .currentFrame = -1, .totalFrames = -1 };
}

- (NSTimeInterval)currentTime
{
	return self.playbackTime.currentTime;
}

- (NSTimeInterval)totalTime
{
	return self.playbackTime.totalTime;
}

- (SFBAudioPlayerPlaybackTime)playbackTime
{
	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState) {
		int64_t currentFrame = decoderState->ApparentFrame();
		int64_t totalFrames = decoderState->ApparentFrameLength();
		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		return { .currentTime = currentFrame / sampleRate, .totalTime = totalFrames / sampleRate };
	}

	return { .currentTime = -1, .totalTime = -1 };
}

- (BOOL)getPlaybackPosition:(SFBAudioPlayerPlaybackPosition *)playbackPosition andTime:(SFBAudioPlayerPlaybackTime *)playbackTime
{
	SFBAudioPlayerPlaybackPosition currentPlaybackPosition = { .currentFrame = -1, .totalFrames = -1 };
	SFBAudioPlayerPlaybackTime currentPlaybackTime = { .currentTime = -1, .totalTime = -1 };

	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState) {
		currentPlaybackPosition = { .currentFrame = decoderState->ApparentFrame(), .totalFrames = decoderState->ApparentFrameLength() };
		double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
		currentPlaybackTime = { .currentTime = currentPlaybackPosition.currentFrame / sampleRate, .totalTime = currentPlaybackPosition.totalFrames / sampleRate };
	}

	if(playbackPosition)
		*playbackPosition = currentPlaybackPosition;
	if(playbackTime)
		*playbackTime = currentPlaybackTime;

	return decoderState != nullptr;
}

#pragma mark Seeking

- (BOOL)seekForward
{
	return [self seekForward:3];
}

- (BOOL)seekBackward
{
	return [self seekBackward:3];
}

- (BOOL)seekForward:(NSTimeInterval)secondsToSkip
{
	NSParameterAssert(secondsToSkip > 0);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition currentFrame = decoderState->ApparentFrame();
	AVAudioFramePosition targetFrame = currentFrame + (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame >= decoderState->ApparentFrameLength())
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekBackward:(NSTimeInterval)secondsToSkip
{
	NSParameterAssert(secondsToSkip > 0);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition currentFrame = decoderState->ApparentFrame();
	AVAudioFramePosition targetFrame = currentFrame - (AVAudioFramePosition)(secondsToSkip * sampleRate);

	if(targetFrame < 0)
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToTime:(NSTimeInterval)timeInSeconds
{
	NSParameterAssert(timeInSeconds >= 0);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	double sampleRate = decoderState->mConverter.outputFormat.sampleRate;
	AVAudioFramePosition targetFrame = (AVAudioFramePosition)(timeInSeconds * sampleRate);

	if(targetFrame >= decoderState->ApparentFrameLength())
		return NO;

	return [self seekToFrame:targetFrame];
}

- (BOOL)seekToPosition:(float)position
{
	NSParameterAssert(position >= 0);
	NSParameterAssert(position < 1);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	AVAudioFramePosition totalFrames = decoderState->ApparentFrameLength();
	return [self seekToFrame:(AVAudioFramePosition)(totalFrames * position)];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	NSParameterAssert(frame >= 0);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	if(frame >= decoderState->ApparentFrameLength())
		return NO;

	decoderState->RequestSeekToFrame(frame);
	dispatch_semaphore_signal(_semaphore);

	return YES;

}

- (BOOL)supportsSeeking
{
	auto decoderState = std::atomic_load(&_decoderState);
	return decoderState ? decoderState->mDecoder.supportsSeeking : NO;
}

#pragma mark AVAudioEngine

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_sync(_engineQueue, ^{
		block(_engine);
	});
}

- (void)audioEngineConfigurationChanged:(NSNotification *)notification
{
	// FIXME: Actually do something here
	os_log_info(OS_LOG_DEFAULT, "Received AVAudioEngineConfigurationChangeNotification");
}

#pragma mark Decoding Thread

- (void *)decoderThreadEntry
{
	os_log_info(OS_LOG_DEFAULT, "Decoder thread starting");

	__block AVAudioFramePosition nextSampleTime = 0;
	__block DecoderStateData::shared_ptr schedulingDecoderState{};

	dispatch_queue_t bufferStackQueue = dispatch_queue_create("org.sbooth.AudioEngine.Player.BufferStackAccessQueue", DISPATCH_QUEUE_SERIAL);
	if(bufferStackQueue == NULL) {
		os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
		return NULL;
	}

	for(;;) {
		// Dequeue and process the next decoder
		__block SFBAudioDecoder *decoder = nil;
		dispatch_sync(_queue, ^{
			if(!_queuedDecoders.empty()) {
				decoder = _queuedDecoders.front();
				_queuedDecoders.pop();
			}
		});

		if(decoder) {
			// Open the decoder if necessary
			NSError *error;
			if(!decoder.isOpen && ![decoder openReturningError:&error]) {
				if(_decodingErrorNotificationHandler)
					_decodingErrorNotificationHandler(decoder, error);

				if(error)
					os_log_error(OS_LOG_DEFAULT, "Error opening decoder: %{public}@", error);

				continue;
			}

			// Create the decoder state
			auto decoderState = std::make_shared<DecoderStateData>(decoder);
			std::atomic_exchange(&schedulingDecoderState, decoderState);

			// The bus format is the format _player is expected to output
			// In the event the bus format and decoder processing format don't match, conversion will
			// be performed in DecoderStateData::DecodeAudio()
			__block AVAudioFormat *busFormat;
			dispatch_sync(_engineQueue, ^{
				busFormat = [_player outputFormatForBus:0];
			});

			// TODO: Add notification for channel count or sample rate mismatch

			os_log_info(OS_LOG_DEFAULT, "Decoding starting for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);
			os_log_info(OS_LOG_DEFAULT, "Decoder processing format: %{public}@", decoderState->mDecoder.processingFormat);
			os_log_info(OS_LOG_DEFAULT, "Bus format: %{public}@", busFormat);

			// The default decoder state contains 10 buffers of 1024 frames each
			// For typical 44.1 kHz audio this equates to approximately 0.23 sec
			decoderState->AllocateBuffers(busFormat);

			// TODO: Set converter parameters
//			decoderState->mConverter.sampleRateConverterAlgorithm = AVSampleRateConverterAlgorithm_Mastering;
//			decoderState->mConverter.sampleRateConverterQuality = AVAudioQualityMax;

			// Decode the audio until complete or stopped
			for(;;) {
				// Perform seek if requested
				if(decoderState->SeekRequested()) {
					// Stop _player to unschedule any pending buffers
					__block BOOL wasPlaying;
					dispatch_sync(_engineQueue, ^{
						wasPlaying = _player.isPlaying;
						[_player stop];
					});

					if(decoderState->PerformSeek())
						nextSampleTime = 0;

					if(wasPlaying)
						dispatch_sync(_engineQueue, ^{
							[_player play];
						});
				}

				// Dequeue the next available buffer for scheduling
				__block AVAudioPCMBuffer *buf = nil;
				dispatch_sync(bufferStackQueue, ^{
					buf = decoderState->DequeueBuffer();
				});

				// No buffers available; wait for one
				if(!buf) {
					dispatch_semaphore_wait(_semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2));
					continue;
				}

				// Decode audio into the buffer, converting to the bus format in the process
				if(!decoderState->DecodeAudio(buf, &error)) {
					os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", error);
					dispatch_sync(bufferStackQueue, ^{
						decoderState->ReturnBuffer(buf);
					});
				}

				if(decoderState->mFlags.load() & eDecoderStateDataFlagDecodingFinished) {
					os_log_info(OS_LOG_DEFAULT, "Decoding finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

					// Some formats (MP3) may not know the exact number of frames in advance
					// without processing the entire file, which is a potentially slow operation
					decoderState->mFrameLength.exchange(decoderState->mDecoder.frameLength);

					// Perform the decoding finished callback
					if(_decodingFinishedNotificationHandler)
						_decodingFinishedNotificationHandler(decoderState->mDecoder);
				}

				// Schedule the buffer for playback
				if(buf.frameLength) {
					AVAudioTime *time = [[AVAudioTime alloc] initWithSampleTime:nextSampleTime atRate:buf.format.sampleRate];

					if(!(decoderState->mFlags.load() & eDecoderStateDataFlagSchedulingStarted)) {
						os_log_info(OS_LOG_DEFAULT, "Scheduling started for \"%{public}@\" at sample time %lld", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path], nextSampleTime);

						decoderState->mFlags.fetch_or(eDecoderStateDataFlagSchedulingStarted);

						// Perform the scheduling started callback
						if(_schedulingStartedNotificationHandler)
							_schedulingStartedNotificationHandler(decoderState->mDecoder);
					}

					// Schedule the buffer on the AVAudioPlayerNode
					dispatch_sync(_engineQueue, ^{
						[_player scheduleBuffer:buf
										 atTime:time
										options:0
						 completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
							  completionHandler:^(AVAudioPlayerNodeCompletionCallbackType /*callbackType*/) {
							// The completion handler is called on a dedicated callback queue by AVAudioPlayerNode

							// Perform the rendering started callback
							if(!(decoderState->mFlags.load() & eDecoderStateDataFlagRenderingStarted)) {
								os_log_info(OS_LOG_DEFAULT, "Rendering started for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

								decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingStarted);

								// Store the decoder state for communication between threads
								std::atomic_store(&self->_decoderState, decoderState);

								// Perform the rendering started callback
								if(self->_renderingStartedNotificationHandler)
									self->_renderingStartedNotificationHandler(decoderState->mDecoder);
							}

							decoderState->mFramesRendered.fetch_add(buf.frameLength);

							// Perform the rendering finished callback
							if(decoderState->mFramesRendered.load() == decoderState->mFramesConverted.load() && decoderState->mFlags.load() & eDecoderStateDataFlagDecodingFinished) {
								os_log_info(OS_LOG_DEFAULT, "Rendering finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

								decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished);
								std::atomic_store(&self->_decoderState, DecoderStateData::shared_ptr{});

								// Perform the rendering finished callback
								if(self->_renderingFinishedNotificationHandler)
									self->_renderingFinishedNotificationHandler(decoderState->mDecoder);

								// If no decoder is being scheduled stop the player and engine
								if(!std::atomic_load(&schedulingDecoderState)) {
									os_log_info(OS_LOG_DEFAULT, "Stopping audio engine");

									nextSampleTime = 0;

									// Stop the audio engine
									dispatch_async(self->_engineQueue, ^{
										[self->_player stop];
										[self->_engine stop];
									});
								}
							}

							// Return the buffer to the decoder state for reuse
							dispatch_sync(bufferStackQueue, ^{
								decoderState->ReturnBuffer(buf);
							});

							dispatch_semaphore_signal(self->_semaphore);
						}];
					});

					decoderState->mFramesScheduled.fetch_add(buf.frameLength);

					if(decoderState->mFramesConverted.load() == decoderState->mFramesScheduled.load() && (decoderState->mFlags.load() & eDecoderStateDataFlagDecodingFinished) && !(decoderState->mFlags.load() & eDecoderStateDataFlagSchedulingFinished)) {
						os_log_info(OS_LOG_DEFAULT, "Scheduling finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

						decoderState->mFlags.fetch_or(eDecoderStateDataFlagSchedulingFinished);
						std::atomic_store(&schedulingDecoderState, DecoderStateData::shared_ptr{});

						// Perform the scheduling finished callback
						if(_schedulingFinishedNotificationHandler)
							_schedulingFinishedNotificationHandler(decoderState->mDecoder);
					}

					nextSampleTime += buf.frameLength;

					// Decoding is finished
					if(decoderState->mFlags.load() & eDecoderStateDataFlagDecodingFinished)
						break;
				}

				// Stop decoding if requested
				if(decoderState->mFlags.load() & eDecoderStateDataFlagStopDecoding) {
					os_log_info(OS_LOG_DEFAULT, "Stopping decoding for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

					// No atomic_compare_exchange_strong is available without atomic_shared_ptr
					if(decoderState == std::atomic_load(&_decoderState))
						std::atomic_store(&_decoderState, DecoderStateData::shared_ptr{});
					nextSampleTime = 0;

					break;
				}
			}

			// Check for termination request
			if(_flags.load() & eAudioPlayerFlagStopDecoding)
				goto finito;
		}

		// Wait for another decoder to be enqueued
		dispatch_semaphore_wait(_semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5));

		// Check for termination request
		if(_flags.load() & eAudioPlayerFlagStopDecoding)
			goto finito;
	}

	finito:
	os_log_info(OS_LOG_DEFAULT, "Decoder thread terminating");

	return nullptr;
}

@end
