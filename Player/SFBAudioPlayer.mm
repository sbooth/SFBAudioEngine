/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <atomic>
#import <thread>
#import <vector>

#import <os/log.h>
#import <pthread.h>

#import "SFBAudioPlayer.h"

#import "Semaphore.h"
#import "SFBAudioDecoder+Internal.h"

@interface SFBAudioPlayer ()
- (void *)decoderThreadEntry;
@end

namespace {

	enum eDecoderStateDataFlags : unsigned int {
		eDecoderStateDataFlagStopDecoding		= 1u << 0,
		eDecoderStateDataFlagDecodingStarted	= 1u << 1,
		eDecoderStateDataFlagDecodingFinished	= 1u << 2,
		eDecoderStateDataFlagRenderingStarted	= 1u << 3,
		eDecoderStateDataFlagRenderingFinished	= 1u << 4
	};

	enum eAudioPlayerFlags : unsigned int {
		eAudioPlayerFlagStopDecoding			= 1u << 0
	};

}

namespace {

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

}

namespace {

	//! State data for tracking/syncing decoding progress
	struct SFBAudioDecoderStateData {
		std::atomic_uint mFlags;
		std::atomic_int64_t mFramesDecoded; 	//!< The number of frames decoded in the converter's input sample rate
		std::atomic_int64_t mFramesConverted;	//!< The number of frames converted in the converter's output sample rate
		std::atomic_int64_t mFramesScheduled;	//!< The number of frames scheduled in the converter's output sample rate
		std::atomic_int64_t mFramesRendered;	//!< The number of frames rendered in the converter's output sample rate
		std::atomic_int64_t mTotalFrames;		//!< The total number of audio frames, in the decoder's sample rate
		std::atomic_int64_t mFrameToSeek;		//!< The desired seek offset, in the converter's output sample rate

//	private:
		SFBAudioDecoder *mDecoder;
		AVAudioConverter *mConverter;
	private:
		AVAudioPCMBuffer *mDecodeBuffer;

		size_t mBufferCount;
		OSQueueHead mQueue;

		struct Buffer {
			struct Buffer *mLink;
			AVAudioPCMBuffer *mBuffer;
		};

	public:
		SFBAudioDecoderStateData(SFBAudioDecoder *decoder, size_t bufferCount = 10)
			: mFlags(0), mFramesDecoded(0), mFramesConverted(0), mFramesScheduled(0), mFramesRendered(0), mTotalFrames(0), mFrameToSeek(-1), mDecoder(decoder), mConverter(nil), mDecodeBuffer(nil), mBufferCount(bufferCount), mQueue(OS_ATOMIC_QUEUE_INIT)
		{}

		~SFBAudioDecoderStateData()
		{
			DeallocateBuffers();
		}

		inline AVAudioFramePosition ApparentFrame() const
		{
			int64_t seek = mFrameToSeek;
			int64_t rendered = mFramesRendered;
			return seek == -1 ? rendered : seek;
		}

		inline AVAudioFramePosition TotalFrames() const
		{
			double sampleRateRatio = mConverter.outputFormat.sampleRate / mConverter.inputFormat.sampleRate;
			return (AVAudioFramePosition)(mTotalFrames * sampleRateRatio);
		}

		bool DecodeAudio(AVAudioPCMBuffer *buffer, NSError **error = nullptr)
		{
			__block NSError *err = nil;
			AVAudioConverterOutputStatus status = [mConverter convertToBuffer:buffer error:error withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
				if(this->mFramesDecoded == 0)
					mFlags.fetch_or(eDecoderStateDataFlagDecodingStarted);

				BOOL result = [mDecoder decodeIntoBuffer:mDecodeBuffer frameLength:inNumberOfPackets error:&err];
				if(!result && err)
					os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);

				this->mFramesDecoded += mDecodeBuffer.frameLength;

				if(result && mDecodeBuffer.frameLength == 0) {
					mFlags.fetch_or(eDecoderStateDataFlagDecodingFinished);
					*outStatus = AVAudioConverterInputStatus_EndOfStream;
				}
				else
					*outStatus = AVAudioConverterInputStatus_HaveData;

				return mDecodeBuffer;
			}];

			mFramesConverted += buffer.frameLength;

			if(status == AVAudioConverterOutputStatus_Error) {
				if(error)
					*error = err;
				return false;
			}

