/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioChannelLayout+SFBChannelLabels.h"

static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions);
static AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions);
static AudioChannelLayout * CreateChannelLayoutWithLabels(UInt32 numberChannelLabels, va_list ap);

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

static AudioChannelLayout * CreateChannelLayoutWithLabels(UInt32 numberChannelLabels, va_list ap)
{
	assert(numberChannelLabels > 0);

	AudioChannelLayout *channelLayout = CreateChannelLayout(numberChannelLabels);
	if(!channelLayout)
		return NULL;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;

	for(UInt32 i = 0; i < numberChannelLabels; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = va_arg(ap, AudioChannelLabel);

	return channelLayout;
}

@implementation AVAudioChannelLayout (SFBChannelLabels)

+ (instancetype)layoutWithChannelLabels:(AVAudioChannelCount)numberChannelLabels, ...
{
	va_list ap;
	va_start(ap, numberChannelLabels);

	AVAudioChannelLayout *layout = [[AVAudioChannelLayout alloc] initWithChannelLabels:numberChannelLabels ap:ap];

	va_end(ap);

	return layout;
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)numberChannelLabels, ...
{
	va_list ap;
	va_start(ap, numberChannelLabels);

	self = [self initWithChannelLabels:numberChannelLabels ap:ap];

	va_end(ap);

	return self;
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)numberChannelLabels ap:(va_list)ap
{
	NSParameterAssert(numberChannelLabels > 0);
	NSParameterAssert(ap != NULL);

	AudioChannelLayout *layout = CreateChannelLayoutWithLabels(numberChannelLabels, ap);
	if(!layout)
		return nil;

	self = [self initWithLayout:layout];
	free(layout);
	return self;
}

@end
