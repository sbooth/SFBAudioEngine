//
// Copyright (c) 2024-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <CoreAudioTypes/CoreAudioTypes.h>

/// Fills an `AudioStreamBasicDescription` with details from a sndfile format
/// - note: Currently this functions fills at most `mFormatID`, `mFormatFlags`, and `mBitsPerChannel`
void FillASBDWithSndfileFormat(AudioStreamBasicDescription * _Nonnull asbd, int format);
