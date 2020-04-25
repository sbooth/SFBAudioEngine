/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

#import "SFBAudioDeviceDataSource.h"

@class SFBAudioOutputDevice;

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(AudioDevice) @interface SFBAudioDevice: NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property (class, nonatomic, readonly) NSArray<SFBAudioDevice *> *allDevices;
@property (class, nonatomic, readonly) NSArray<SFBAudioOutputDevice *> *outputDevices;

@property (class, nonatomic, readonly) SFBAudioOutputDevice *defaultOutputDevice;

- (nullable instancetype)initWithDeviceUID:(NSString *)deviceUID;
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)deviceID NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) AudioObjectID deviceID;
@property (nonatomic, readonly) NSString *deviceUID;
@property (nonatomic, nullable, readonly) NSString *name;
@property (nonatomic, nullable, readonly) NSString *manufacturer;

@property (nonatomic, readonly) BOOL supportsInput;
@property (nonatomic, readonly) BOOL supportsOutput;

- (NSArray<SFBAudioDeviceDataSource *> *)dataSourcesForScope:(AudioObjectPropertyScope)scope;

@end

NS_ASSUME_NONNULL_END
