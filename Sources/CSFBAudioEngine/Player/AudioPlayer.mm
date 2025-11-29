//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <atomic>
#import <cassert>
#import <cmath>
#import <mutex>

#import <objc/runtime.h>

#import <AVAudioFormat+SFBFormatTransformation.h>

#import "AudioPlayer.h"

#import "HostTimeUtilities.hpp"
#import "SFBAudioDecoder.h"
#import "SFBCStringForOSType.h"
#import "StringDescribingAVAudioFormat.h"

namespace SFB {

const os_log_t AudioPlayer::sLog = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

} /* namespace SFB */

namespace {

/// Objective-C associated object key indicating if a decoder has been canceled
constexpr char _decoderIsCanceledKey = '\0';

void AVAudioEngineConfigurationChangeNotificationCallback(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo)
{
	auto that = static_cast<SFB::AudioPlayer *>(observer);
	that->HandleAudioEngineConfigurationChange((__bridge AVAudioEngine *)object, (__bridge NSDictionary *)userInfo);
}

#if TARGET_OS_IPHONE
void AVAudioSessionInterruptionNotificationCallback(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo)
{
	auto that = static_cast<SFB::AudioPlayer *>(observer);
	that->HandleAudioSessionInterruption((__bridge NSDictionary *)userInfo);
}
#endif /* TARGET_OS_IPHONE */

#if !TARGET_OS_IPHONE
/// Returns the name of `audioUnit.deviceID`
///
/// This is the value of `kAudioObjectPropertyName` in the output scope on the main element
NSString * _Nullable AudioDeviceName(AUAudioUnit * _Nonnull audioUnit) noexcept
{
#if DEBUG
	assert(audioUnit != nil);
#endif /* DEBUG */

	AudioObjectPropertyAddress address = {
		.mSelector = kAudioObjectPropertyName,
		.mScope = kAudioObjectPropertyScopeOutput,
		.mElement = kAudioObjectPropertyElementMain
	};
	CFStringRef name = nullptr;
	UInt32 dataSize = sizeof(name);
	const auto result = AudioObjectGetPropertyData(audioUnit.deviceID, &address, 0, nullptr, &dataSize, &name);
	if(result != noErr) {
		os_log_error(SFB::AudioPlayer::sLog, "AudioObjectGetPropertyData (kAudioObjectPropertyName, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}
	return (__bridge_transfer NSString *)name;
}
#endif /* !TARGET_OS_IPHONE */

} /* namespace */

// MARK: - AudioPlayer

SFB::AudioPlayer::AudioPlayer()
{
	// Create the audio processing graph
	mEngine = [[AVAudioEngine alloc] init];
	AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
	if(!ConfigureProcessingGraphForFormat(format, false)) {
		os_log_error(sLog, "Unable to create audio processing graph for 44.1 kHz stereo");
		throw std::runtime_error("ConfigureProcessingGraphForFormat failed");
	}

	// Register for configuration change notifications
	auto notificationCenter = CFNotificationCenterGetLocalCenter();
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioEngineConfigurationChangeNotificationCallback, (__bridge CFStringRef)AVAudioEngineConfigurationChangeNotification, (__bridge void *)mEngine, CFNotificationSuspensionBehaviorDeliverImmediately);

	// Create the dispatch queue used for event processing
	auto attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
	if(!attr) {
		os_log_error(sLog, "dispatch_queue_attr_make_with_qos_class failed");
		throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
	}

	mEventQueue = dispatch_queue_create_with_target("AudioPlayer.Events", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
	if(!mEventQueue) {
		os_log_error(sLog, "Unable to create event dispatch queue: dispatch_queue_create failed");
		throw std::runtime_error("dispatch_queue_create_with_target failed");
	}

#if TARGET_OS_IPHONE
	// Register for audio session interruption notifications
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioSessionInterruptionNotificationCallback, (__bridge CFStringRef)AVAudioSessionInterruptionNotification, (__bridge void *)[AVAudioSession sharedInstance], CFNotificationSuspensionBehaviorDeliverImmediately);
#endif /* TARGET_OS_IPHONE */

#if DEBUG
	assert(mPlayerNode != nil);
#endif /* DEBUG */
}

SFB::AudioPlayer::~AudioPlayer() noexcept
{
	auto notificationCenter = CFNotificationCenterGetLocalCenter();
	CFNotificationCenterRemoveEveryObserver(notificationCenter, this);
}

// MARK: - Playlist Management

bool SFB::AudioPlayer::EnqueueDecoder(Decoder decoder, bool forImmediatePlayback, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error])
		return false;

	auto configureForAndEnqueueDecoder = [&](bool clearQueueAndReset) -> BOOL {
		mFlags.fetch_or(static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		const auto result = ConfigureForAndEnqueueDecoder(decoder, clearQueueAndReset, error);
		if(!result)
			mFlags.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		return result;
	};

	// Reconfigure the audio processing graph for the decoder's processing format if requested
	if(forImmediatePlayback)
		return configureForAndEnqueueDecoder(true);

	// To preserve the order of enqueued decoders, when the internal queue is not empty
	// push all decoders there regardless of format compatibility with the player node
	// This prevents incorrect playback order arising from the scenario where
	// decoders A and AA have formats supported by the player node and decoder B does not;
	// bypassing the internal queue for supported formats when enqueueing A, B, AA
	// would result in playback order A, AA, B

	if(InternalDecoderQueueIsEmpty()) {
		// Enqueue the decoder on mPlayerNode if the decoder's processing format is supported
		if(mPlayerNode->_node->SupportsFormat(decoder.processingFormat)) {
			mFlags.fetch_or(static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
			const auto result = mPlayerNode->_node->EnqueueDecoder(decoder, false, error);
			if(!result)
				mFlags.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
			return result;
		}

		// Reconfigure the audio processing graph for the decoder's processing format
		// only if the player node does not have a current decoder
		if(!mPlayerNode->_node->CurrentDecoder())
			return configureForAndEnqueueDecoder(false);

		// mPlayerNode has a current decoder; fall through and push the decoder to the internal queue
	}

	// Otherwise push the decoder to the internal queue
	if(!PushDecoderToInternalQueue(decoder)) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return false;
	}

	return true;
}

