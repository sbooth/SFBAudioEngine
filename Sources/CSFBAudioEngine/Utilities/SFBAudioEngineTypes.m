//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioEngineTypes.h"

const NSTimeInterval SFBUnknownTime = -1;
const SFBPlaybackPosition SFBInvalidPlaybackPosition = {.framePosition = SFBUnknownFramePosition,
                                                        .frameLength = SFBUnknownFrameLength};
const SFBPlaybackTime SFBInvalidPlaybackTime = {.currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime};
