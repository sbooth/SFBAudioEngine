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

const os_log_t AudioPlayer::log_ = os_log_create("org.sbooth.AudioEngine", "AudioPlayer");

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
		os_log_error(SFB::AudioPlayer::log_, "AudioObjectGetPropertyData (kAudioObjectPropertyName, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
	engine_ = [[AVAudioEngine alloc] init];
	if(!engine_) {
		os_log_error(log_, "Unable to create AVAudioEngine instance");
		throw std::runtime_error("Unable to create AVAudioEngine");
	}

	AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
	if(!format) {
		os_log_error(log_, "Unable to create AVAudioFormat for 44.1 kHz stereo");
		throw std::runtime_error("Unable to create AVAudioFormat");
	}

	playerNode_ = CreatePlayerNode(format);
	if(!playerNode_)
		throw std::runtime_error("Unable to create audio player node");

	[engine_ attachNode:playerNode_];
	[engine_ connect:playerNode_ to:engine_.mainMixerNode format:format];
	[engine_ prepare];

	// TODO: Is it necessary to adjust the player node's maximum frames to render for 44.1?

#if DEBUG
	LogProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

	// Register for configuration change notifications
	auto notificationCenter = CFNotificationCenterGetLocalCenter();
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioEngineConfigurationChangeNotificationCallback, (__bridge CFStringRef)AVAudioEngineConfigurationChangeNotification, (__bridge void *)engine_, CFNotificationSuspensionBehaviorDeliverImmediately);

	// Create the dispatch queue used for event processing
	auto attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0);
	if(!attr) {
		os_log_error(log_, "dispatch_queue_attr_make_with_qos_class failed");
		throw std::runtime_error("dispatch_queue_attr_make_with_qos_class failed");
	}

	eventQueue_ = dispatch_queue_create_with_target("AudioPlayer.Events", attr, DISPATCH_TARGET_QUEUE_DEFAULT);
	if(!eventQueue_) {
		os_log_error(log_, "Unable to create event dispatch queue: dispatch_queue_create failed");
		throw std::runtime_error("dispatch_queue_create_with_target failed");
	}

#if TARGET_OS_IPHONE
	// Register for audio session interruption notifications
	CFNotificationCenterAddObserver(notificationCenter, this, AVAudioSessionInterruptionNotificationCallback, (__bridge CFStringRef)AVAudioSessionInterruptionNotification, (__bridge void *)[AVAudioSession sharedInstance], CFNotificationSuspensionBehaviorDeliverImmediately);
#endif /* TARGET_OS_IPHONE */
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
		flags_.fetch_or(static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		const auto result = ConfigureForAndEnqueueDecoder(decoder, clearQueueAndReset, error);
		if(!result)
			flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
		return result;
	};

	// Ensure only one decoder can be enqueued at a time
	std::lock_guard lock{lock_};

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
		// Even though the player node is being accessed it isn't necessary to (shared) lock
		// `playerNodeMutex_` because `lock_` is already taken and protects
		// any outside modifications that might be made to the player node by additional
		// enqueues or audio engine configuration changes

		// Enqueue the decoder on playerNode_ if the decoder's processing format is supported
		if(playerNode_->_node->SupportsFormat(decoder.processingFormat)) {
			flags_.fetch_or(static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
			const auto result = playerNode_->_node->EnqueueDecoder(decoder, false, error);
			if(!result)
				flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
			return result;
		}

		// Reconfigure the audio processing graph for the decoder's processing format
		// only if the player node does not have a current decoder
		if(!playerNode_->_node->CurrentDecoder())
			return configureForAndEnqueueDecoder(false);

		// playerNode_ has a current decoder; fall through and push the decoder to the internal queue
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
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning))) {
		if(NSError *err = nil; ![engine_ startAndReturnError:&err]) {
			flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return false;
		}
		flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
	}

	{
		std::shared_lock lock{playerNodeMutex_};
		if(playerNode_->_node->IsPlaying())
			return true;
		playerNode_->_node->Play();
	}

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Play()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];

	return true;
}

