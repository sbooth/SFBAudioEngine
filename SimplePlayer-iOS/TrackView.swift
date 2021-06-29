//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

struct TrackView: View {
	let track: Track

	var body: some View {
		VStack(alignment: .leading, spacing: 2) {
			Text(track.metadata.title ?? track.url.lastPathComponent)
				.font(.headline)
				.fontWeight(.bold)
			Text(track.metadata.artist ?? "")
				.font(.subheadline)
		}
	}
}

struct TrackView_Previews: PreviewProvider {
	static var previews: some View {
		TrackView(track: Track(url: URL(string: "Santeria.mp3")!))
	}
}
