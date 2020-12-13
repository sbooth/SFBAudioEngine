/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio subdevice
NS_SWIFT_NAME(Subdevice) @interface SFBSubdevice : SFBAudioDevice

/// Returns the extra latency or \c 0 on error
@property (nonatomic, readonly) Float64 extraLatency;
/// Returns the drift compensation or \c 0 on error
@property (nonatomic, readonly) UInt32 driftCompensation;
/// Returns the drift compensation quality or \c 0 on error
@property (nonatomic, readonly) UInt32 driftCompensationQuality;

@end

NS_ASSUME_NONNULL_END
