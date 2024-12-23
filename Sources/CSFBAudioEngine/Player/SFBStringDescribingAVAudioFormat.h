//
// Copyright (c) 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBCAChannelLayout.hpp>
#import <SFBCAStreamBasicDescription.hpp>

namespace SFB {

inline NSString * _Nullable StringDescribingAVAudioFormat(AVAudioFormat * _Nullable format, bool includeChannelLayout = true) noexcept
{
	if(!format)
		return nullptr;

	SFB::CAStreamBasicDescription asbd{*(format.streamDescription)};
	NSString *formatDescription = asbd.FormatDescription();

	if(includeChannelLayout) {
		NSString *layoutDescription = (__bridge_transfer NSString *)SFB::CopyAudioChannelLayoutDescription(format.channelLayout.layout);
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@ [%@]>", format, formatDescription, layoutDescription ?: @"no channel layout"];
	}
	else
		return [NSString stringWithFormat:@"<AVAudioFormat %p: %@>", format, formatDescription];
}

} // namespace SFB
