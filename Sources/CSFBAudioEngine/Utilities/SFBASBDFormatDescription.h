//
// SPDX-FileCopyrightText: 2014 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <CoreAudioTypes/CoreAudioTypes.h>
#import <Foundation/Foundation.h>

CF_EXTERN_C_BEGIN

/// Returns a string representation of the stream format described by an AudioStreamBasicDescription structure.
NSString *_Nullable SFBASBDFormatDescription(const AudioStreamBasicDescription *_Nonnull streamDescription);

CF_EXTERN_C_END
