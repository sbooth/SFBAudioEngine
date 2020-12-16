/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// Audio subdevice clock drift compensation quality
typedef NS_ENUM(UInt32, SFBSubdeviceDriftCompensationQuality) {
	/// Minimum quality
	SFBSubdeviceDriftCompensationQualityMin 	= kAudioSubDeviceDriftCompensationMinQuality,
	/// Low quality
	SFBSubdeviceDriftCompensationQualityLow 	= kAudioSubDeviceDriftCompensationLowQuality,
	/// Medium quality
	SFBSubdeviceDriftCompensationQualityMedium 	= kAudioSubDeviceDriftCompensationMediumQuality,
	/// High quality
	SFBSubdeviceDriftCompensationQualityHigh 	= kAudioSubDeviceDriftCompensationHighQuality,
	/// Maximum quality
	SFBSubdeviceDriftCompensationQualityMax 	= kAudioSubDeviceDriftCompensationMaxQuality
} NS_SWIFT_NAME(Subdevice.DriftCompensationQuality);

/// An audio subdevice
NS_SWIFT_NAME(Subdevice) @interface SFBSubdevice : SFBAudioDevice

/// Returns an array of available subdevices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBSubdevice *> *subdevices;

/// Returns the extra latency or \c 0 on error
@property (nonatomic, readonly) double extraLatency;
/// Returns the drift compensation or \c 0 on error
@property (nonatomic, readonly) BOOL driftCompensation;
/// Returns the drift compensation quality or \c 0 on error
@property (nonatomic, readonly) SFBSubdeviceDriftCompensationQuality driftCompensationQuality;

@end

NS_ASSUME_NONNULL_END