// MARK: - Playback Control

bool SFB::AudioPlayer::Play(NSError **error) noexcept
{
	if((mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) && PlayerNodeIsPlaying())
		return true;

	if(NSError *err = nil; ![mEngine startAndReturnError:&err]) {
		mFlags.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
		os_log_error(sLog, "Error starting AVAudioEngine: %{public}@", err);
		if(error)
			*error = err;
		return false;
	}

	mFlags.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
	mPlayerNode->_node->Play();

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Play()");
#endif /* DEBUG */

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[mPlayer.delegate audioPlayer:mPlayer playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];

	return true;
}

void SFB::AudioPlayer::Pause() noexcept
{
	if(!((mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) && PlayerNodeIsPlaying()))
		return;

	mPlayerNode->_node->Pause();

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePaused && "Incorrect playback state in Pause()");
#endif /* DEBUG */

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[mPlayer.delegate audioPlayer:mPlayer playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
}

void SFB::AudioPlayer::Resume() noexcept
{
	if(!((mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) && !PlayerNodeIsPlaying()))
		return;

	mPlayerNode->_node->Play();

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Resume()");
#endif /* DEBUG */

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[mPlayer.delegate audioPlayer:mPlayer playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
}

void SFB::AudioPlayer::Stop() noexcept
{
	if(!(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	[mEngine stop];
	mFlags.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
	mPlayerNode->_node->Stop();

	ClearInternalDecoderQueue();

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStateStopped && "Incorrect playback state in Stop()");
#endif /* DEBUG */

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[mPlayer.delegate audioPlayer:mPlayer playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
}

bool SFB::AudioPlayer::TogglePlayPause(NSError **error) noexcept
{
	const auto playbackState = PlaybackState();
	switch(playbackState) {
		case SFBAudioPlayerPlaybackStatePlaying:
			Pause();
			return true;
		case SFBAudioPlayerPlaybackStatePaused:
			Resume();
			return true;
		case SFBAudioPlayerPlaybackStateStopped:
			return Play(error);
	}
}

void SFB::AudioPlayer::Reset() noexcept
{
	mPlayerNode->_node->Reset();
	[mEngine reset];

	ClearInternalDecoderQueue();
}

// MARK: - Player State

bool SFB::AudioPlayer::EngineIsRunning() const noexcept
{
	const auto isRunning = mEngine.isRunning;
#if DEBUG
		assert(static_cast<bool>(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) == isRunning && "Cached value for mEngine.isRunning invalid");
#endif /* DEBUG */
	return isRunning;
}

void SFB::AudioPlayer::SetNowPlaying(Decoder nowPlaying) noexcept
{
	Decoder previouslyPlaying = nil;
	{
		std::lock_guard lock(mNowPlayingLock);
		if(mNowPlaying == nowPlaying)
			return;
		previouslyPlaying = mNowPlaying;
		mNowPlaying = nowPlaying;
	}

	os_log_debug(sLog, "Now playing changed to %{public}@", nowPlaying);

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:previouslyPlaying:)])
		[mPlayer.delegate audioPlayer:mPlayer nowPlayingChanged:nowPlaying previouslyPlaying:previouslyPlaying];
}