void SFB::AudioPlayer::Pause() noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	{
		std::shared_lock lock{playerNodeMutex_};
		if(!playerNode_->_node->IsPlaying())
			return;
		playerNode_->_node->Pause();
	}

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePaused && "Incorrect playback state in Pause()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePaused];
}

void SFB::AudioPlayer::Resume() noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	{
		std::shared_lock lock{playerNodeMutex_};
		if(playerNode_->_node->IsPlaying())
			return;
		playerNode_->_node->Play();
	}

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStatePlaying && "Incorrect playback state in Resume()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStatePlaying];
}

void SFB::AudioPlayer::Stop() noexcept
{
	if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)))
		return;

	[engine_ stop];
	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

	{
		std::shared_lock lock{playerNodeMutex_};
		playerNode_->_node->Stop();
	}

	ClearInternalDecoderQueue();

#if DEBUG
	assert(PlaybackState() == SFBAudioPlayerPlaybackStateStopped && "Incorrect playback state in Stop()");
#endif /* DEBUG */

	if([player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:SFBAudioPlayerPlaybackStateStopped];
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
	{
		std::shared_lock lock{playerNodeMutex_};
		playerNode_->_node->Reset();
	}

	[engine_ reset];

	ClearInternalDecoderQueue();
}

// MARK: - Player State

bool SFB::AudioPlayer::EngineIsRunning() const noexcept
{
	const auto isRunning = engine_.isRunning;
#if DEBUG
		assert(static_cast<bool>(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) == isRunning && "Cached value for engine_.isRunning invalid");
#endif /* DEBUG */
	return isRunning;
}

void SFB::AudioPlayer::SetNowPlaying(Decoder nowPlaying) noexcept
{
	Decoder previouslyPlaying = nil;
	{
		std::lock_guard lock(nowPlayingLock_);
		if(nowPlaying_ == nowPlaying)
			return;
		previouslyPlaying = nowPlaying_;
		nowPlaying_ = nowPlaying;
	}

	os_log_debug(log_, "Now playing changed to %{public}@", nowPlaying);

	if([player_.delegate respondsToSelector:@selector(audioPlayer:nowPlayingChanged:previouslyPlaying:)])
		[player_.delegate audioPlayer:player_ nowPlayingChanged:nowPlaying previouslyPlaying:previouslyPlaying];
}

#if !TARGET_OS_IPHONE

// MARK: - Volume Control