			return true;
		}

		void AllocateBuffers(AVAudioFormat *format, AVAudioFrameCount frameCount = 1024)
		{
			DeallocateBuffers();

			mConverter = [[AVAudioConverter alloc] initFromFormat:mDecoder.processingFormat toFormat:format];
			AVAudioFrameCount conversionBufferLength = (AVAudioFrameCount)((mConverter.inputFormat.sampleRate / mConverter.outputFormat.sampleRate) * frameCount);
			mDecodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.inputFormat frameCapacity:conversionBufferLength];

			for(size_t i = 0; i < mBufferCount; ++i) {
				Buffer *buffer = new Buffer;
				buffer->mBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:mConverter.outputFormat frameCapacity:frameCount];
				OSAtomicEnqueue(&mQueue, buffer, offsetof(Buffer, mLink));
			}
		}

		void DeallocateBuffers()
		{
			Buffer *buffer;
			while((buffer = (Buffer *)OSAtomicDequeue(&mQueue, offsetof(Buffer, mLink))))
				delete buffer;

			mConverter = nil;
			mDecodeBuffer = nil;
		}

		AVAudioPCMBuffer * DequeueBuffer()
		{
			Buffer *buffer = (Buffer *)OSAtomicDequeue(&mQueue, offsetof(Buffer, mLink));
			if(buffer) {
				AVAudioPCMBuffer *buf = buffer->mBuffer;
				delete buffer;
				return buf;
			}
			return nullptr;
		}

		void ReturnBuffer(AVAudioPCMBuffer *buf)
		{
			Buffer *buffer = new Buffer;
			buffer->mBuffer = buf;
			OSAtomicEnqueue(&mQueue, buffer, offsetof(Buffer, mLink));
		}
	};

}

namespace {

	//! A single decoder for processing
	struct SFBAudioPlayerDecoderQueueElement {
		struct SFBAudioPlayerDecoderQueueElement *mLink;
		SFBAudioDecoder *mDecoder;
	};

	static inline void EnqueueElement(OSFifoQueueHead *list, SFBAudioPlayerDecoderQueueElement *element)
	{
		OSAtomicFifoEnqueue(list, element, offsetof(SFBAudioPlayerDecoderQueueElement, mLink));
	}

	static inline SFBAudioPlayerDecoderQueueElement * DequeueElement(OSFifoQueueHead *list)
	{
		return (SFBAudioPlayerDecoderQueueElement *)OSAtomicFifoDequeue(list, offsetof(SFBAudioPlayerDecoderQueueElement, mLink));
	}

}

@interface SFBAudioPlayer ()
{
@private
	AVAudioEngine 			*_engine;
	AVAudioPlayerNode		*_player;
	std::atomic_uint		_flags;
	OSFifoQueueHead			_fifo;
	dispatch_queue_t		_engineQueue;
	SFB::Semaphore			_semaphore;
	std::thread				_decoderThread;
	std::atomic_uint64_t	_timeStamp;

	std::shared_ptr<SFBAudioDecoderStateData> _decoderState;
}

@end

@implementation SFBAudioPlayer

