/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

@class SFBAudioDevice, SFBClockDevice;

/// An audio box
NS_SWIFT_NAME(AudioBox) @interface SFBAudioBox : SFBAudioObject

/// Returns an array of available audio boxes or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioBox *> *boxes;

/// Returns an initialized \c SFBAudioBox object with the specified box UID
/// @param boxUID The desired box UID
/// @return An initialized \c SFBAudioBox object or \c nil if \c boxUID is invalid or unknown
- (nullable instancetype)initWithBoxUID:(NSString *)boxUID;

/// Returns the box UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *boxUID;
/// Returns the transport type of the box or \c 0 on error
@property (nonatomic, readonly) UInt32 transportType;
/// Returns \c YES if the box has audio
@property (nonatomic, readonly) BOOL hasAudio;
/// Returns \c YES if the box has video
@property (nonatomic, readonly) BOOL hasVideo;
/// Returns \c YES if the box has MIDI
@property (nonatomic, readonly) BOOL hasMIDI;
/// Returns \c YES if the box is acquired
@property (nonatomic, readonly) BOOL acquired;
/// Returns an array  of audio devices provided by the box or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices;
/// Returns an array  of audio clock devices provided by the box or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clocks;

@end

NS_ASSUME_NONNULL_END
