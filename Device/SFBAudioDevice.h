/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

#import "SFBAudioDeviceDataSource.h"

@class SFBAudioOutputDevice;

NS_ASSUME_NONNULL_BEGIN

/// Posted when the available audio devices change
extern const NSNotificationName SFBAudioDevicesChangedNotification;

NS_SWIFT_NAME(AudioDevice) @interface SFBAudioDevice : NSObject

@property (class, nonatomic, readonly) NSArray<SFBAudioDevice *> *allDevices;
@property (class, nonatomic, readonly) NSArray<SFBAudioOutputDevice *> *outputDevices;

@property (class, nonatomic, readonly) SFBAudioOutputDevice *defaultOutputDevice;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithDeviceUID:(NSString *)deviceUID;
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) AudioObjectID deviceID;
@property (nonatomic, readonly) NSString *deviceUID;
@property (nonatomic, nullable, readonly) NSString *name;
@property (nonatomic, nullable, readonly) NSString *manufacturer;

@property (nonatomic, readonly) BOOL supportsInput;
@property (nonatomic, readonly) BOOL supportsOutput;

#pragma mark - Device Properties

@property (nonatomic) double sampleRate;
@property (nonatomic, readonly) NSArray<NSNumber *> *availableSampleRates NS_REFINED_FOR_SWIFT;

- (NSArray<SFBAudioDeviceDataSource *> *)dataSourcesInScope:(AudioObjectPropertyScope)scope;

- (NSArray<SFBAudioDeviceDataSource *> *)activeDataSourcesInScope:(AudioObjectPropertyScope)scope;
- (void)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources inScope:(AudioObjectPropertyScope)scope;

#pragma mark - Device Property Observation

- (void)whenSampleRateChangesPerformBlock:(void (^)(void))block NS_SWIFT_NAME(whenSampleRateChanges(perform:));
- (void)whenDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(void (^)(void))block;
- (void)whenActiveDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(void (^)(void))block;

- (BOOL)hasProperty:(AudioObjectPropertySelector)property;
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(void (^)(void))block;
- (void)whenProperty:(AudioObjectPropertySelector)property changesInScope:(AudioObjectPropertyScope)scope performBlock:(void (^)(void))block;
- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(void (^)(void))block;

@end

NS_ASSUME_NONNULL_END
