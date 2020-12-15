/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

@class SFBAudioDevice, SFBClockDevice;

NS_ASSUME_NONNULL_BEGIN

/// An audio box
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(AudioBox) @interface SFBAudioBox : SFBAudioObject

/// Returns an array of available audio boxes or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyBoxList on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioBox *> *boxes;

/// Returns an initialized \c SFBAudioBox object with the specified box UID
/// @param boxUID The desired box UID
/// @return An initialized \c SFBAudioBox object or \c nil if \c boxUID is invalid or unknown
- (nullable instancetype)initWithBoxUID:(NSString *)boxUID;

/// Returns the box UID or \c nil on error
/// @note This corresponds to \c kAudioBoxPropertyBoxUID
@property (nonatomic, nullable, readonly) NSString *boxUID;
/// Returns the transport type  or \c 0 on error
/// @note This corresponds to \c kAudioBoxPropertyTransportType
@property (nonatomic, readonly) SFBAudioDeviceTransportType transportType;
/// Returns \c YES if the box has audio
/// @note This corresponds to \c kAudioBoxPropertyHasAudio
@property (nonatomic, readonly) BOOL hasAudio;
/// Returns \c YES if the box has video
/// @note This corresponds to \c kAudioBoxPropertyHasVideo
@property (nonatomic, readonly) BOOL hasVideo;
/// Returns \c YES if the box has MIDI
/// @note This corresponds to \c kAudioBoxPropertyHasMIDI
@property (nonatomic, readonly) BOOL hasMIDI;
/// Returns \c YES if the box is acquired
/// @note This corresponds to \c kAudioBoxPropertyAcquired
@property (nonatomic, readonly) BOOL acquired;
/// Returns an array  of audio devices provided by the box or \c nil on error
/// @note This corresponds to \c kAudioBoxPropertyDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices;
/// Returns an array  of audio clock devices provided by the box or \c nil on error
/// @note This corresponds to \c kAudioBoxPropertyClockDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices;

@end

NS_ASSUME_NONNULL_END
