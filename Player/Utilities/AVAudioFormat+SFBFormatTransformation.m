/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioFormat+SFBFormatTransformation.h"

@implementation AVAudioFormat (SFBFormatTransformation)

- (nullable AVAudioFormat *)nonInterleavedEquivalent
{
	AudioStreamBasicDescription asbd = *(self.streamDescription);
	if(asbd.mFormatID != kAudioFormatLinearPCM)
		return nil;

	if(asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved)
		return self;

	asbd.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;

	asbd.mBytesPerPacket /= asbd.mChannelsPerFrame;
	asbd.mBytesPerFrame /= asbd.mChannelsPerFrame;

	return [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:self.channelLayout];
}

- (nullable AVAudioFormat *)interleavedEquivalent
{
	AudioStreamBasicDescription asbd = *(self.streamDescription);
	if(asbd.mFormatID != kAudioFormatLinearPCM)
		return nil;

	if(!(asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved))
		return self;

	asbd.mFormatFlags &= ~kAudioFormatFlagIsNonInterleaved;

	asbd.mBytesPerPacket *= asbd.mChannelsPerFrame;
	asbd.mBytesPerFrame *= asbd.mChannelsPerFrame;

	return [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:self.channelLayout];
}

@end

