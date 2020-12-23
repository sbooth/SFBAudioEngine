/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBAudioObject.h>

@class SFBAudioDevice;

NS_ASSUME_NONNULL_BEGIN

/// A clock source for an audio device
NS_SWIFT_NAME(AudioDevice.ClockSource) @interface SFBAudioDeviceClockSource : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioDeviceClockSource object for the specified audio object clock source
/// @param audioDevice The owning audio device
/// @param scope The clock source's scope
/// @param clockSourceID The clock source ID
/// @return An initialized \c SFBAudioDevice object or \c nil on error
- (instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(SFBAudioObjectPropertyScope)scope clockSourceID:(UInt32)clockSourceID NS_DESIGNATED_INITIALIZER;

/// Returns the owning audio device
@property (nonatomic, readonly) SFBAudioDevice *audioDevice;
/// Returns the clock source scope
@property (nonatomic, readonly) SFBAudioObjectPropertyScope scope;
/// Returns the clock source ID
@property (nonatomic, readonly) UInt32 clockSourceID;

/// Returns the clock source name or \c nil on error
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the clock source kind or \c nil on error
@property (nonatomic, nullable, readonly) NSNumber *kind;

@end

NS_ASSUME_NONNULL_END
