/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioObject.PropertySelector: ExpressibleByStringLiteral {
	public init(stringLiteral value: StringLiteralType) {
		var fourcc: UInt32 = 0
		for uc in value.prefix(4).unicodeScalars {
			fourcc = (fourcc << 8) + (uc.value & 0xff)
		}
		self.init(rawValue: fourcc)!
	}
}

extension AudioObject.PropertySelector: CustomDebugStringConvertible {
	public var debugDescription: String {
		let bytes: [CChar] = [
			0x27, // '
			CChar((rawValue >> 24) & 0xff),
			CChar((rawValue >> 16) & 0xff),
			CChar((rawValue >> 8) & 0xff),
			CChar(rawValue & 0xff),
			0x27, // '
			0
		]
		return String(cString: bytes)
	}
}

extension AudioObject.PropertyScope: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .global: 			return ".global"
		case .input: 			return ".input"
		case .output: 			return ".output"
		case .playThrough: 		return ".playThrough"
		case .wildcard: 		return ".wildcard"
		@unknown default: 		return "UNKNOWN"
		}
	}
}

extension AudioObject.PropertyElement {
	/// The master element, `kAudioObjectPropertyElementMaster`
	public static var master: AudioObject.PropertyElement {
		return kAudioObjectPropertyElementMaster
	}

	/// The wildcard element, `kAudioObjectPropertyElementWildcard`
	public static var wildcard: AudioObject.PropertyElement {
		return kAudioObjectPropertyElementWildcard
	}
}

extension AudioObject.PropertyElement: CustomDebugStringConvertible {
	public var debugDescription: String {
		if self == kAudioObjectPropertyElementMaster {
			return ".master"
		}
		else if self == kAudioObjectPropertyElementWildcard {
			return ".wildcard"
		}
		else {
			return "\(self)"
		}
	}
}

extension AudioDevice.TransportType: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .unknown:			return ".unknown"
		case .builtIn:			return ".builtIn"
		case .aggregate:		return ".aggregate"
		case .virtual:			return ".virtual"
		case .PCI:				return ".PCI"
		case .USB:				return ".USB"
		case .fireWire:			return ".fireWire"
		case .bluetooth:		return ".bluetooth"
		case .bluetoothLE:		return ".bluetoothLE"
		case .HDMI:				return ".HDMI"
		case .displayPort:		return ".displayPort"
		case .airPlay:			return ".airPlay"
		case .AVB:				return ".AVB"
		case .thunderbolt:		return ".thunderbolt"
		@unknown default: 		return "UNKNOWN"
		}
	}
}
