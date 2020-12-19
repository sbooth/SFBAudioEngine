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

/// Returns the extra latency or \c nil on error
/// @note This corresponds to \c kAudioSubDevicePropertyExtraLatency
- (nullable NSNumber *)extraLatencyInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the drift compensation or \c nil on error
/// @note This corresponds to \c kAudioSubDevicePropertyDriftCompensation
- (nullable NSNumber *)driftCompensationInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Sets drift compensation
/// @note This corresponds to \c kAudioSubDevicePropertyDriftCompensation
/// @param value The desired value
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setDriftCompensation:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Returns the drift compensation quality or \c nil on error
/// @note This corresponds to \c kAudioSubDevicePropertyDriftCompensationQuality
- (nullable NSNumber *)driftCompensationQualityInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Sets the drift compensation quality
/// @note This corresponds to \c kAudioSubDevicePropertyDriftCompensationQuality
/// @param value The desired value
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setDriftCompensationQuality:(unsigned int)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
