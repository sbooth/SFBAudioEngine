//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

struct Track: Identifiable {
	let id = UUID()
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