float SFB::AudioPlayer::VolumeForChannel(AudioObjectPropertyElement channel) const noexcept
{
	AudioUnitParameterValue volume;
	const auto result = AudioUnitGetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(result != noErr) {
		os_log_error(log_, "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		return std::nanf("1");
	}

	return volume;
}

bool SFB::AudioPlayer::SetVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept
{
	os_log_info(log_, "Setting volume for channel %u to %g", channel, volume);

	const auto result = AudioUnitSetParameter(engine_.outputNode.audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(result != noErr) {
		os_log_error(log_, "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, %u) failed: %d '%{public}.4s'", channel, result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return false;
	}

	return true;
}

// MARK: - Output Device

AUAudioObjectID SFB::AudioPlayer::OutputDeviceID() const noexcept
{
	return engine_.outputNode.AUAudioUnit.deviceID;
}

bool SFB::AudioPlayer::SetOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept
{
	os_log_info(log_, "Setting output device to 0x%x", outputDeviceID);

	if(NSError *err = nil; ![engine_.outputNode.AUAudioUnit setDeviceID:outputDeviceID error:&err]) {
		os_log_error(log_, "Error setting output device: %{public}@", err);
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

	SFBAudioPlayerNode *playerNode = nil;
	{
		std::shared_lock lock{playerNodeMutex_};
		playerNode = playerNode_;
	}
	const auto engine = engine_;

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
		std::lock_guard lock(queueLock_);
		queuedDecoders_.push_back(decoder);
	} catch(const std::exception& e) {
		os_log_error(log_, "Error pushing %{public}@ to queuedDecoders_: %{public}s", decoder, e.what());
		return false;
	}

	os_log_info(log_, "Pushed %{public}@", decoder);

	return true;
}

SFB::AudioPlayer::Decoder SFB::AudioPlayer::PopDecoderFromInternalQueue() noexcept
{
	Decoder decoder = nil;
	std::lock_guard lock(queueLock_);
	if(!queuedDecoders_.empty()) {
		decoder = queuedDecoders_.front();
		queuedDecoders_.pop_front();
	}
	if(decoder)
		os_log_info(log_, "Popped %{public}@", decoder);
	return decoder;
}

void SFB::AudioPlayer::HandleAudioEngineConfigurationChange(AVAudioEngine *engine, NSDictionary *userInfo) noexcept
{
	if(engine != engine_) {
		os_log_fault(log_, "AVAudioEngineConfigurationChangeNotification received for incorrect AVAudioEngine instance");
		return;
	}

	// AVAudioEngine posts this notification from a dedicated queue
	os_log_debug(log_, "Received AVAudioEngineConfigurationChangeNotification");

	// AVAudioEngine stops itself when interrupted and there is no way to determine if the engine was
	// running before this notification was issued unless the state is cached
	const bool engineWasRunning = flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning);
	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

	std::shared_lock lock{playerNodeMutex_};

	// Attempt to preserve the playback state
	const auto playerNodeWasPlaying = playerNode_->_node->IsPlaying();

	playerNode_->_node->Pause();

	// Update the audio processing graph
	const auto success = [&] {
		std::lock_guard lock{lock_};
		return ConfigureProcessingGraph(playerNode_->_node->RenderingFormat(), false);
	}();

	if(!success) {
		os_log_error(log_, "Unable to configure audio processing graph for %{public}@", SFB::StringDescribingAVAudioFormat(playerNode_->_node->RenderingFormat()));
		// The graph is not in a working state
		if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)]) {
			NSError *error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			[player_.delegate audioPlayer:player_ encounteredError:error];
		}
		return;
	}

	// Restart AVAudioEngine if previously running
	if(engineWasRunning) {
		if(NSError *error = nil; ![engine_ startAndReturnError:&error]) {
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", error);
//			if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
//				[player_.delegate audioPlayer:player_ encounteredError:error];
			return;
		}

		flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

		// Restart the player node if needed
		if(playerNodeWasPlaying)
			playerNode_->_node->Play();
	}

	if((engineWasRunning != static_cast<bool>(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) || playerNodeWasPlaying != playerNode_->_node->IsPlaying()) && [player_.delegate respondsToSelector:@selector(audioPlayer:playbackStateChanged:)])
		[player_.delegate audioPlayer:player_ playbackStateChanged:PlaybackState()];

	if([player_.delegate respondsToSelector:@selector(audioPlayerAVAudioEngineConfigurationChange:)])
		[player_.delegate audioPlayerAVAudioEngineConfigurationChange:player_];
}

#if TARGET_OS_IPHONE
void SFB::AudioPlayer::HandleAudioSessionInterruption(NSDictionary *userInfo) noexcept
{
	const auto interruptionType = [[userInfo objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
	switch(interruptionType) {
		case AVAudioSessionInterruptionTypeBegan:
			os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeBegan)");
			Pause();
			break;

		case AVAudioSessionInterruptionTypeEnded:
			os_log_debug(log_, "Received AVAudioSessionInterruptionNotification (AVAudioSessionInterruptionTypeEnded)");

			// AVAudioEngine stops itself when AVAudioSessionInterruptionNotification is received
			// However, Flags::engineIsRunning indicates if the engine was running before the interruption
			if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) {
				flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
				if(NSError *error = nil; ![engine_ startAndReturnError:&error]) {
					os_log_error(log_, "Error starting AVAudioEngine: %{public}@", error);
					return;
				}
				flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
			}
			break;

		default:
			os_log_error(log_, "Unknown value %lu for AVAudioSessionInterruptionTypeKey", static_cast<unsigned long>(interruptionType));
			break;
	}
}
#endif /* TARGET_OS_IPHONE */

