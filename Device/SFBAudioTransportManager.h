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
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *endpoints NS_REFINED_FOR_SWIFT;
/// Returns the audio endpoint provided by the transport manager with the specified UID or \c nil if unknown
/// @note This corresponds to \c kAudioTransportManagerPropertyTranslateUIDToEndPoint
- (nullable SFBAudioObject *)endpointForUID:(NSString *)endpointUID NS_SWIFT_NAME(endpoint(_:));
/// Returns the transport type  or \c nil on error
/// @note This corresponds to \c kAudioTransportManagerPropertyTransportType
@property (nonatomic, nullable, readonly) NSNumber *transportType NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
