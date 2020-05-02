/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

@class SFBAudioDevice;

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(AudioDeviceDataSource) @interface SFBAudioDeviceDataSource : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(AudioObjectPropertyScope)scope dataSourceID:(UInt32)dataSourceID NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) SFBAudioDevice *audioDevice;
@property (nonatomic, readonly) AudioObjectPropertyScope scope;
@property (nonatomic, readonly) UInt32 dataSourceID;

@property (nonatomic, nullable, readonly) NSString *name;
@property (nonatomic, readonly) UInt32 kind;

@end

NS_ASSUME_NONNULL_END
