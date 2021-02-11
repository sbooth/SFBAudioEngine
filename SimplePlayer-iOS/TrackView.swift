//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

struct TrackView: View {
	@State var track: Track

	var body: some View {
		VStack(alignment: .leading, spacing: 2) {
			Text(track.metadata.title ?? track.url.lastPathComponent)
				.font(Font.system(.title).bold())

			Text(track.metadata.artist ?? "")
				.font(.system(.headline))
		}
	}
}

struct TrackView_Previews: PreviewProvider {
	static var previews: some View {
		TrackView(track: Track(URL(string: "fnord")!))
	}
}
