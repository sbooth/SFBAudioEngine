/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// Audio stream terminal types
typedef NS_ENUM(UInt32, SFBAudioStreamTerminalType) {
	/// Unknown
	SFBAudioStreamTerminalTypeUnknown 					= kAudioStreamTerminalTypeUnknown,
	/// Line level
	SFBAudioStreamTerminalTypeLine 						= kAudioStreamTerminalTypeLine,
	/// Digital audio interface
	SFBAudioStreamTerminalTypeDigitalAudioInterface 	= kAudioStreamTerminalTypeDigitalAudioInterface,
	/// Spekaer
	SFBAudioStreamTerminalTypeSpeaker 					= kAudioStreamTerminalTypeSpeaker,
	/// Headphones
	SFBAudioStreamTerminalTypeHeadphones 				= kAudioStreamTerminalTypeHeadphones,
	/// LFE speaker
	SFBAudioStreamTerminalTypeLFESpeaker 				= kAudioStreamTerminalTypeLFESpeaker,
	/// Telephone handset speaker
	SFBAudioStreamTerminalTypeReceiverSpeaker 			= kAudioStreamTerminalTypeReceiverSpeaker,
	/// Microphone
	SFBAudioStreamTerminalTypeMicrophone 				= kAudioStreamTerminalTypeMicrophone,
	/// Headset microphone
	SFBAudioStreamTerminalTypeHeadsetMicrophone 		= kAudioStreamTerminalTypeHeadsetMicrophone,
	/// Telephone handset microphone
	SFBAudioStreamTerminalTypeReceiverMicrophone 		= kAudioStreamTerminalTypeReceiverMicrophone,
	/// TTY
	SFBAudioStreamTerminalTypeTTY 						= kAudioStreamTerminalTypeTTY,
	/// HDMI
	SFBAudioStreamTerminalTypeHDMI 						= kAudioStreamTerminalTypeHDMI,
	/// DisplayPort
	SFBAudioStreamTerminalTypeDisplayPort 				= kAudioStreamTerminalTypeDisplayPort
} NS_SWIFT_NAME(AudioStream.TerminalType);

/// An audio stream
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal), a master element (\c kAudioObjectPropertyElementMaster), and an element for each channel in each stream
NS_SWIFT_NAME(AudioStream) @interface SFBAudioStream : SFBAudioObject

/// Returns \c YES if the stream is active
/// @note This corresponds to \c kAudioStreamPropertyIsActive
- (BOOL)isActiveOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c YES if this is an output stream
/// @note This corresponds to \c kAudioStreamPropertyDirection
- (BOOL)isOutputOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the terminal type  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyTerminalType
- (SFBAudioStreamTerminalType)terminalTypeOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the starting channel in the owning device  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyStartingChannel
- (UInt32)startingChannelOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the latency  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyLatency
- (UInt32)latencyOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the virtual format or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyVirtualFormat
/// @note The return value contains a wrapped \c AudioStreamBasicDescription structure
- (nullable NSValue *)virtualFormatOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the available virtual formats or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyAvailableVirtualFormats
/// @note The return value contains an array of wrapped \c AudioStreamRangedDescription structures
- (nullable NSArray<NSValue *> *)availableVirtualFormatsOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the physical format or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyPhysicalFormat
- (nullable NSValue *)physicalFormatOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the available physical formats or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyAvailablePhysicalFormats
/// @note The return value contains an array of wrapped \c AudioStreamRangedDescription structures
- (nullable NSArray<NSValue *> *)availablePhysicalFormatsOnElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
@end

NS_ASSUME_NONNULL_END
