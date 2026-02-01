//
// SPDX-FileCopyrightText: 2024 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import <CoreAudioTypes/CoreAudioTypes.h>

/// Fills an `AudioStreamBasicDescription` with details from a sndfile format
/// - note: Currently this functions fills at most `mFormatID`, `mFormatFlags`, and `mBitsPerChannel`
void FillASBDWithSndfileFormat(AudioStreamBasicDescription *_Nonnull asbd, int format);
