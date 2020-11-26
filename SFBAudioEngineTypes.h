/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#pragma mark Audio Format Identifiers

/// Additional audio format IDs
typedef NS_ENUM(UInt32, SFBAudioFormatID) {
	/// Direct Stream Digital (DSD)
	SFBAudioFormatIDDirectStreamDigital 	= 'DSD ',
	/// DSD over PCM (DoP)
	SFBAudioFormatIDDoP 					= 'DoP ',
	/// Module
	SFBAudioFormatIDModule 					= 'MOD ',
	/// Monkey's Audio (APE)
	SFBAudioFormatIDMonkeysAudio 			= 'APE ',
	/// Musepack
	SFBAudioFormatIDMusepack 				= 'MPC ',
	/// Shorten
	SFBAudioFormatIDShorten					= 'SHN ',
	/// Ogg Speex
	SFBAudioFormatIDSpeex 					= 'SPX ',
	/// True Audio
	SFBAudioFormatIDTrueAudio 				= 'TTA ',
	/// Ogg Vorbis
	SFBAudioFormatIDVorbis 					= 'VORB',
	/// WavPack
	SFBAudioFormatIDWavPack 				= 'WV  '
} NS_SWIFT_NAME(AudioFormatID);
