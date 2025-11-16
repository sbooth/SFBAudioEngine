//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <atomic>
#import <deque>
#import <memory>

#import <os/log.h>

#import <AVFAudio/AVFAudio.h>

#import <SFBOSUnfairLock.hpp>

#import "SFBAudioDecoder.h"
#import "SFBAudioPlayer.h"
#import "SFBAudioPlayerNode+Internal.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

namespace SFB {

// MARK: - AudioPlayer

/// SFBAudioPlayer implementation
class AudioPlayer final {
public:
	using unique_ptr 	= std::unique_ptr<AudioPlayer>;
	using Decoder 		= id<SFBPCMDecoding>;
	using DecoderQueue 	= std::deque<Decoder>;

	/// The shared log for all `AudioPlayer` instances
	static const os_log_t sLog;

	/// Unsafe reference to owning `SFBAudioPlayer` instance
	__unsafe_unretained SFBAudioPlayer 	*mPlayer 			{nil};

private:
	/// The underlying `AVAudioEngine` instance
	AVAudioEngine 						*mEngine 			{nil};

	/// The player driving the audio processing graph
	SFBAudioPlayerNode					*mPlayerNode 		{nil};

	/// The lock used to protect access to `mQueuedDecoders`
	mutable SFB::OSUnfairLock			mQueueLock;
	/// Decoders enqueued for non-gapless playback
	DecoderQueue 						mQueuedDecoders;

	/// The lock used to protect access to `mNowPlaying`
	mutable SFB::OSUnfairLock 			mNowPlayingLock;
	/// The currently rendering decoder
	id <SFBPCMDecoding> 				mNowPlaying 		{nil};

	/// The dispatch queue used for asynchronous events
	dispatch_queue_t					mEventQueue 		{nil};

	/// Flags
	std::atomic_uint 					mFlags 				{0};
	static_assert(std::atomic_uint::is_always_lock_free, "Lock-free std::atomic_uint required");

public:
	AudioPlayer();
	~AudioPlayer() noexcept;

	AudioPlayer(const AudioPlayer&) = delete;
	AudioPlayer(AudioPlayer&&) = delete;
	AudioPlayer& operator=(const AudioPlayer&) = delete;
	AudioPlayer& operator=(AudioPlayer&&) = delete;

	// MARK: - Playlist Management

	bool EnqueueDecoder(Decoder _Nonnull decoder, bool forImmediatePlayback, NSError **error) noexcept;

	bool FormatWillBeGaplessIfEnqueued(AVAudioFormat * _Nonnull format) const noexcept
	{
#if DEBUG
		assert(format != nil);
#endif /* DEBUG */
		return mPlayerNode->_node->SupportsFormat(format);
	}

	void ClearQueue() noexcept
	{
		mPlayerNode->_node->ClearQueue();
		ClearInternalDecoderQueue();
	}

	bool QueueIsEmpty() const noexcept
	{
		return mPlayerNode->_node->QueueIsEmpty() && InternalDecoderQueueIsEmpty();
	}

	// MARK: - Playback Control

	bool Play(NSError **error) noexcept;
	void Pause() noexcept;
	void Resume() noexcept;
	void Stop() noexcept;
	bool TogglePlayPause(NSError **error) noexcept;

	void Reset() noexcept;

	// MARK: - Player State

	bool EngineIsRunning() const noexcept;

	bool PlayerNodeIsPlaying() const noexcept
	{
		return mPlayerNode->_node->IsPlaying();
	}

	SFBAudioPlayerPlaybackState PlaybackState() const noexcept
	{
		if(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::eEngineIsRunning))
			return mPlayerNode->_node->IsPlaying() ? SFBAudioPlayerPlaybackStatePlaying : SFBAudioPlayerPlaybackStatePaused;
		else
			return SFBAudioPlayerPlaybackStateStopped;
	}

	bool IsPlaying() const noexcept
	{
		return (mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::eEngineIsRunning)) && mPlayerNode->_node->IsPlaying();
	}

	bool IsPaused() const noexcept
	{
		return (mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::eEngineIsRunning)) && !mPlayerNode->_node->IsPlaying();
	}

	bool IsStopped() const noexcept
	{
		return !(mFlags.load(std::memory_order_acquire) & static_cast<unsigned int>(Flags::eEngineIsRunning));
	}

	bool IsReady() const noexcept
	{
		return mPlayerNode->_node->IsReady();
	}

	Decoder _Nullable CurrentDecoder() const noexcept
	{
		return mPlayerNode->_node->CurrentDecoder();
	}

	Decoder _Nullable NowPlaying() const noexcept
	{
		std::lock_guard lock(mNowPlayingLock);
		return mNowPlaying;
	}

private:
	void SetNowPlaying(Decoder _Nullable nowPlaying) noexcept;

