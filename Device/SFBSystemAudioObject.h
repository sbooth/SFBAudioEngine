/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// The audio system object
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(SystemAudioObject) @interface SFBSystemAudioObject : SFBAudioObject

/// The singleton instance
@property (class, nonatomic, readonly) SFBSystemAudioObject *sharedInstance;

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID NS_UNAVAILABLE;

/// Returns \c @ YES if audio devices should mix stereo to mono  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyMixStereoToMono
@property (nonatomic, nullable, readonly) NSNumber *mixStereoToMono NS_REFINED_FOR_SWIFT;
/// Sets whether devices should mix stereo to mono
/// @note This corresponds to \c kAudioHardwarePropertyMixStereoToMono
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setMixStereoToMono:(BOOL)value error:(NSError **)error;

/// Returns \c @ YES if the current process contains the master HAL instance  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyProcessIsMaster
@property (nonatomic, nullable, readonly) NSNumber *processIsMaster NS_REFINED_FOR_SWIFT;

/// Returns \c @ YES if the HAL is initing or exiting the process  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyIsInitingOrExiting
@property (nonatomic, nullable, readonly) NSNumber *isInitingOrExiting NS_REFINED_FOR_SWIFT;

/// Informs the HAL the effective user id of the process has changed
/// @note This corresponds to \c kAudioHardwarePropertyUserIDChanged
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setUserIDChangedReturningError:(NSError **)error NS_SWIFT_NAME(setUserIDChanged());

/// Returns \c @ YES if the process will be heard  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyProcessIsAudible
@property (nonatomic, nullable, readonly) NSNumber *processIsAudible NS_REFINED_FOR_SWIFT;
/// Sets whether the process is audible
/// @note This corresponds to \c kAudioHardwarePropertyProcessIsAudible
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setProcessIsAudible:(BOOL)value error:(NSError **)error;

/// Returns \c @ YES if the process will allow the CPU to sleep while audio IO is in progress or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertySleepingIsAllowed
@property (nonatomic, nullable, readonly) NSNumber *sleepingIsAllowed NS_REFINED_FOR_SWIFT;
/// Sets whether the process will allow the CPU to sleep while audio IO is in progress
/// @note This corresponds to \c kAudioHardwarePropertySleepingIsAllowed
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setSleepingIsAllowed:(BOOL)value error:(NSError **)error;

/// Returns \c @ YES if the process should be unloaded after a period of inactivity  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyUnloadingIsAllowed
@property (nonatomic, nullable, readonly) NSNumber *unloadingIsAllowed NS_REFINED_FOR_SWIFT;
/// Sets whether the process should be unloaded after a period of inactivity
/// @note This corresponds to \c kAudioHardwarePropertyUnloadingIsAllowed
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setUnloadingIsAllowed:(BOOL)value error:(NSError **)error;

/// Returns \c @ YES if the HAL should automatically take hog mode on behalf of the process  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyHogModeIsAllowed
@property (nonatomic, nullable, readonly) NSNumber *hogModeIsAllowed NS_REFINED_FOR_SWIFT;
/// Sets whether the HAL should automatically take hog mode on behalf of the process
/// @note This corresponds to \c kAudioHardwarePropertyHogModeIsAllowed
/// @param value The desired value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setHogModeIsAllowed:(BOOL)value error:(NSError **)error;

/// Returns \c @ YES if the login session of the user is a console or headless session  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyUserSessionIsActiveOrHeadless
@property (nonatomic, nullable, readonly) NSNumber *userSessionIsActiveOrHeadless NS_REFINED_FOR_SWIFT;

/// Returns the power hint  or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyPowerHint
@property (nonatomic, nullable, readonly) NSNumber *powerHint NS_REFINED_FOR_SWIFT;
/// Sets the power hint
/// @note This corresponds to \c kAudioHardwarePropertyPowerHint
/// @param value The desired power hint
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully
- (BOOL)setPowerHint:(AudioHardwarePowerHint)value error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
