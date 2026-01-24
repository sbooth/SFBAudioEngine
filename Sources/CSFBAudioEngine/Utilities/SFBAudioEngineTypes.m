//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioEngineTypes.h"

const NSTimeInterval      SFBUnknownTime = -1;
const SFBPlaybackPosition SFBInvalidPlaybackPosition = {.framePosition = SFBUnknownFramePosition,
                                                        .frameLength = SFBUnknownFrameLength};
const SFBPlaybackTime     SFBInvalidPlaybackTime = {.currentTime = SFBUnknownTime, .totalTime = SFBUnknownTime};