public:
	// MARK: - Playback Properties

	SFBPlaybackPosition PlaybackPosition() const noexcept
	{
		return mPlayerNode->_node->PlaybackPosition();
	}

	SFBPlaybackTime PlaybackTime() const noexcept
	{
		return mPlayerNode->_node->PlaybackTime();
	}

	bool GetPlaybackPositionAndTime(SFBPlaybackPosition * _Nullable playbackPosition, SFBPlaybackTime * _Nullable playbackTime) const noexcept
	{
		return mPlayerNode->_node->GetPlaybackPositionAndTime(playbackPosition, playbackTime);
	}

	// MARK: - Seeking

	bool SeekForward(NSTimeInterval secondsToSkip) noexcept
	{
		return mPlayerNode->_node->SeekForward(secondsToSkip);
	}
	
	bool SeekBackward(NSTimeInterval secondsToSkip) noexcept
	{
		return mPlayerNode->_node->SeekBackward(secondsToSkip);
	}

	bool SeekToTime(NSTimeInterval timeInSeconds) noexcept
	{
		return mPlayerNode->_node->SeekToTime(timeInSeconds);
	}

	bool SeekToPosition(double position) noexcept
	{
		return mPlayerNode->_node->SeekToPosition(position);
	}

	bool SeekToFrame(AVAudioFramePosition frame) noexcept
	{
		return mPlayerNode->_node->SeekToFrame(frame);
	}

	bool SupportsSeeking() const noexcept
	{
		return mPlayerNode->_node->SupportsSeeking();
	}

#if !TARGET_OS_IPHONE

	// MARK: - Volume Control

	float VolumeForChannel(AudioObjectPropertyElement channel) const noexcept;
	bool SetVolumeForChannel(float volume, AudioObjectPropertyElement channel, NSError **error) noexcept;

	// MARK: - Output Device

	AUAudioObjectID OutputDeviceID() const noexcept;
	bool SetOutputDeviceID(AUAudioObjectID outputDeviceID, NSError **error) noexcept;

#endif /* !TARGET_OS_IPHONE */

	// MARK: - AVAudioEngine

	AVAudioEngine * GetAudioEngine() const noexcept
	{
		return mEngine;
	}

	SFBAudioPlayerNode * GetPlayerNode() const noexcept
	{
		return mPlayerNode;
	}

	// MARK: - Debugging

	void LogProcessingGraphDescription(os_log_t _Nonnull log, os_log_type_t type) const noexcept;

private:
	/// Possible bits in `mFlags`
	enum class Flags : unsigned int {
		/// Cached value of `_audioEngine.isRunning`
		eEngineIsRunning				= 1u << 0,
		/// Set if there is a decoder being enqueued on the player node that has not yet started decoding
		eHavePendingDecoder				= 1u << 1,
		/// Set if the pending decoder becomes active when the player is not playing
		ePendingDecoderBecameActive		= 1u << 2,
	};

	/// Returns true if the internal queue of decoders is empty
	bool InternalDecoderQueueIsEmpty() const noexcept
	{
		std::lock_guard lock(mQueueLock);
		return mQueuedDecoders.empty();
	}

	/// Removes all decoders from the internal decoder queue
	void ClearInternalDecoderQueue() noexcept
	{
		std::lock_guard lock(mQueueLock);
		mQueuedDecoders.clear();
	}

	/// Inserts `decoder` at the end of the internal decoder queue
	bool PushDecoderToInternalQueue(Decoder _Nonnull decoder) noexcept;

	/// Removes and returns the first decoder from the internal decoder queue
	Decoder _Nullable PopDecoderFromInternalQueue() noexcept;

public:
	/// Called to process `AVAudioEngineConfigurationChangeNotification`
	void HandleAudioEngineConfigurationChange(AVAudioEngine * _Nonnull engine, NSDictionary * _Nullable userInfo) noexcept;

#if TARGET_OS_IPHONE
	/// Called to process `AVAudioSessionInterruptionNotification`
	void HandleAudioSessionInterruption(NSDictionary * _Nullable userInfo) noexcept;
#endif /* TARGET_OS_IPHONE */

private:
	/// Configures the player to render audio from `decoder` and enqueues `decoder` on the player node
	/// - parameter clearQueueAndReset: If `true` the internal decoder queue is cleared and the player node is reset
	/// - parameter error: An optional pointer to an `NSError` object to receive error information
	/// - returns: `true` if the player was successfully configured
	bool ConfigureForAndEnqueueDecoder(Decoder _Nonnull decoder, bool clearQueueAndReset, NSError **error) noexcept;

	/// Configures the audio processing graph for playback of audio with `format`, replacing the audio player node if necessary
	///
	/// This method does nothing if the current rendering format is equal to `format`
	/// - important: This stops the audio engine if reconfiguration is necessary
	/// - parameter format: The desired audio format
	/// - parameter forceUpdate: Whether the graph should be rebuilt even if the current rendering format is equal to `format`
	/// - returns: `true` if the processing graph was successfully configured
	bool ConfigureProcessingGraphForFormat(AVAudioFormat * _Nonnull format, bool forceUpdate) noexcept;

	void HandleDecodingStarted(const AudioPlayerNode& node, Decoder _Nonnull decoder) noexcept;
	void HandleDecodingComplete(const AudioPlayerNode& node, Decoder _Nonnull decoder) noexcept;
	void HandleRenderingWillStart(const AudioPlayerNode& node, Decoder _Nonnull decoder, uint64_t hostTime) noexcept;
	void HandleRenderingDecoderWillChange(const AudioPlayerNode& node, Decoder _Nonnull decoder, Decoder _Nonnull nextDecoder, uint64_t hostTime) noexcept;
	void HandleRenderingWillComplete(const AudioPlayerNode& node, Decoder _Nonnull decoder, uint64_t hostTime) noexcept;
	void HandleDecoderCanceled(const AudioPlayerNode& node, Decoder _Nonnull decoder, AVAudioFramePosition framesRendered) noexcept;
	void HandleAsynchronousError(const AudioPlayerNode& node, NSError * _Nonnull error) noexcept;
};

} /* namespace SFB */

#pragma clang diagnostic pop
