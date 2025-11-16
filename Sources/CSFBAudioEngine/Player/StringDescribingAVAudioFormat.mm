//
// Copyright (c) 2024-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <CAChannelLayout.hpp>
#import <CAStreamDescription.hpp>

#import "StringDescribingAVAudioFormat.h"

namespace SFB {

NSString * StringDescribingAVAudioFormat(AVAudioFormat *format, bool includeChannelLayout) noexcept
{
	if(!format)
		return nullptr;

	NSString *formatDescription = CXXCoreAudio::AudioStreamBasicDescriptionFormatDescription(*format.streamDescription);
	if(includeChannelLayout) {
		NSString *layoutDescription = CXXCoreAudio::AudioChannelLayoutDescription(format.channelLayout.layout);
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@ [%@]>", format, formatDescription, layoutDescription ?: @"no channel layout"];
	}
	else
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@>", format, formatDescription];
}

} /* namespace SFB */
