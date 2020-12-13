/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioPlugIn.h>

NS_ASSUME_NONNULL_BEGIN

@class SFBEndpointDevice;

/// An audio transport manager
NS_SWIFT_NAME(AudioTransportManager) @interface SFBAudioTransportManager : SFBAudioPlugIn

/// Returns an array of available audio transport managers or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioTransportManager *> *transportManagers;

/// Returns an array  of audio end points provided by the transport manager or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBEndpointDevice *> *endPoints;
/// Returns the transport type  or \c 0 on error
@property (nonatomic, readonly) UInt32 transportType;

@end

NS_ASSUME_NONNULL_END
