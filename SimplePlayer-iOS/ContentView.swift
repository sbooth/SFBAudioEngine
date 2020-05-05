//
// Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import SwiftUI

struct ContentView: View {
	let player: AudioPlayer
	let tracks: [Track]

	init(_ player: AudioPlayer) {
		self.player = player
		var tracks = [Track]()
		if let url = Bundle.main.url(forResource: "test", withExtension: "wav") {
			tracks.append(Track(url))
		}
		self.tracks = tracks
	}

	var body: some View {
		return NavigationView {
			List(tracks) { track in
				NavigationLink(destination: PlayerView(self.player, track: track)
					.onAppear(perform: { try? self.player.play(track.url) })
					.onDisappear(perform: { self.player.stop() })) {
					TrackView(track: track)
				}
			}.navigationBarTitle("Select a track")
		}
	}
}
struct ContentView_Previews: PreviewProvider {
	static var previews: some View {
		ContentView(AudioPlayer())
	}
}