- (instancetype) init
{
	if((self = [super init])) {
		_engineQueue = dispatch_queue_create("org.sbooth.AudioEngine.Player", DISPATCH_QUEUE_SERIAL);
		if(_engineQueue == NULL) {
			os_log_error(OS_LOG_DEFAULT, "dispatch_queue_create failed");
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

		_engine = [[AVAudioEngine alloc] init];
		_player = [[AVAudioPlayerNode alloc] init];
		[_engine attachNode:_player];
		[_engine connect:_player to:_engine.mainMixerNode format:nil];
	}
	return self;
}

- (void)dealloc
{
	_flags.fetch_or(eAudioPlayerFlagStopDecoding);
	_semaphore.Signal();

	try {
		_decoderThread.join();
	}

	catch(const std::exception& e) {
		os_log_error(OS_LOG_DEFAULT, "Unable to join decoder thread: %{public}s", e.what());
	}

	[self clearQueue];
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

	SFBAudioPlayerDecoderQueueElement *element = new SFBAudioPlayerDecoderQueueElement;
	element->mDecoder = decoder;

	EnqueueElement(&_fifo, element);
	_semaphore.Signal();

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

	SFBAudioPlayerDecoderQueueElement *element = new SFBAudioPlayerDecoderQueueElement;
	element->mDecoder = decoder;

	EnqueueElement(&_fifo, element);
	_semaphore.Signal();

	return YES;
}

- (BOOL)skipToNext
{
	auto decoderState = std::atomic_load(&_decoderState);
	if(decoderState) {
		decoderState->mFlags.fetch_or(eDecoderStateDataFlagStopDecoding);
		_semaphore.Signal();
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
	SFBAudioPlayerDecoderQueueElement *element;
	while((element = DequeueElement(&_fifo)))
		delete element;
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
		_semaphore.Signal();
	}

	return YES;
}

- (BOOL)playPauseReturningError:(NSError **)error
{
	return self.isPlaying ? [self pauseReturningError:error] : [self playReturningError:error];
}

#pragma mark Player State

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
	if(decoderState) {
		return { .currentFrame = decoderState->ApparentFrame(), .totalFrames = decoderState->TotalFrames() };
	}
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
		int64_t totalFrames = decoderState->TotalFrames();
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
		currentPlaybackPosition = { .currentFrame = decoderState->ApparentFrame(), .totalFrames = decoderState->TotalFrames() };
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

	if(targetFrame >= decoderState->TotalFrames())
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

	if(targetFrame >= decoderState->TotalFrames())
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

	AVAudioFramePosition totalFrames = decoderState->TotalFrames();
	return [self seekToFrame:(AVAudioFramePosition)(totalFrames * position)];
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame
{
	NSParameterAssert(frame >= 0);

	auto decoderState = std::atomic_load(&_decoderState);
	if(!decoderState)
		return NO;

	if(frame >= decoderState->TotalFrames())
		return NO;

	decoderState->mFrameToSeek = frame;
	_semaphore.Signal();

	return YES;

}

- (BOOL)supportsSeeking
{
	auto decoderState = std::atomic_load(&_decoderState);
	return decoderState ? decoderState->mDecoder.supportsSeeking : NO;
}

#pragma mark AVAudioEngine Access

- (void)withEngine:(SFBAudioPlayerAVAudioEngineBlock)block
{
	dispatch_sync(_engineQueue, ^{
		block(_engine);
	});
}

#pragma mark Decoding Thread

- (void *)decoderThreadEntry
{
	os_log_info(OS_LOG_DEFAULT, "Decoder thread starting");

	AVAudioFramePosition framePosition = 0;

	while(!(_flags & eAudioPlayerFlagStopDecoding)) {

		// Dequeue and process the next decoder

		// The element was allocated using new in -playDecoder:error: or -enqueueDecoder:error:
		// Use a unique_ptr here to assume ownership and ensure deletion
		std::unique_ptr<SFBAudioPlayerDecoderQueueElement> element(DequeueElement(&_fifo));
		if(element) {
			// Open the decoder if necessary
			NSError *error;
			if(!element->mDecoder.isOpen && ![element->mDecoder openReturningError:&error]) {
				if(_decodingErrorNotificationHandler)
					_decodingErrorNotificationHandler(element->mDecoder, error);

				if(error)
					os_log_error(OS_LOG_DEFAULT, "Error opening decoder: %{public}@", error);

				continue;
			}

			// Create the decoder state
			auto decoderState = std::make_shared<SFBAudioDecoderStateData>(element->mDecoder);

			// NB: The decoder may return an estimate of the total frames
			decoderState->mTotalFrames = decoderState->mDecoder.totalFrames;

			// The bus format is the format _player is expected to output
			// In the even the bus and decoder processing formats don't match, conversion will
			// be performed in SFBAudioDecoderStateData::DecodeAudio()
			__block AVAudioFormat *busFormat;
			dispatch_sync(_engineQueue, ^{
				busFormat = [_player outputFormatForBus:0];
			});

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
			while(!(_flags & eAudioPlayerFlagStopDecoding) && !(decoderState->mFlags & eDecoderStateDataFlagStopDecoding)) {
				if(decoderState->mFlags & eDecoderStateDataFlagDecodingFinished) {
					os_log_info(OS_LOG_DEFAULT, "Decoding finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

					// Some formats (MP3) may not know the exact number of frames in advance
					// without processing the entire file, which is a potentially slow operation
					decoderState->mTotalFrames = decoderState->mDecoder.totalFrames;

					// Perform the decoding finished callback
					if(_decodingFinishedNotificationHandler)
						_decodingFinishedNotificationHandler(decoderState->mDecoder);

					break;
				}

				AVAudioFramePosition seekOffset = decoderState->mFrameToSeek;

				// Seek to the specified frame
				if(seekOffset != -1) {
					// Stop _player to unschedule any pending buffers
					__block BOOL wasPlaying;
					dispatch_sync(_engineQueue, ^{
						wasPlaying = _player.isPlaying;
						[_player stop];
					});

					double sampleRateRatio = decoderState->mConverter.inputFormat.sampleRate / decoderState->mConverter.outputFormat.sampleRate;
					AVAudioFramePosition adjustedSeekOffset = (AVAudioFramePosition)(seekOffset * sampleRateRatio);

					os_log_debug(OS_LOG_DEFAULT, "Seek to frame %lld requested, actually seeking to frame %lld", seekOffset, adjustedSeekOffset);

					if([decoderState->mDecoder seekToFrame:adjustedSeekOffset error:nil])
						// Reset the converter to flush any buffers
						[decoderState->mConverter reset];
					else
						os_log_debug(OS_LOG_DEFAULT, "Error seeking to frame %lld", adjustedSeekOffset);

					AVAudioFramePosition newFrame = decoderState->mDecoder.currentFrame;
					if(newFrame != adjustedSeekOffset)
						os_log_debug(OS_LOG_DEFAULT, "Inaccurate seek to frame %lld, got frame %lld", adjustedSeekOffset, newFrame);

					// Update the seek request
					decoderState->mFrameToSeek = -1;

					// Update the frame counters accordingly
					// A seek is handled in essentially the same way as initial playback
					if(newFrame != -1) {
						decoderState->mFramesDecoded = adjustedSeekOffset;
						decoderState->mFramesConverted = seekOffset;
						decoderState->mFramesScheduled = 0;
						decoderState->mFramesRendered = seekOffset;
					}

					if(wasPlaying)
						dispatch_sync(_engineQueue, ^{
							[_player play];
						});
				}

				// Dequeue the next available buffer for scheduling
				AVAudioPCMBuffer *buf = decoderState->DequeueBuffer();
				if(buf) {
					// Decode audio into the buffer, converting to the bus format in the process
					if(!decoderState->DecodeAudio(buf, &error)) {
						os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", error);
						decoderState->ReturnBuffer(buf);
					}

					// Schedule the buffer for playback
					if(buf.frameLength) {
//						__block AVAudioTime *lastRenderTime;
//						dispatch_sync(_engineQueue, ^{
//							lastRenderTime = [_player lastRenderTime];
//						});

						AVAudioTime *time = [[AVAudioTime alloc] initWithSampleTime:framePosition + decoderState->mFramesScheduled atRate:buf.format.sampleRate];

//						os_log_info(OS_LOG_DEFAULT, "Scheduling %d frames at %{public}@", buf.frameLength, time);

						// Schedule the buffer on the AVAudioPlayerNode
						dispatch_sync(_engineQueue, ^{
							[_player scheduleBuffer:buf
											 atTime:time
											options:0
							 completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
								  completionHandler:^(AVAudioPlayerNodeCompletionCallbackType /*callbackType*/) {
								// The completion handler is called on a dedicated callback queue by AVAudioPlayerNode

								// Perform the rendering started callback
								if(decoderState->mFramesRendered == 0 && !(decoderState->mFlags & eDecoderStateDataFlagRenderingStarted)) {
									os_log_info(OS_LOG_DEFAULT, "Rendering started for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

									decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingStarted);

									// Store the decoder state for communication between threads
									std::atomic_store(&self->_decoderState, decoderState);

									// Perform the rendering started callback
									if(self->_renderingStartedNotificationHandler)
										self->_renderingStartedNotificationHandler(decoderState->mDecoder);
								}

								decoderState->mFramesRendered += buf.frameLength;

								// Perform the rendering finished callback
								if(decoderState->mFramesRendered == decoderState->mFramesConverted && decoderState->mFlags & eDecoderStateDataFlagDecodingFinished) {
									os_log_info(OS_LOG_DEFAULT, "Rendering finished for \"%{public}@\"", [[NSFileManager defaultManager] displayNameAtPath:decoderState->mDecoder.inputSource.url.path]);

									decoderState->mFlags.fetch_or(eDecoderStateDataFlagRenderingFinished);
									std::atomic_store(&self->_decoderState, std::shared_ptr<SFBAudioDecoderStateData>{});

									// Perform the rendering finished callback
									if(self->_renderingFinishedNotificationHandler)
										self->_renderingFinishedNotificationHandler(decoderState->mDecoder);
								}

								// Return the buffer to the decoder state for reuse
								decoderState->ReturnBuffer(buf);
								self->_semaphore.Signal();
							}];
						});

						decoderState->mFramesScheduled += buf.frameLength;
					}
				}
				// No buffers available, wait for one to become available
				else
					_semaphore.TimedWait(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
			}
		}

		// Wait for another element to be enqueued
		_semaphore.TimedWait(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
	}

	os_log_info(OS_LOG_DEFAULT, "Decoder thread terminating");

	return nullptr;
}

@end
