//
// Copyright (c) 2011 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import SFBAudioEngine
import UIKit

/// An audio track
struct Track: Identifiable {
	/// The unique identifier of this track
	let id = UUID()
	/// The URL holding the audio data
	let url: URL

	/// Audio properties for the track
	let properties: AudioProperties
	/// Audio metadata and attached pictures for the track
	let metadata: AudioMetadata

	/// Reads audio properties and metadata and initializes a playlist item
	init(url: URL) {
		self.url = url
		if let audioFile = try? AudioFile(readingPropertiesAndMetadataFrom: url) {
			self.properties = audioFile.properties
			self.metadata = audioFile.metadata
		} else {
			self.properties = AudioProperties()
			self.metadata = AudioMetadata()
		}
	}

	/// Returns a decoder for this track or `nil` if the audio type is unknown
	func decoder(enableDoP: Bool = false) throws -> PCMDecoding? {
		let pathExtension = url.pathExtension.lowercased()
		if AudioDecoder.handlesPaths(withExtension: pathExtension) {
			return try AudioDecoder(url: url)
		} else if DSDDecoder.handlesPaths(withExtension: pathExtension) {
			let dsdDecoder = try DSDDecoder(url: url)
			return enableDoP ? try DoPDecoder(decoder: dsdDecoder) : try DSDPCMDecoder(decoder: dsdDecoder)
		}
		return nil
	}
}

extension Track: Equatable {
	/// Returns true if the two tracks have the same `id`
	static func ==(lhs: Track, rhs: Track) -> Bool {
		return lhs.id == rhs.id
	}
}

extension AudioMetadata {
	/// Returns a random attached picture or `nil` if no attached pictures are available
	var randomImage: UIImage? {
		guard let imageData = attachedPictures.randomElement()?.imageData else {
			return nil
		}
		return UIImage(data: imageData)
	}
}
