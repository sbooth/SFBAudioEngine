//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

struct ContentView: View {
	@EnvironmentObject var model: DataModel

	var body: some View {
		return NavigationView {
			List(model.tracks) { track in
				NavigationLink(destination: PlayerView(viewModel: PlayerViewModel(dataModel: model))
								.onAppear(perform: { try? model.player.play(track.url) })
								.onDisappear(perform: { model.player.stop() })) {
					TrackView(track: track)
				}
			}
			.navigationBarTitle("Tracks")
		}
	}
}
struct ContentView_Previews: PreviewProvider {
	static var previews: some View {
		ContentView()
	}
}
