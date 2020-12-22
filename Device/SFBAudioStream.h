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

/// Returns \c @ YES if the stream is active or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyIsActive
@property (nonatomic, nullable, readonly) NSNumber *isActive NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if this is an output stream or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyDirection
@property (nonatomic, nullable, readonly) NSNumber *isOutput NS_REFINED_FOR_SWIFT;
/// Returns the terminal type  or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyTerminalType
@property (nonatomic, nullable, readonly) NSNumber *terminalType;
/// Returns the starting channel in the owning device  or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyStartingChannel
@property (nonatomic, nullable, readonly) NSNumber *startingChannel NS_REFINED_FOR_SWIFT;
/// Returns the latency  or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyLatency
@property (nonatomic, nullable, readonly) NSNumber *latency;
/// Returns the virtual format or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyVirtualFormat
/// @note The return value contains a wrapped \c AudioStreamBasicDescription structure
@property (nonatomic, nullable, readonly) NSValue *virtualFormat NS_REFINED_FOR_SWIFT;
/// Sets the virtual format
/// @note This corresponds to \c kAudioStreamPropertyVirtualFormat
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setVirtualFormat:(AudioStreamBasicDescription)value error:(NSError **)error NS_SWIFT_NAME(setVirtualFormat(_:));
/// Returns the available virtual formats or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyAvailableVirtualFormats
/// @note The return value contains an array of wrapped \c AudioStreamRangedDescription structures
@property (nonatomic, nullable, readonly) NSArray<NSValue *> *availableVirtualFormats NS_REFINED_FOR_SWIFT;
/// Returns the physical format or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyPhysicalFormat
@property (nonatomic, nullable, readonly) NSValue *physicalFormat NS_REFINED_FOR_SWIFT;
/// Sets the physical format
/// @note This corresponds to \c kAudioStreamPropertyPhysicalFormat
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setPhysicalFormat:(AudioStreamBasicDescription)value error:(NSError **)error NS_SWIFT_NAME(setPhysicalFormat(_:));
/// Returns the available physical formats or \c nil on error
/// @note This corresponds to \c kAudioStreamPropertyAvailablePhysicalFormats
/// @note The return value contains an array of wrapped \c AudioStreamRangedDescription structures
@property (nonatomic, nullable, readonly) NSArray<NSValue *> *availablePhysicalFormats NS_REFINED_FOR_SWIFT;
@end

NS_ASSUME_NONNULL_END
