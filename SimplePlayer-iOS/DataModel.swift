//
// Copyright (c) 2011 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI
import Combine
import SFBAudioEngine

/// The data model for the SimplePlayer app
class DataModel: NSObject, ObservableObject {
	/// The available tracks
	@Published var tracks: [Track] = []
	/// The track now playing
	@Published var nowPlaying: Track? = nil
	/// The player's playback state
	@Published var playbackState: AudioPlayer.PlaybackState = .stopped

	/// The underlying audio player
	let player = AudioPlayer()

	// Subjects converting delegate messages
	private lazy var nowPlayingSubject = CurrentValueSubject<Track?, Never>(nil)
	private lazy var playbackStateSubject = CurrentValueSubject<AudioPlayer.PlaybackState, Never>(.stopped)

	override init() {
		super.init()
		player.delegate = self
		nowPlayingSubject
			.receive(on: DispatchQueue.main)
			.assign(to: &$nowPlaying)
		playbackStateSubject
			.receive(on: DispatchQueue.main)
			.assign(to: &$playbackState)
	}

	func load() {
		DispatchQueue.global(qos: .background).async {
			var tracks: [Track] = []
			for pathExtension in AudioDecoder.supportedPathExtensions {
				if let urls = Bundle.main.urls(forResourcesWithExtension: pathExtension, subdirectory: nil) {
					for url in urls {
						tracks.append(Track(url: url))
					}
				}
			}
			DispatchQueue.main.async {
				self.tracks = tracks
			}
		}
	}

	func seekBackward() {
		player.seekBackward()
	}

	func seekForward() {
		player.seekForward()
	}

	func togglePlayPause() {
		try? player.togglePlayPause()
	}
}

extension DataModel: AudioPlayer.Delegate {
	func audioPlayerNowPlayingChanged(_ audioPlayer: AudioPlayer) {
		if let nowPlaying = audioPlayer.nowPlaying, let track = tracks.first(where: { $0.url == nowPlaying.inputSource.url }){
			nowPlayingSubject.send(track)
		} else {
			nowPlayingSubject.send(nil)
		}
	}

	func audioPlayerPlaybackStateChanged(_ audioPlayer: AudioPlayer) {
		playbackStateSubject.send(audioPlayer.playbackState)
	}
}

