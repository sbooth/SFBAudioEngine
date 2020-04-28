/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDevice.h"

@class SFBAudioDeviceDataSource;

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(AudioOutputDevice) @interface SFBAudioOutputDevice: SFBAudioDevice

#pragma mark - Device Properties

@property (nonatomic, getter=isMuted) BOOL mute;

@property (nonatomic, readonly) BOOL hasMasterVolume;
@property (nonatomic, readonly) float masterVolume;
- (BOOL)setMasterVolume:(float)masterVolume error:(NSError **)error;

- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error;

@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *preferredStereoChannels NS_REFINED_FOR_SWIFT;

@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *dataSources;
@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *activeDataSources;
- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources error:(NSError **)error;

#pragma mark - Device Property Observation

- (void)whenMuteChangesPerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenMuteChanges(perform:));
- (void)whenMasterVolumeChangesPerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenMasterVolumeChanges(perform:));

- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel performBlock:(void (^)(void))block;
- (void)whenDataSourcesChangePerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenDataSourcesChange(perform:));
- (void)whenActiveDataSourcesChangePerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenActiveDataSourcesChange(perform:));

@end

NS_ASSUME_NONNULL_END