SFBAudioPlayerNode * SFB::AudioPlayer::CreatePlayerNode(AVAudioFormat *format) noexcept
{
#if DEBUG
	assert(format != nil);
#endif /* DEBUG */

	SFBAudioPlayerNode *playerNode = [[SFBAudioPlayerNode alloc] initWithFormat:format];
	if(!playerNode) {
		os_log_error(log_, "Unable to create SFBAudioPlayerNode with format %{public}@", SFB::StringDescribingAVAudioFormat(format));
		return nil;
	}

	// Avoid keeping a strong reference to `playerNode` in the event notification blocks
	// to prevent a retain cycle. This is safe in this case because the AudioPlayerNode
	// destructor synchronously waits for all event notifications to complete so the blocks
	// will never be called with an invalid reference.
	const auto& node = *(playerNode->_node);

	playerNode->_node->decodingStartedBlock_ = ^(Decoder decoder){
		HandleDecodingStarted(node, decoder);
	};
	playerNode->_node->decodingCompleteBlock_ = ^(Decoder decoder){
		HandleDecodingComplete(node, decoder);
	};

	playerNode->_node->renderingWillStartBlock_ = ^(Decoder decoder, uint64_t hostTime){
		HandleRenderingWillStart(node, decoder, hostTime);
	};
	playerNode->_node->renderingDecoderWillChangeBlock_ = ^(Decoder decoder, Decoder nextDecoder, uint64_t hostTime) {
		HandleRenderingDecoderWillChange(node, decoder, nextDecoder, hostTime);
	};
	playerNode->_node->renderingWillCompleteBlock_ = ^(Decoder decoder, uint64_t hostTime){
		HandleRenderingWillComplete(node, decoder, hostTime);
	};

	playerNode->_node->decoderCanceledBlock_ = ^(Decoder decoder, AVAudioFramePosition framesRendered) {
		HandleDecoderCanceled(node, decoder, framesRendered);
	};

	playerNode->_node->asynchronousErrorBlock_ = ^(NSError *error) {
		HandleAsynchronousError(node, error);
	};

	return playerNode;
}

bool SFB::AudioPlayer::ConfigureForAndEnqueueDecoder(Decoder decoder, bool clearQueueAndReset, NSError **error) noexcept
{
#if DEBUG
	assert(decoder != nil);
	lock_.assert_owner();
#endif /* DEBUG */

	// Attempt to preserve the playback state
	const bool engineWasRunning = flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning);
	const auto playerNodeWasPlaying = playerNode_->_node->IsPlaying();

	// If the current player node doesn't support the decoder's format (required for gapless join),
	// reconfigure AVAudioEngine with a new SFBAudioPlayerNode with the correct format
	if(auto format = decoder.processingFormat; !playerNode_->_node->SupportsFormat(format)) {
		// AudioPlayerNode requires the standard format
		if(!format.isStandard) {
			AVAudioFormat *standardEquivalentFormat = [format standardEquivalent];
			if(!standardEquivalentFormat) {
				os_log_error(log_, "Unable to convert format %{public}@ to standard equivalent", SFB::StringDescribingAVAudioFormat(format));
				if(error)
					*error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
				return false;
			}
			format = standardEquivalentFormat;
		}

		if(!ConfigureProcessingGraph(format, true)) {
			if(error)
				*error = [NSError errorWithDomain:SFBAudioPlayerNodeErrorDomain code:SFBAudioPlayerNodeErrorCodeFormatNotSupported userInfo:nil];
			SetNowPlaying(nil);
			return false;
		}
	}

	if(clearQueueAndReset)
		ClearInternalDecoderQueue();

	// Failure is unlikely since the audio processing graph was reconfigured for the decoder's processing format
	if(!playerNode_->_node->EnqueueDecoder(decoder, clearQueueAndReset, error)) {
		SetNowPlaying(nil);
		return false;
	}

	// AVAudioEngine will be stopped if it was reconfigured
	// If it was previously running, restart it and the player node as appropriate
	if(engineWasRunning && !(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning))) {
		if(NSError *err = nil; ![engine_ startAndReturnError:&err]) {
			os_log_error(log_, "Error starting AVAudioEngine: %{public}@", err);
			if(error)
				*error = err;
			return false;
		}

		flags_.fetch_or(static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);
		if(playerNodeWasPlaying)
			playerNode_->_node->Play();
	}