#if !TARGET_OS_IPHONE

// MARK: - Volume Control

float SFB::AudioPlayer::VolumeForChannel(AudioObjectPropertyElement channel) const noexcept
{
	AudioUnitParameterValue volume;
	const auto result = AudioUnitGetParameter(mEngine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(result != noErr) {
		os_log_error(sLog, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		return std::nanf("1");
	}

	return volume;
}

bool SFB::AudioPlayer::SetVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept
{
	os_log_info(sLog, "Setting volume for channel %u to %g", channel, volume);

	const auto result = AudioUnitSetParameter(mEngine.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(result != noErr) {
		os_log_error(sLog, "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return false;
	}

	return true;
}

// MARK: - Output Device

AUAudioObjectID SFB::AudioPlayer::OutputDeviceID() const noexcept
{
	return mEngine.outputNode.AUAudioUnit.deviceID;
}

bool SFB::AudioPlayer::SetOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept
{
	os_log_info(sLog, "Setting output device to 0x%x", outputDeviceID);

	if(NSError *err = nil; ![mEngine.outputNode.AUAudioUnit setDeviceID:outputDeviceID error:&err]) {
		os_log_error(sLog, "Error setting output device: %{public}@", err);
		if(error)
			*error = err;
		return false;
	}

	return true;
}

#endif /* !TARGET_OS_IPHONE */

// MARK: - Debugging

void SFB::AudioPlayer::LogProcessingGraphDescription(os_log_t log, os_log_type_t type) const noexcept
{
	NSMutableString *string = [NSMutableString stringWithFormat:@"<AudioPlayer: %p> audio processing graph:\n", this];

	const auto playerNode = mPlayerNode;
	const auto engine = mEngine;

	AVAudioFormat *inputFormat = playerNode.renderingFormat;
	[string appendFormat:@"↓ rendering\n    %@\n", SFB::StringDescribingAVAudioFormat(inputFormat)];

	AVAudioFormat *outputFormat = [playerNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@\n", playerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@\n", playerNode];

	AVAudioConnectionPoint *connectionPoint = [[engine outputConnectionPointsForNode:playerNode outputBus:0] firstObject];
	while(connectionPoint.node != engine.mainMixerNode) {
		inputFormat = [connectionPoint.node inputFormatForBus:connectionPoint.bus];
		outputFormat = [connectionPoint.node outputFormatForBus:connectionPoint.bus];
		if(![outputFormat isEqual:inputFormat])
			[string appendFormat:@"→ %@\n    %@\n", connectionPoint.node, SFB::StringDescribingAVAudioFormat(outputFormat)];
		else
			[string appendFormat:@"→ %@\n", connectionPoint.node];

		connectionPoint = [[engine outputConnectionPointsForNode:connectionPoint.node outputBus:0] firstObject];
	}

	inputFormat = [engine.mainMixerNode inputFormatForBus:0];
	outputFormat = [engine.mainMixerNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@\n", engine.mainMixerNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@\n", engine.mainMixerNode];

	inputFormat = [engine.outputNode inputFormatForBus:0];
	outputFormat = [engine.outputNode outputFormatForBus:0];
	if(![outputFormat isEqual:inputFormat])
		[string appendFormat:@"→ %@\n    %@]", engine.outputNode, SFB::StringDescribingAVAudioFormat(outputFormat)];
	else
		[string appendFormat:@"→ %@", engine.outputNode];

#if !TARGET_OS_IPHONE
	[string appendFormat:@"\n↓ \"%@\"", AudioDeviceName(engine.outputNode.AUAudioUnit)];
#endif /* !TARGET_OS_IPHONE */

	os_log_with_type(log, type, "%{public}@", string);
}

// MARK: - Decoder Queue

bool SFB::AudioPlayer::PushDecoderToInternalQueue(Decoder decoder) noexcept
{
	try {
		std::lock_guard lock(mQueueLock);
		mQueuedDecoders.push_back(decoder);
	}
	catch(const std::exception& e) {
		os_log_error(sLog, "Error pushing %{public}@ to mQueuedDecoders: %{public}s", decoder, e.what());
		return false;
	}

	os_log_info(sLog, "Pushed %{public}@", decoder);

	return true;
}

SFB::AudioPlayer::Decoder SFB::AudioPlayer::PopDecoderFromInternalQueue() noexcept
{
	Decoder decoder = nil;
	std::lock_guard lock(mQueueLock);
	if(!mQueuedDecoders.empty()) {
		decoder = mQueuedDecoders.front();
		mQueuedDecoders.pop_front();
	}
	os_log_info(sLog, "Popped %{public}@", decoder);
	return decoder;
}

void SFB::AudioPlayer::HandleAudioEngineConfigurationChange(AVAudioEngine *engine, NSDictionary *userInfo) noexcept
{
	if(engine != mEngine) {
		os_log_fault(sLog, "AVAudioEngineConfigurationChangeNotification received for incorrect AVAudioEngine instance");
		return;
	}

	// AVAudioEngine posts this notification from a dedicated queue
	os_log_debug(sLog, "Received AVAudioEngineConfigurationChangeNotification");

	// AVAudioEngine stops itself when interrupted and there is no way to determine if the engine was
	// running before this notification was issued unless the state is cached
	const bool engineWasRunning = mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning);
	mFlags.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

	// Attempt to preserve the playback state
	const auto playerNodeWasPlaying = mPlayerNode->_node->IsPlaying();

	mPlayerNode->_node->Pause();

	// Force an update of the audio processing graph
	if(!ConfigureProcessingGraphForFormat(mPlayerNode->_node->RenderingFormat(), true)) {
		os_log_error(sLog, "Unable to create audio processing graph for %{public}@", SFB::StringDescribingAVAudioFormat(mPlayerNode->_node->RenderingFormat()));
		// The graph is not in a working state
		if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
			NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			[mPlayer.delegate audioPlayer:mPlayer encounteredError:error];
		}
		return;
	}

	// Restart AVAudioEngine if previously running
	if(engineWasRunning) {
		if(NSError *error = nil; ![mEngine startAndReturnError:&error]) {
			os_log_error(sLog, "Error starting AVAudioEngine: %{public}@", error);
//			if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
//				[mPlayer.delegate audioPlayer:mPlayer encounteredError:error];
			return;
		}

		mFlags.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

		// Restart the player node if needed
		if(playerNodeWasPlaying)
			mPlayerNode->_node->Play();
	}

	if((engineWasRunning != static_cast<bool>(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) || playerNodeWasPlaying != mPlayerNode->_node->IsPlaying()) && [mPlayer.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[mPlayer.delegate audioPlayer:mPlayer playbackStateChanged:PlaybackState()];

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayerAVAudioEngineConfigurationChange:)])
		[mPlayer.delegate audioPlayerAVAudioEngineConfigurationChange:mPlayer];
}

#if TARGET_OS_IPHONE
void SFB::AudioPlayer::HandleAudioSessionInterruption(NSDictionary *userInfo) noexcept
{
	const auto interruptionType = [[userInfo objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
	switch(interruptionType) {
		case AVAudioSessionInterruptionTypeBegan:
			os_log_debug(sLog, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeBegan)");
			Pause();
			break;

		case AVAudioSessionInterruptionTypeEnded:
			os_log_debug(sLog, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

			// AVAudioEngine stops itself when AVAudioSessionInterruptionNotification is received
			// However, Flags::engineIsRunning indicates if the engine was running before the interruption
			if(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) {
				mFlags.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
				if(NSError *error = nil; ![mEngine startAndReturnError:&error]) {
					os_log_error(sLog, "Error starting AVAudioEngine: %{public}@", error);
					return;
				}
				mFlags.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
			}
			break;

		default:
			os_log_error(sLog, "Unknown value %lu for AVAudioSessionInterruptionTypeKey", static_cast<unsigned long>(interruptionType));
			break;
	}
}
#endif /* TARGET_OS_IPHONE */

bool SFB::AudioPlayer::ConfigureForAndEnqueueDecoder(Decoder decoder, bool clearQueueAndReset, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
#endif /* DEBUG */

	// Attempt to preserve the playback state
	const bool engineWasRunning = mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning);
	const auto playerNodeWasPlaying = mPlayerNode->_node->IsPlaying();

	// If the current player node doesn't support the decoder's format (required for gapless join),
	// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
	if(auto format = decoder.processingFormat; !mPlayerNode->_node->SupportsFormat(format)) {
		if(!ConfigureProcessingGraphForFormat(format, false)) {
			if(error)
				*error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			SetNowPlaying(nil);
			return false;
		}
	}

	if(clearQueueAndReset)
		ClearInternalDecoderQueue();

	const auto success = mPlayerNode->_node->EnqueueDecoder(decoder, clearQueueAndReset, error);

	// Failure is unlikely since the audio processing graph was reconfigured for the decoder's processing format
	if(!success) {
		SetNowPlaying(nil);
		return false;
	}

	// AVAudioEngine may have been stopped in `ConfigureProcessingGraphForFormat()`
	// If this is the case and it was previously running, restart it and the player node
	// as appropriate
	if(engineWasRunning && !(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning))) {
		if(NSError *err = nil; ![mEngine startAndReturnError:&err]) {
			os_log_error(sLog, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return false;
		}

		mFlags.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
		if(playerNodeWasPlaying)
			mPlayerNode->_node->Play();
	}

#if DEBUG
	assert(static_cast<bool>(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) == engineWasRunning && "Incorrect audio engine state in ConfigureForAndEnqueueDecoder()");
	assert(mPlayerNode->_node->IsPlaying() == playerNodeWasPlaying && "Incorrect player node state in ConfigureForAndEnqueueDecoder()");
#endif /* DEBUG */

	return true;
}

bool SFB::AudioPlayer::ConfigureProcessingGraphForFormat(AVAudioFormat *format, bool forceUpdate) noexcept
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	// AudioPlayerNode requires the standard format
	if(!format.isStandard) {
		AVAudioFormat *standardEquivalentFormat = [format standardEquivalent];
		if(!standardEquivalentFormat) {
			os_log_error(sLog, "Unable to convert format %{public}@ to standard equivalent", SFB::StringDescribingAVAudioFormat(format));
			return false;
		}
		format = standardEquivalentFormat;
	}

	// mPlayerNode may be nil since this method is called from the constructor
	const auto formatsEqual = mPlayerNode && [format isEqual:mPlayerNode->_node->RenderingFormat()];
	if(formatsEqual && !forceUpdate)
		return true;

	// Even if the engine isn't running, call stop to force release of any render resources
	// Empirically this is necessary when transitioning between formats with different
	// channel counts, although it seems that it shouldn't be
	[mEngine stop];
	mFlags.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

	if(mPlayerNode && mPlayerNode->_node->IsPlaying())
		mPlayerNode->_node->Stop();

	// Avoid creating a new AudioPlayerNode if not necessary
	SFBAudioPlayerNode *playerNode = nil;
	if(!formatsEqual) {
		playerNode = [[SFBAudioPlayerNode alloc] initWithFormat:format];
		if(!playerNode) {
			os_log_error(sLog, "Unable to create SFBAudioPlayerNode with format %{public}@", SFB::StringDescribingAVAudioFormat(format));
			return false;
		}

		// Avoid keeping a strong reference to `playerNode` in the event notification blocks
		// to prevent a retain cycle. This is safe in this case because the AudioPlayerNode
		// destructor synchronously waits for all event notifications to complete so the blocks
		// will never be called with an invalid reference.
		const auto& node = *(playerNode->_node);

		playerNode->_node->mDecodingStartedBlock = ^(Decoder decoder){
			HandleDecodingStarted(node, decoder);
		};
		playerNode->_node->mDecodingCompleteBlock = ^(Decoder decoder){
			HandleDecodingComplete(node, decoder);
		};

		playerNode->_node->mRenderingWillStartBlock = ^(Decoder decoder, uint64_t hostTime){
			HandleRenderingWillStart(node, decoder, hostTime);
		};
		playerNode->_node->mRenderingDecoderWillChangeBlock = ^(Decoder decoder, Decoder nextDecoder, uint64_t hostTime) {
			HandleRenderingDecoderWillChange(node, decoder, nextDecoder, hostTime);
		};
		playerNode->_node->mRenderingWillCompleteBlock = ^(Decoder decoder, uint64_t hostTime){
			HandleRenderingWillComplete(node, decoder, hostTime);
		};

		playerNode->_node->mDecoderCanceledBlock = ^(Decoder decoder, AVAudioFramePosition framesRendered) {
			HandleDecoderCanceled(node, decoder, framesRendered);
		};

		playerNode->_node->mAsynchronousErrorBlock = ^(NSError *error) {
			HandleAsynchronousError(node, error);
		};
	}

	AVAudioOutputNode *outputNode = mEngine.outputNode;
	AVAudioMixerNode *mixerNode = mEngine.mainMixerNode;

	// This class requires that the main mixer node be connected to the output node
	assert([mEngine inputConnectionPointForNode:outputNode inputBus:0].node == mixerNode && "Illegal AVAudioEngine configuration");

	AVAudioFormat *outputNodeOutputFormat = [outputNode outputFormatForBus:0];
	AVAudioFormat *mixerNodeOutputFormat = [mixerNode outputFormatForBus:0];

	const auto outputFormatsMismatch = outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount || outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate;
	if(outputFormatsMismatch) {
		os_log_debug(sLog,
					 "Mismatch between output formats for main mixer and output nodes:\n    mainMixerNode: %{public}@\n       outputNode: %{public}@",
					 SFB::StringDescribingAVAudioFormat(mixerNodeOutputFormat),
					 SFB::StringDescribingAVAudioFormat(outputNodeOutputFormat));

		[mEngine disconnectNodeInput:outputNode bus:0];

		// Reconnect the mixer and output nodes using the output node's output format
		[mEngine connect:mixerNode to:outputNode format:outputNodeOutputFormat];
	}

	if(playerNode) {
		AVAudioConnectionPoint *playerNodeOutputConnectionPoint = nil;
		if(mPlayerNode) {
			playerNodeOutputConnectionPoint = [[mEngine outputConnectionPointsForNode:mPlayerNode outputBus:0] firstObject];
			[mEngine detachNode:mPlayerNode];
		}

		// When an audio player node is deallocated the destructor synchronously waits
		// for decoder cancelation (if there is an active decoder) and then for any
		// final events to be processed and event notification blocks called.
		// The potential therefore exists to block the calling thread for a perceptible amount
		// of time, especially if the block calls take longer than ideal.
		//
		// In my measurements the baseline with an empty delegate implementation of
		// -audioPlayer:decoderCanceled:framesRendered: seems to be around 100 µsec
		//
		// Assuming there are no external references to the audio player node,
		// setting it here sends -dealloc
		mPlayerNode = playerNode;
		[mEngine attachNode:mPlayerNode];

		// Reconnect the player node to the next node in the processing chain
		// This is the mixer node in the default configuration, but additional nodes may
		// have been inserted between the player and mixer nodes. In this case allow the delegate
		// to make any necessary adjustments based on the format change if desired.
		if(playerNodeOutputConnectionPoint && playerNodeOutputConnectionPoint.node != mixerNode) {
			if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
				AVAudioNode *node = [mPlayer.delegate audioPlayer:mPlayer reconfigureProcessingGraph:mEngine withFormat:format];
				// Ensure the delegate returned a valid node
				assert(node != nil && "nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
				[mEngine connect:mPlayerNode to:node format:format];
			}
			else
				[mEngine connect:mPlayerNode to:playerNodeOutputConnectionPoint.node format:format];
		}
		else
			[mEngine connect:mPlayerNode to:mixerNode format:format];
	}

	// AVAudioMixerNode handles sample rate conversion, but it may require input buffer sizes
	// (maximum frames per slice) greater than the default for AVAudioSourceNode (1156).
	//
	// For high sample rates, the sample rate conversion can require more rendered frames than are available by default.
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the mixer don't match, adjust
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	if(format.sampleRate > outputNodeOutputFormat.sampleRate) {
		os_log_debug(sLog, "AVAudioMixerNode input sample rate (%g Hz) and output sample rate (%g Hz) don't match", format.sampleRate, outputNodeOutputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		const double ratio = format.sampleRate / outputNodeOutputFormat.sampleRate;
		const auto maximumFramesToRender = static_cast<AUAudioFrameCount>(std::ceil(512 * ratio));

		if(auto audioUnit = mPlayerNode.AUAudioUnit; audioUnit.maximumFramesToRender < maximumFramesToRender) {
			const auto renderResourcesAllocated = audioUnit.renderResourcesAllocated;
			if(renderResourcesAllocated)
				[audioUnit deallocateRenderResources];

			os_log_debug(sLog, "Adjusting SFBAudioPlayerNode's maximumFramesToRender to %u", maximumFramesToRender);
			audioUnit.maximumFramesToRender = maximumFramesToRender;

			NSError *error;
			if(renderResourcesAllocated && ![audioUnit allocateRenderResourcesAndReturnError:&error])
				os_log_error(sLog, "Error allocating AUAudioUnit render resources for SFBAudioPlayerNode: %{public}@", error);
		}
	}

#if DEBUG
	LogProcessingGraphDescription(sLog, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

	[mEngine prepare];
	return true;
}

void SFB::AudioPlayer::HandleDecodingStarted(const AudioPlayerNode& node, Decoder decoder) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale decoding started notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)])
		[mPlayer.delegate audioPlayer:mPlayer decodingStarted:decoder];

	if(const auto flags = mFlags.load(std::memory_order_acquire); (flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !((flags & static_cast<unsigned int>(Flags::engineIsRunning)) && node.IsPlaying()) && node.CurrentDecoder() == decoder) {
		mFlags.fetch_or(static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
		SetNowPlaying(decoder);
	}
	mFlags.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
}

void SFB::AudioPlayer::HandleDecodingComplete(const AudioPlayerNode& node, Decoder decoder) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale decoding complete notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)])
		[mPlayer.delegate audioPlayer:mPlayer decodingComplete:decoder];
}

