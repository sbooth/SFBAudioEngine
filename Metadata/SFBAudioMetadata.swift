/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioMetadata {
	/// The compilation flag
	var compilation: Bool? {
		get {
			__compilation?.boolValue
		}
		set {
			__compilation = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The track number
	var trackNumber: Int? {
		get {
			__trackNumber?.intValue
		}
		set {
			__trackNumber = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The track total
	var trackTotal: Int? {
		get {
			__trackTotal?.intValue
		}
		set {
			__trackTotal = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The disc number
	var discNumber: Int? {
		get {
			__discNumber?.intValue
		}
		set {
			__discNumber = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The disc total
	var discTotal: Int? {
		get {
			__discTotal?.intValue
		}
		set {
			__discTotal = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The Beats per minute (BPM)
	var bpm: Int? {
		get {
			__bpm?.intValue
		}
		set {
			__bpm = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The rating
	var rating: Int? {
		get {
			__rating?.intValue
		}
		set {
			__rating = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain reference loudness
	var replayGainReferenceLoudness: Double? {
		get {
			__replayGainReferenceLoudness?.doubleValue
		}
		set {
			__replayGainReferenceLoudness = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain track gain
	var replayGainTrackGain: Double? {
		get {
			__replayGainTrackGain?.doubleValue
		}
		set {
			__replayGainTrackGain = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain track peak
	var replayGainTrackPeak: Double? {
		get {
			__replayGainTrackPeak?.doubleValue
		}
		set {
			__replayGainTrackPeak = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain album gain
	var replayGainAlbumGain: Double? {
		get {
			__replayGainAlbumGain?.doubleValue
		}
		set {
			__replayGainAlbumGain = newValue != nil ? newValue! as NSNumber : nil
		}
	}

	/// The replay gain album peak
	var replayGainAlbumPeak: Double? {
		get {
			__replayGainAlbumPeak?.doubleValue
		}
		set {
			__replayGainAlbumPeak = newValue != nil ? newValue! as NSNumber : nil
		}
	}
}