#if DEBUG
	assert(static_cast<bool>(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::engineIsRunning)) == engineWasRunning && "Incorrect audio engine state in ConfigureForAndEnqueueDecoder()");
	assert(playerNode_->_node->IsPlaying() == playerNodeWasPlaying && "Incorrect player node state in ConfigureForAndEnqueueDecoder()");
#endif /* DEBUG */

	return true;
}

bool SFB::AudioPlayer::ConfigureProcessingGraph(AVAudioFormat *format, bool replacePlayerNode) noexcept
{
#if DEBUG
	assert(format != nil);
	assert(format.isStandard);
	assert(replacePlayerNode || [format isEqual:playerNode_->_node->RenderingFormat()]);
	lock_.assert_owner();
#endif /* DEBUG */

	SFBAudioPlayerNode *playerNode = nil;
	if(replacePlayerNode && !(playerNode = CreatePlayerNode(format)))
		return false;

	// Even if the engine isn't running, call stop to force release of any render resources
	// Empirically this is necessary when transitioning between formats with different
	// channel counts, although it seems that it shouldn't be
	[engine_ stop];
	flags_.fetch_and(~static_cast<unsigned int>(Flags::engineIsRunning), std::memory_order_acq_rel);

	if(playerNode_->_node->IsPlaying())
		playerNode_->_node->Stop();

	AVAudioOutputNode *outputNode = engine_.outputNode;
	AVAudioMixerNode *mixerNode = engine_.mainMixerNode;

	// This class requires that the main mixer node be connected to the output node
	assert([engine_ inputConnectionPointForNode:outputNode inputBus:0].node == mixerNode && "Illegal AVAudioEngine configuration");

	AVAudioFormat *outputNodeOutputFormat = [outputNode outputFormatForBus:0];
	AVAudioFormat *mixerNodeOutputFormat = [mixerNode outputFormatForBus:0];

	const auto outputFormatsMismatch = outputNodeOutputFormat.channelCount != mixerNodeOutputFormat.channelCount || outputNodeOutputFormat.sampleRate != mixerNodeOutputFormat.sampleRate;
	if(outputFormatsMismatch) {
		os_log_debug(log_,
					 "Mismatch between output formats for main mixer and output nodes:\n    mainMixerNode: %{public}@\n       outputNode: %{public}@",
					 SFB::StringDescribingAVAudioFormat(mixerNodeOutputFormat),
					 SFB::StringDescribingAVAudioFormat(outputNodeOutputFormat));

		[engine_ disconnectNodeInput:outputNode bus:0];

		// Reconnect the mixer and output nodes using the output node's output format
		[engine_ connect:mixerNode to:outputNode format:outputNodeOutputFormat];
	}

	SFBAudioPlayerNode *playerNodeToDealloc = nil;
	if(playerNode) {
		playerNodeToDealloc = playerNode_;

		AVAudioConnectionPoint *playerNodeOutputConnectionPoint = [[engine_ outputConnectionPointsForNode:playerNode_ outputBus:0] firstObject];
		[engine_ detachNode:playerNode_];

		{
			// Obtain a write lock for the player node in case any reads are in progress
			std::lock_guard lock{playerNodeMutex_};
			playerNode_ = playerNode;
		}
		[engine_ attachNode:playerNode_];

		// Reconnect the player node to the next node in the processing chain
		// This is the mixer node in the default configuration, but additional nodes may
		// have been inserted between the player and mixer nodes. In this case allow the delegate
		// to make any necessary adjustments based on the format change if desired.
		if(playerNodeOutputConnectionPoint && playerNodeOutputConnectionPoint.node != mixerNode) {
			if([player_.delegate respondsToSelector:@selector(audioPlayer:reconfigureProcessingGraph:withFormat:)]) {
				AVAudioNode *node = [player_.delegate audioPlayer:player_ reconfigureProcessingGraph:engine_ withFormat:format];
				// Ensure the delegate returned a valid node
				assert(node != nil && "nil AVAudioNode returned by -audioPlayer:reconfigureProcessingGraph:withFormat:");
				[engine_ connect:playerNode_ to:node format:format];
			} else
				[engine_ connect:playerNode_ to:playerNodeOutputConnectionPoint.node format:format];
		} else
			[engine_ connect:playerNode_ to:mixerNode format:format];
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
		os_log_debug(log_, "AVAudioMixerNode input sample rate (%g Hz) and output sample rate (%g Hz) don't match", format.sampleRate, outputNodeOutputFormat.sampleRate);

		// 512 is the nominal "standard" value for kAudioUnitProperty_MaximumFramesPerSlice
		const double ratio = format.sampleRate / outputNodeOutputFormat.sampleRate;
		const auto maximumFramesToRender = static_cast<AUAudioFrameCount>(std::ceil(512 * ratio));

		if(auto audioUnit = playerNode_.AUAudioUnit; audioUnit.maximumFramesToRender < maximumFramesToRender) {
			const auto renderResourcesAllocated = audioUnit.renderResourcesAllocated;
			if(renderResourcesAllocated)
				[audioUnit deallocateRenderResources];

			os_log_debug(log_, "Adjusting SFBAudioPlayerNode's maximumFramesToRender to %u", maximumFramesToRender);
			audioUnit.maximumFramesToRender = maximumFramesToRender;

			NSError *error;
			if(renderResourcesAllocated && ![audioUnit allocateRenderResourcesAndReturnError:&error])
				os_log_error(log_, "Error allocating AUAudioUnit render resources for SFBAudioPlayerNode: %{public}@", error);
		}
	}

#if DEBUG
	LogProcessingGraphDescription(log_, OS_LOG_TYPE_DEBUG);
#endif /* DEBUG */

	[engine_ prepare];

	// When an audio player node is deallocated the destructor synchronously waits
	// for decoder cancelation (if there is an active decoder) and then for any
	// final events to be processed and event notification blocks called.
	// The potential therefore exists to block the calling thread for a perceptible amount
	// of time, especially if the block calls take longer than ideal.
	//
	// Assuming there are no external references to the audio player node,
	// setting it to nil here sends -dealloc
	//
	// N.B. If the player node lock is held a deadlock will occur
	playerNodeToDealloc = nil;

	return true;
}

