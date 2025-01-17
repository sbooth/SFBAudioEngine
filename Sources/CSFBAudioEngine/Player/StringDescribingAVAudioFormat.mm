//
// Copyright (c) 2024-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBCAChannelLayout.hpp>
#import <SFBCAStreamBasicDescription.hpp>

#import "StringDescribingAVAudioFormat.h"

namespace SFB {

NSString * StringDescribingAVAudioFormat(AVAudioFormat *format, bool includeChannelLayout) noexcept
{
	if(!format)
		return nullptr;

	SFB::CAStreamBasicDescription asbd{*(format.streamDescription)};
	NSString *formatDescription = asbd.FormatDescription();

	if(includeChannelLayout) {
		NSString *layoutDescription = SFB::AudioChannelLayoutDescription(format.channelLayout.layout);
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@ [%@]>", format, formatDescription, layoutDescription ?: @"no channel layout"];
	}
	else
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@>", format, formatDescription];
}

} /* namespace SFB */
