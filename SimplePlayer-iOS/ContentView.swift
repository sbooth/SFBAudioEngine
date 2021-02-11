//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

struct ContentView: View {
	let playerController: PlayerController
	let tracks: [Track]

	init(_ playerController: PlayerController) {
		self.playerController = playerController
		var tracks = [Track]()
		if let url = Bundle.main.url(forResource: "test", withExtension: "flac") {
			tracks.append(Track(url))
		}
		self.tracks = tracks
	}

	var body: some View {
		return NavigationView {
			List(tracks) { track in
				NavigationLink(destination: PlayerView(playerController, track: track)
					.onAppear(perform: { try? playerController.player.play(track.url) })
					.onDisappear(perform: { playerController.player.stop() })) {
					TrackView(track: track)
				}
			}.navigationBarTitle("Select a track")
		}
	}
}
struct ContentView_Previews: PreviewProvider {
	static var previews: some View {
		ContentView(PlayerController())
	}
}