void SFB::AudioPlayer::HandleDecodingStarted(const AudioPlayerNode& node, Decoder decoder) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale decoding started notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([player_.delegate respondsToSelector:@selector(audioPlayer:decodingStarted:)])
		[player_.delegate audioPlayer:player_ decodingStarted:decoder];

	if(const auto flags = flags_.load(std::memory_order_acquire); (flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !((flags & static_cast<unsigned int>(Flags::engineIsRunning)) && node.IsPlaying()) && node.CurrentDecoder() == decoder) {
		flags_.fetch_or(static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
		SetNowPlaying(decoder);
	}
	flags_.fetch_and(~static_cast<unsigned int>(Flags::havePendingDecoder), std::memory_order_acq_rel);
}

void SFB::AudioPlayer::HandleDecodingComplete(const AudioPlayerNode& node, Decoder decoder) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale decoding complete notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([player_.delegate respondsToSelector:@selector(audioPlayer:decodingComplete:)])
		[player_.delegate audioPlayer:player_ decodingComplete:decoder];
}

void SFB::AudioPlayer::HandleRenderingWillStart(const AudioPlayerNode& node, Decoder decoder, uint64_t hostTime) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale rendering will start notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering started notification at the expected host time
	dispatch_after(hostTime, eventQueue_, ^{
		if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
			os_log_debug(log_, "Ignoring stale rendering started notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering will start notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(log_, "Rendering started notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if(!(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::pendingDecoderBecameActive)))
			SetNowPlaying(decoder);
		flags_.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[player_.delegate audioPlayer:player_ renderingStarted:decoder];
	});

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillStart:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleRenderingDecoderWillChange(const AudioPlayerNode& node, Decoder decoder, Decoder nextDecoder, uint64_t hostTime) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale rendering decoder will change notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering decoder changed notification at the expected host time
	dispatch_after(hostTime, eventQueue_, ^{
		if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
			os_log_debug(log_, "Ignoring stale rendering decoder changed notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering decoder will change notification", decoder);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(nextDecoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering decoder will change notification", nextDecoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(log_, "Rendering decoder changed notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[player_.delegate audioPlayer:player_ renderingComplete:decoder];

		SetNowPlaying(nextDecoder);

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingStarted:)])
			[player_.delegate audioPlayer:player_ renderingStarted:nextDecoder];
	});

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillComplete:decoder atHostTime:hostTime];

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillStart:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillStart:nextDecoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleRenderingWillComplete(const AudioPlayerNode& node, Decoder _Nonnull decoder, uint64_t hostTime) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale rendering will complete notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	// Schedule the rendering completed notification at the expected host time
	dispatch_after(hostTime, eventQueue_, ^{
		if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
			os_log_debug(log_, "Ignoring stale rendering complete notification from <AudioPlayerNode: %p>", &node);
			return;
		}

		if(NSNumber *isCanceled = objc_getAssociatedObject(decoder, &_decoderIsCanceledKey); isCanceled.boolValue) {
			os_log_debug(log_, "%{public}@ canceled after rendering will complete notification", decoder);
			return;
		}

#if DEBUG
		const auto now = SFB::GetCurrentHostTime();
		const auto delta = SFB::ConvertAbsoluteHostTimeDeltaToNanoseconds(hostTime, now);
		const auto tolerance = static_cast<uint64_t>(1e9 / node.RenderingFormat().sampleRate);
		if(delta > tolerance)
			os_log_debug(log_, "Rendering complete notification arrived %.2f msec %s", static_cast<double>(delta) / 1e6, now > hostTime ? "late" : "early");
#endif /* DEBUG */

		if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingComplete:)])
			[player_.delegate audioPlayer:player_ renderingComplete:decoder];

		if(flags_.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::havePendingDecoder))
			return;

		// Dequeue the next decoder
		if(Decoder decoder = PopDecoderFromInternalQueue(); decoder) {
			NSError *error = nil;
			const auto success = [&] {
				std::lock_guard lock{lock_};
				return ConfigureForAndEnqueueDecoder(decoder, false, &error);
			}();

			if(!success) {
				if(error && [player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
					[player_.delegate audioPlayer:player_ encounteredError:error];
			}
		} else {
			// End of audio
#if DEBUG
			os_log_debug(log_, "End of audio reached");
#endif /* DEBUG */

			SetNowPlaying(nil);

			if([player_.delegate respondsToSelector:@selector(audioPlayerEndOfAudio:)])
				[player_.delegate audioPlayerEndOfAudio:player_];
			else
				Stop();
		}
	});

	if([player_.delegate respondsToSelector:@selector(audioPlayer:renderingWillComplete:atHostTime:)])
		[player_.delegate audioPlayer:player_ renderingWillComplete:decoder atHostTime:hostTime];
}

