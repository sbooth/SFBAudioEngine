/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>
#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio stream
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal), a master element (\c kAudioObjectPropertyElementMaster), and an element for each channel in each stream
NS_SWIFT_NAME(AudioStream) @interface SFBAudioStream : SFBAudioObject

/// Returns \c YES if the stream is active
@property (nonatomic, readonly) BOOL isActive;
/// Returns \c YES if this is an output stream
@property (nonatomic, readonly) BOOL isOutput;
/// Returns the terminal type  or \c 0 on error
@property (nonatomic, readonly) UInt32 terminalType;
/// Returns the starting channel in the owning device  or \c 0 on error
@property (nonatomic, readonly) UInt32 startingChannel;
/// Returns the latency  or \c 0 on error
@property (nonatomic, readonly) UInt32 latency;
/// Returns the virtual format  or \c nil on error
@property (nonatomic, nullable, readonly) AVAudioFormat * virtualFormat;
/// Returns the physical format  or \c nil on error
@property (nonatomic, nullable, readonly) AVAudioFormat * physicalFormat;

@end

NS_ASSUME_NONNULL_END
