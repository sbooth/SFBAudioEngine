/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio box
NS_SWIFT_NAME(AudioBox) @interface SFBAudioBox : SFBAudioObject

/// Returns an array of available audio boxes or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioBox *> *boxes;

/// Returns an initialized \c SFBAudioBox object with the specified box UID
/// @param boxUID The desired box UID
/// @return An initialized \c SFBAudioBox object or \c nil if \c boxUID is invalid or unknown
- (nullable instancetype)initWithBoxUID:(NSString *)boxUID;

/// Returns the box ID
/// @note This is equivalent to \c objectID
@property (nonatomic, readonly) AudioObjectID boxID;
/// Returns the box UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *boxUID;

@end

NS_ASSUME_NONNULL_END
