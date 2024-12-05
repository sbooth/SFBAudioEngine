//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioMetadata {
	/// The compilation flag
	public var isCompilation: Bool? {
		get {
			__compilation?.boolValue
		}
		set {
			__compilation = newValue as NSNumber?
		}
	}

	/// The track number
	public var trackNumber: Int? {
		get {
			__trackNumber?.intValue
		}
		set {
			__trackNumber = newValue as NSNumber?
		}
	}

	/// The track total
	public var trackTotal: Int? {
		get {
			__trackTotal?.intValue
		}
		set {
			__trackTotal = newValue as NSNumber?
		}
	}

	/// The disc number
	public var discNumber: Int? {
		get {
			__discNumber?.intValue
		}
		set {
			__discNumber = newValue as NSNumber?
		}
	}

	/// The disc total
	public var discTotal: Int? {
		get {
			__discTotal?.intValue
		}
		set {
			__discTotal = newValue as NSNumber?
		}
	}

	/// The beats per minute (BPM)
	public var bpm: Int? {
		get {
			__bpm?.intValue
		}
		set {
			__bpm = newValue as NSNumber?
		}
	}

	/// The rating
	public var rating: Int? {
		get {
			__rating?.intValue
		}
		set {
			__rating = newValue as NSNumber?
		}
	}

	/// The replay gain reference loudness
	public var replayGainReferenceLoudness: Double? {
		get {
			__replayGainReferenceLoudness?.doubleValue
		}
		set {
			__replayGainReferenceLoudness = newValue as NSNumber?
		}
	}

	/// The replay gain track gain
	public var replayGainTrackGain: Double? {
		get {
			__replayGainTrackGain?.doubleValue
		}
		set {
			__replayGainTrackGain = newValue as NSNumber?
		}
	}

	/// The replay gain track peak
	public var replayGainTrackPeak: Double? {
		get {
			__replayGainTrackPeak?.doubleValue
		}
		set {
			__replayGainTrackPeak = newValue as NSNumber?
		}
	}

	/// The replay gain album gain
	public var replayGainAlbumGain: Double? {
		get {
			__replayGainAlbumGain?.doubleValue
		}
		set {
			__replayGainAlbumGain = newValue as NSNumber?
		}
	}

	/// The replay gain album peak
	public var replayGainAlbumPeak: Double? {
		get {
			__replayGainAlbumPeak?.doubleValue
		}
		set {
			__replayGainAlbumPeak = newValue as NSNumber?
		}
	}
}
