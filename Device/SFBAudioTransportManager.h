/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioPlugIn.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio transport manager
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(AudioTransportManager) @interface SFBAudioTransportManager : SFBAudioPlugIn

/// Returns an array of available audio transport managers or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyTransportManagerList on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioTransportManager *> *transportManagers;

/// Returns an array  of audio endpoints provided by the transport manager or \c nil on error
/// @note This corresponds to \c kAudioTransportManagerPropertyEndPointList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *endpoints;
/// Returns the transport type  or \c 0 on error
/// @note This corresponds to \c kAudioTransportManagerPropertyTransportType
@property (nonatomic, readonly) SFBAudioDeviceTransportType transportType;

@end

NS_ASSUME_NONNULL_END