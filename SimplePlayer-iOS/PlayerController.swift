//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Combine
import Foundation

/// A controller class converting delegate messages to `Combine` publishers
class PlayerController: NSObject {
	/// The player we manage
	let player = AudioPlayer()

	let nowPlayingPublisher = PassthroughSubject<PCMDecoding?, Never>()
	let playbackStatePublisher = PassthroughSubject<AudioPlayer.PlaybackState, Never>()

	override init() {
		super.init()
		player.delegate = self
	}
}

extension PlayerController: AudioPlayer.Delegate {
	func audioPlayerNowPlayingChanged(_ audioPlayer: AudioPlayer) {
		nowPlayingPublisher.send(audioPlayer.nowPlaying)
	}

	func audioPlayerPlaybackStateChanged(_ audioPlayer: AudioPlayer) {
		playbackStatePublisher.send(audioPlayer.playbackState)
	}
}