void SFB::AudioPlayer::HandleRenderingWillStart(const AudioPlayerNode& node, Decoder decoder, uint64_t hostTime) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale rendering will start notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering started notification at the expected host time
	dispatch_after(hostTime, mEventQueue, ^{
		if(mPlayerNode != node.mNode) {
			os_log_debug(sLog, "Ignoring stale rendering started notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(sLog, "%{public}@ canceled after rendering will start notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(sLog, "Rendering started notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(!(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::pendingDecoderBecameActive)))
			SetNowPlaying(decoder);
		mFlags.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);

		if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[mPlayer.delegate audioPlayer:mPlayer renderingStarted:decoder];
	});

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[mPlayer.delegate audioPlayer:mPlayer renderingWillStart:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleRenderingDecoderWillChange(const AudioPlayerNode& node, Decoder decoder, Decoder nextDecoder, uint64_t hostTime) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale rendering decoder will change notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering decoder changed notification at the expected host time
	dispatch_after(hostTime, mEventQueue, ^{
		if(mPlayerNode != node.mNode) {
			os_log_debug(sLog, "Ignoring stale rendering decoder changed notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(sLog, "%{public}@ canceled after rendering decoder will change notification", decoder);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(nextDecoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(sLog, "%{public}@ canceled after rendering decoder will change notification", nextDecoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(sLog, "Rendering decoder changed notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[mPlayer.delegate audioPlayer:mPlayer renderingComplete:decoder];

		SetNowPlaying(nextDecoder);

		if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[mPlayer.delegate audioPlayer:mPlayer renderingStarted:nextDecoder];
	});

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[mPlayer.delegate audioPlayer:mPlayer renderingWillComplete:decoder atHostTime:hostTime];

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[mPlayer.delegate audioPlayer:mPlayer renderingWillStart:nextDecoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleRenderingWillComplete(const AudioPlayerNode& node, Decoder _Nonnull decoder, uint64_t hostTime) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale rendering will complete notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering completed notification at the expected host time
	dispatch_after(hostTime, mEventQueue, ^{
		if(mPlayerNode != node.mNode) {
			os_log_debug(sLog, "Ignoring stale rendering complete notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(sLog, "%{public}@ canceled after rendering will complete notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(sLog, "Rendering complete notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[mPlayer.delegate audioPlayer:mPlayer renderingComplete:decoder];

		if(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::havePendingDecoder))
			return;

		// Dequeue the next decoder
		if(id<SFBPCMDecoding> decoder = PopDecoderFromInternalQueue(); decoder) {
			NSError *error = nil;
			if(!ConfigureForAndEnqueueDecoder(decoder, false, &error)) {
				if(error && [mPlayer.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
					[mPlayer.delegate audioPlayer:mPlayer encounteredError:error];
			}
		}
		// End of audio
		else {
#if DEBUG
			os_log_debug(sLog, "End of audio reached");
#endif /* DEBUG */

			SetNowPlaying(nil);

			if([mPlayer.delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)])
				[mPlayer.delegate audioPlayerEndOfAudio:mPlayer];
			else
				Stop();
		}
	});

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[mPlayer.delegate audioPlayer:mPlayer renderingWillComplete:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleDecoderCanceled(const AudioPlayerNode& node, Decoder decoder, AVAudioFramePosition framesRendered) noexcept
{
	// Mark the decoder as canceled for any scheduled render notifications
	objc_setAssociatedObject(decoder, &_decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)])
		[mPlayer.delegate audioPlayer:mPlayer decoderCanceled:decoder framesRendered:framesRendered];

	if(mPlayerNode == node.mNode) {
		mFlags.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
		if(const auto flags = mFlags.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !(flags & static_cast<unsigned int>(Flags::engineIsRunning)))
			SetNowPlaying(nil);
	}
}

void SFB::AudioPlayer::HandleAsynchronousError(const AudioPlayerNode& node, NSError *error) noexcept
{
	if(mPlayerNode != node.mNode) {
		os_log_debug(sLog, "Ignoring stale asynchronous error notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([mPlayer.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
		[mPlayer.delegate audioPlayer:mPlayer encounteredError:error];
}
