/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayer {
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime) {
		var position = PlaybackPosition()
		var time = PlaybackTime()
		__getPlaybackPosition(&position, andTime: &time)
		return (position: position, time: time)
	}
}

#if os(iOS)

import Combine

/// Class implementing publishers for a player's time and position using CADisplayLink
private class AudioPlayerObserver {
	weak var player: AudioPlayer?

	let timePublisher = PassthroughSubject<AudioPlayer.PlaybackTime, Never>()
	let positionPublisher = PassthroughSubject<AudioPlayer.PlaybackPosition, Never>()

	private var displayLink: CADisplayLink!

	init(_ player: AudioPlayer) {
		self.player = player
		displayLink = CADisplayLink(target: self, selector: #selector(step))
		displayLink?.add(to: .main, forMode: .common)
	}

	deinit {
		displayLink?.invalidate()
	}

	@objc private func step() {
		if let player = self.player, player.engineIsRunning {
			let positionAndTime = player.positionAndTime
			timePublisher.send(positionAndTime.time)
			positionPublisher.send(positionAndTime.position)
		}
	}
}

private var associatedObjectKey: Void?

extension AudioPlayer {
	/// Returns a publisher for the player's playback time
	public var timePublisher: PassthroughSubject<PlaybackTime, Never> {
		return observer().timePublisher
	}

	/// Returns a publisher for the player's playback position
	public var positionPublisher: PassthroughSubject<PlaybackPosition, Never> {
		return observer().positionPublisher
	}

	// Since class extensions can't have computed properties, emulate with associated objects

//	private lazy var observer: AudioPlayerObserver = {
//		return AudioPlayerObserver(self)
//	}()

	private func observer() -> AudioPlayerObserver {
		if let observer = objc_getAssociatedObject(self, &associatedObjectKey) as? AudioPlayerObserver {
			return observer
		}

		let observer = AudioPlayerObserver(self)
		objc_setAssociatedObject(self, &associatedObjectKey, observer, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
		return observer
	}
}

#endif