void SFB::AudioPlayer::HandleDecoderCanceled(const AudioPlayerNode& node, Decoder decoder, AVAudioFramePosition framesRendered) noexcept
{
	// Mark the decoder as canceled for any scheduled render notifications
	objc_setAssociatedObject(decoder, &_decoderIsCanceledKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

	if([player_.delegate respondsToSelector:@selector(audioPlayer:decoderCanceled:framesRendered:)])
		[player_.delegate audioPlayer:player_ decoderCanceled:decoder framesRendered:framesRendered];

	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ == node.node_) {
		flags_.fetch_and(~static_cast<unsigned int>(Flags::pendingDecoderBecameActive), std::memory_order_acq_rel);
		if(const auto flags = flags_.load(std::memory_order_acquire); !(flags & static_cast<unsigned int>(Flags::havePendingDecoder)) && !(flags & static_cast<unsigned int>(Flags::engineIsRunning)))
			SetNowPlaying(nil);
	}
}

void SFB::AudioPlayer::HandleAsynchronousError(const AudioPlayerNode& node, NSError *error) noexcept
{
	if(std::shared_lock lock{playerNodeMutex_}; playerNode_ != node.node_) {
		os_log_debug(log_, "Ignoring stale asynchronous error notification from <AudioPlayerNode: %p>", &node);
		return;
	}

	if([player_.delegate respondsToSelector:@selector(audioPlayer:encounteredError:)])
		[player_.delegate audioPlayer:player_ encounteredError:error];
}
