//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <AudioToolbox/AudioFormat.h>

#import "AVAudioChannelLayoutsAreEquivalent.h"

namespace SFB {

bool AVAudioChannelLayoutsAreEquivalent(AVAudioChannelLayout *lhs, AVAudioChannelLayout *rhs) noexcept
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
