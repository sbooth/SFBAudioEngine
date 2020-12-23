/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBAudioObject.h>

@class SFBAudioDevice;

NS_ASSUME_NONNULL_BEGIN

/// A data source for an audio device
NS_SWIFT_NAME(AudioDevice.DataSource) @interface SFBAudioDeviceDataSource : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioDeviceDataSource object for the specified audio object data source
/// @param audioDevice The owning audio device
/// @param scope The data source's scope
/// @param dataSourceID The data source ID
/// @return An initialized \c SFBAudioDevice object or \c nil on error
- (instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(SFBAudioObjectPropertyScope)scope dataSourceID:(UInt32)dataSourceID NS_DESIGNATED_INITIALIZER;

/// Returns the owning audio device
@property (nonatomic, readonly) SFBAudioDevice *audioDevice;
/// Returns the data source scope
@property (nonatomic, readonly) SFBAudioObjectPropertyScope scope;
/// Returns the data source ID
@property (nonatomic, readonly) UInt32 dataSourceID;

/// Returns the data source name or \c nil on error
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the data source kind or \c nil on error
@property (nonatomic, nullable, readonly) NSNumber *kind;

@end

NS_ASSUME_NONNULL_END
