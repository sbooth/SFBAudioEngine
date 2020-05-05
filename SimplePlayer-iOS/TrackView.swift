//
// Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import SwiftUI

struct TrackView: View {
	@State var track: Track

	var body: some View {
		HStack(spacing: 8) {
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
