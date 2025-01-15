//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioPlayerNode.h"

#import "AudioPlayerNode.h"

@interface SFBAudioPlayerNode ()
{
@package
	SFB::AudioPlayerNode::unique_ptr _impl;
}
@end
