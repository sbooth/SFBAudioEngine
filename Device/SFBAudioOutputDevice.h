/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDevice.h"

@class SFBAudioDeviceDataSource;

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(AudioOutputDevice) @interface SFBAudioOutputDevice: SFBAudioDevice

@property (nonatomic, getter=isMuted) BOOL mute;
@property (nonatomic) float masterVolume;

- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
- (void)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel;

@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *dataSources;
@property (nonatomic) NSArray<SFBAudioDeviceDataSource *> *activeDataSources;

- (void)whenDataSourcesChangePerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenDataSourcesChange(perform:));;

@end

NS_ASSUME_NONNULL_END
