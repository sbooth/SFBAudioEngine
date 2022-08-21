//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioMetadata {
	/// The track number
	public var trackNumber: Int? {
		get {
			__trackNumber?.intValue
		}
		set {
			__trackNumber = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The track total
	public var trackTotal: Int? {
		get {
			__trackTotal?.intValue
		}
		set {
			__trackTotal = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The disc number
	public var discNumber: Int? {
		get {
			__discNumber?.intValue
		}
		set {
			__discNumber = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The disc total
	public var discTotal: Int? {
		get {
			__discTotal?.intValue
		}
		set {
			__discTotal = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The compilation flag
	public var isCompilation: Bool? {
		get {
			__compilation?.boolValue
		}
		set {
			__compilation = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The rating
	public var rating: Int? {
		get {
			__rating?.intValue
		}
		set {
			__rating = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The beats per minute (BPM)
	public var bpm: Int? {
		get {
			__bpm?.intValue
		}
		set {
			__bpm = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain reference loudness
	public var replayGainReferenceLoudness: Double? {
		get {
			__replayGainReferenceLoudness?.doubleValue
		}
		set {
			__replayGainReferenceLoudness = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain track gain
	public var replayGainTrackGain: Double? {
		get {
			__replayGainTrackGain?.doubleValue
		}
		set {
			__replayGainTrackGain = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain track peak
	public var replayGainTrackPeak: Double? {
		get {
			__replayGainTrackPeak?.doubleValue
		}
		set {
			__replayGainTrackPeak = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain album gain
	public var replayGainAlbumGain: Double? {
		get {
			__replayGainAlbumGain?.doubleValue
		}
		set {
			__replayGainAlbumGain = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain album peak
	public var replayGainAlbumPeak: Double? {
		get {
			__replayGainAlbumPeak?.doubleValue
		}
		set {
			__replayGainAlbumPeak = newValue != nil ? newValue! as NSNumber : nil
		}
	}
}
