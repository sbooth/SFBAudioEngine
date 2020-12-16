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
@property (nonatomic, readonly) BOOL isActive;
/// Returns \c YES if this is an output stream
/// @note This corresponds to \c kAudioStreamPropertyDirection
@property (nonatomic, readonly) BOOL isOutput;
/// Returns the terminal type  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyTerminalType
@property (nonatomic, readonly) SFBAudioStreamTerminalType terminalType;
/// Returns the starting channel in the owning device  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyStartingChannel
@property (nonatomic, readonly) UInt32 startingChannel;
/// Returns the latency  or \c 0 on error
/// @note This corresponds to \c kAudioStreamPropertyLatency
@property (nonatomic, readonly) UInt32 latency;

/// Retrieves the virtual format  and returns \c YES on success
/// @note This corresponds to \c kAudioStreamPropertyVirtualFormat
/// @param format A pointer to an \c AudioStreamBasicDescription to receive the format
/// @return \c YES on success
- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)format NS_SWIFT_UNAVAILABLE("Use -getVirtualFormat:onElement: instead");
/// Retrieves the virtual format  and returns \c YES on success
/// @note This corresponds to \c kAudioStreamPropertyVirtualFormat
/// @param format A pointer to an \c AudioStreamBasicDescription to receive the format
/// @param element The desired element
/// @return \c YES on success
- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)format onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Retrieves the physical format  and returns \c YES on success
/// @note This corresponds to \c kAudioStreamPropertyPhysicalFormat
/// @param format A pointer to an \c AudioStreamBasicDescription to receive the format
/// @return \c YES on success
- (BOOL)getPhysicalFormat:(AudioStreamBasicDescription *)format NS_SWIFT_UNAVAILABLE("Use -getPhysicalFormat:onElement: instead");;
/// Retrieves the physical format  and returns \c YES on success
/// @note This corresponds to \c kAudioStreamPropertyPhysicalFormat
/// @param format A pointer to an \c AudioStreamBasicDescription to receive the format
/// @param element The desired element
/// @return \c YES on success
- (BOOL)getPhysicalFormat:(AudioStreamBasicDescription *)format onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
