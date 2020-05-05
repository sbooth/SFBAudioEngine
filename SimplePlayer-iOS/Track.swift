//
// Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import SwiftUI

struct Track: Identifiable {
	var id = UUID()
	let url: URL
	let metadata: AudioMetadata

	init(_ url: URL) {
		self.url = url
		if let file = try? AudioFile(readingPropertiesAndMetadataFrom: url) {
			metadata = file.metadata
		}
		else {
			metadata = AudioMetadata()
		}
	}
}
