/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioChannelLayout+SFBChannelLabels.h"

static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions);
static AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions);

static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
{
	return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
}

static AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions)
{
	size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	if(!channelLayout)
		return NULL;

	memset(channelLayout, 0, layoutSize);

	return channelLayout;
}

@implementation AVAudioChannelLayout (SFBChannelLabels)

+ (instancetype)layoutWithChannelLabels:(AVAudioChannelCount)count, ...
{
	va_list ap;
	va_start(ap, count);

	AVAudioChannelLayout *layout = [[AVAudioChannelLayout alloc] initWithChannelLabels:count ap:ap];

	va_end(ap);

	return layout;
}

+ (instancetype)layoutWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count
{
	return [[AVAudioChannelLayout alloc] initWithChannelLabels:channelLabels count:count];
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)count, ...
{
	va_list ap;
	va_start(ap, count);

	self = [self initWithChannelLabels:count ap:ap];

	va_end(ap);

	return self;
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)count ap:(va_list)ap
{
	NSParameterAssert(count > 0);
	NSParameterAssert(ap != NULL);

	AudioChannelLayout *channelLayout = CreateChannelLayout(count);
	if(!channelLayout)
		return nil;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;

	for(AVAudioChannelCount i = 0; i < count; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = va_arg(ap, AudioChannelLabel);

	self = [self initWithLayout:channelLayout];
	free(channelLayout);
	return self;
}

- (instancetype)initWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count
{
	NSParameterAssert(channelLabels != NULL);
	NSParameterAssert(count > 0);

	AudioChannelLayout *channelLayout = CreateChannelLayout(count);
	if(!channelLayout)
		return nil;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;

	for(AVAudioChannelCount i = 0; i < count; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = channelLabels[i];

	self = [self initWithLayout:channelLayout];
	free(channelLayout);
	return self;
}

@end
