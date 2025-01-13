//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <AudioToolbox/AudioFormat.h>
#import <AVFAudio/AVFAudio.h>

namespace SFB {

/// Returns `true` if `lhs` and `rhs` are equivalent
///
/// Channel layouts are considered equivalent if:
/// 1) Both channel layouts are `nil`
/// 2) One channel layout is `nil` and the other has a mono or stereo layout tag
/// 3) `kAudioFormatProperty_AreChannelLayoutsEquivalent` is true
inline bool AVAudioChannelLayoutsAreEquivalent(AVAudioChannelLayout * _Nullable lhs, AVAudioChannelLayout * _Nullable rhs) noexcept
{
	if(!lhs && !rhs)
		return true;
	else if(lhs && !rhs) {
		const auto layoutTag = lhs.layoutTag;
		if(layoutTag == kAudioChannelLayoutTag_Mono || layoutTag == kAudioChannelLayoutTag_Stereo)
			return true;
	}
	else if(!lhs && rhs) {
		const auto layoutTag = rhs.layoutTag;
		if(layoutTag == kAudioChannelLayoutTag_Mono || layoutTag == kAudioChannelLayoutTag_Stereo)
			return true;
	}

	if(!lhs || !rhs)
		return false;

	const AudioChannelLayout *layouts [] = {
		lhs.layout,
		rhs.layout
	};

	UInt32 layoutsEqual = 0;
	UInt32 propertySize = sizeof(layoutsEqual);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), static_cast<const void *>(layouts), &propertySize, &layoutsEqual);
	if(noErr != result)
		return false;

	return layoutsEqual;
}

} /* namespace SFB */
