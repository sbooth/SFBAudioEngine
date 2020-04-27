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
@property (nonatomic) float masterVolume;

- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
- (void)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel;

@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *preferredStereoChannels NS_REFINED_FOR_SWIFT;

@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *dataSources;
@property (nonatomic) NSArray<SFBAudioDeviceDataSource *> *activeDataSources;

#pragma mark - Device Property Observation

- (void)whenMuteChangesPerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenMuteChanges(perform:));
- (void)whenMasterVolumeChangesPerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenMasterVolumeChanges(perform:));

- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel performBlock:(void (^)(void))block;
- (void)whenDataSourcesChangePerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenDataSourcesChange(perform:));
- (void)whenActiveDataSourcesChangePerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenActiveDataSourcesChange(perform:));

@end

NS_ASSUME_NONNULL_END
