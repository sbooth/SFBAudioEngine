/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// An aggregate audio device
NS_SWIFT_NAME(AggregateAudioDevice) @interface SFBAggregateAudioDevice : SFBAudioDevice

/// Returns \c YES if the aggregate device is private
@property (nonatomic, readonly) BOOL isPrivate;

@end

NS_ASSUME_NONNULL_END
