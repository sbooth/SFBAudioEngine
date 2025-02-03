//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension PlaybackPosition {
	/// Returns `true` if the current frame position and total number of frames are valid
	public var isValid: Bool {
		framePosition != unknownFramePosition && frameLength != unknownFrameLength
	}
	/// Returns `true` if the current frame position is valid
	public var isFramePositionValid: Bool {
		framePosition != unknownFramePosition
	}
	/// Returns `true` if the total number of frames is valid
	public var isFrameLengthValid: Bool {
		frameLength != unknownFrameLength
	}

	/// The current frame position or `nil` if unknown
	public var current: AVAudioFramePosition? {
		isFramePositionValid ? framePosition : nil
	}
	/// The total number of frames or `nil` if unknown
	public var total: AVAudioFramePosition? {
		isFrameLengthValid ? frameLength : nil
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard isValid else {
			return nil
		}
		return Double(framePosition) / Double(frameLength)
	}

	/// Returns the frames remaining
	public var remaining: AVAudioFramePosition? {
		guard isValid else {
			return nil
		}
		return frameLength - framePosition
	}
}
